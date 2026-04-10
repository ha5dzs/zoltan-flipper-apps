#include "pocsag.h"

#include <lib/toolbox/level_duration.h>
#include <stdlib.h>
#include <string.h>
#include <furi.h>
#include <flipper_format/flipper_format_i.h>

// Baud rate configuration - must match POCSAG_BAUD_RATE in pocsag_pager_app_i.c
#ifndef POCSAG_BAUD_RATE
#define POCSAG_BAUD_RATE 512
#endif

#if POCSAG_BAUD_RATE == 512
#define POCSAG_TE_BASE 1953  // 512 baud: 1953.125us per bit
#elif POCSAG_BAUD_RATE == 1200
#define POCSAG_TE_BASE 833   // 1200 baud: 833.333us per bit
#elif POCSAG_BAUD_RATE == 2400
#define POCSAG_TE_BASE 417   // 2400 baud: 416.667us per bit
#else
#error "Invalid POCSAG_BAUD_RATE - must be 512, 1200, or 2400"
#endif

#define TAG "POCSAGEncoder"

// Debug macro that always prints - DEBUG ONLY (commented out by default)
// The #define DEBUG_LOG line is active - use for debugging TX generation
// Uncomment the line below to disable all debug logging:
// #define DEBUG_LOG(...) do {} while(0)  // Uncomment to disable debug
#define DEBUG_LOG(...) FURI_LOG_E(TAG, __VA_ARGS__)  // DEBUG MODE: Debug logging enabled

// POCSAG constants
#define POCSAG_PREAMBLE_BITS 576
#define POCSAG_SYNC_WORD 0x7CD215D8
#define POCSAG_IDLE_CODE_WORD 0x7A89C197
#define POCSAG_CW_BITS 32

// BCH(31,21) polynomial: x^10 + x^9 + x^8 + x^6 + x^5 + x^3 + 1
#define BCH_POLY 0x769

// POCSAG Encoder instance
struct SubGhzProtocolEncoderPOCSAG {
    SubGhzProtocolEncoderBase base;

    // POCSAG data
    uint32_t ric;
    char message[256];
    size_t msg_len;

    // Transmission data
    LevelDuration* upload;
    size_t upload_size;
    size_t upload_idx;
    uint32_t repeat;
    bool is_running;
};

typedef struct SubGhzProtocolEncoderPOCSAG SubGhzProtocolEncoderPOCSAG;

// Generate BCH(31,21) error correction code
// Input: 21 bits of data (bits 0-20)
// Output: 10-bit BCH code (bits 0-9)
static uint32_t pocsag_bch_encode_31_21(uint32_t data) {
    uint32_t reg = 0;

    // Shift data bits into register, computing remainder
    for(int i = 0; i < 21; i++) {
        bool bit = (data >> i) & 1;  // LSB first
        bool reg_msb = (reg >> 10) & 1;
        reg = (reg << 1) | bit;
        if(reg_msb) {
            reg ^= BCH_POLY;
        }
    }

    // Continue shifting to clear the register
    for(int i = 0; i < 10; i++) {
        bool reg_msb = (reg >> 10) & 1;
        reg = (reg << 1);
        if(reg_msb) {
            reg ^= BCH_POLY;
        }
    }

    return reg & 0x3FF; // Return 10-bit BCH code
}

// Calculate even parity bit
static uint32_t pocsag_even_parity(uint32_t data) {
    uint32_t count = 0;
    for(int i = 0; i < 32; i++) {
        if(data & (1 << i)) count++;
    }
    return count & 1; // Return 1 if odd, 0 if even
}

// Create an address codeword
// Format: [0][RIC(18 bits)][Func(2 bits)][BCH(10 bits)][Parity]
static uint32_t pocsag_encode_address_codeword(uint32_t ric, uint8_t func) {
    uint32_t codeword = 0;

    // Bit 31 = 0 for address codeword ( transmitted first in the 32 bits )
    // Bits 30-13: 18-bit address payload (RIC / 8)
    // Bits 12-11: 2-bit function code
    // Bits 10-1: 10-bit BCH
    // Bit 0: parity

    uint32_t address_payload = ric >> 3;  // RIC / 8

    // Build the codeword with address payload and function
    codeword = (address_payload & 0x3FFFF) << 13;  // Address at bits 30-13
    codeword |= (func & 0x3) << 11;                 // Function at bits 12-11

    // BCH(31,21) encodes 21 bits of data to produce 10 bits of ECC
    // The 21 data bits are: bit 20 = 0 (address marker), bits 19-2 = address payload, bits 1-0 = function
    // This corresponds to bits 30-11 of the codeword (after shifting)
    uint32_t bch = pocsag_bch_encode_31_21(codeword >> 11);
    codeword |= (bch & 0x3FF) << 1;

    // Bit 0: even parity
    codeword |= pocsag_even_parity(codeword);

    return codeword;
}

// Encode alphanumeric message into message codewords and populate array
// The decoder reads payload MSB-first from codeword bits 30-11 (payload bits 19-0).
// We need to pack characters MSB-first into the payload.
// Returns number of codewords generated
static size_t pocsag_encode_message_codewords(
    const char* message,
    size_t msg_len,
    uint32_t* codewords,
    size_t max_codewords) {
    size_t cw_idx = 0;
    uint32_t pending_bits = 0;
    uint8_t pending_count = 0;

    // Pack characters in reverse order with bits reversed
    // So first char's MSB ends up at payload MSB (bit 19)
    for(size_t i = msg_len; i > 0; i--) {
        size_t idx = i - 1;
        uint8_t ch = message[idx] & 0x7F;
        // Reverse bits: bit 0 -> bit 6, bit 1 -> bit 5, etc.
        // This way, when packed LSB-first, MSB ends up at MSB of payload
        uint8_t ch_bits = 0;
        for(uint8_t j = 0; j < 7; j++) {
            if(ch & (1 << j)) {
                ch_bits |= (1 << (6 - j));
            }
        }

        // Pack: add character bits at current LSB position
        pending_bits |= ((uint32_t)ch_bits << pending_count);
        pending_count += 7;

        // Extract as many 20-bit codewords as possible
        while(pending_count >= 20 && cw_idx < max_codewords) {
            // Extract the bottom 20 bits (LSB-first)
            uint32_t payload = pending_bits & 0xFFFFF;
            uint32_t codeword = 0;

            // Bit 31 = 1 for message codeword
            codeword = 1 << 31;

            // Bits 11-30: 20-bit message payload
            // The decoder reads MSB-first from payload, so we need to reverse the payload bits
            uint32_t reversed_payload = 0;
            for(uint8_t j = 0; j < 20; j++) {
                if(payload & (1 << j)) {
                    reversed_payload |= (1 << (19 - j));
                }
            }
            codeword |= (reversed_payload & 0xFFFFF) << 11;

            // Debug: log payload bits
            FURI_LOG_E(TAG, "Char %zu: payload=0x%05lX, reversed_payload=0x%05lX, codeword=0x%08lX",
                       cw_idx, payload, reversed_payload, codeword);

            // Bits 1-10: BCH(31,21) error correction
            uint32_t bch = pocsag_bch_encode_31_21(codeword >> 11);
            codeword |= (bch & 0x3FF) << 1;

            // Bit 0: even parity
            codeword |= pocsag_even_parity(codeword);

            codewords[cw_idx++] = codeword;
            pending_bits >>= 20;
            pending_count -= 20;
        }
    }

    // Handle remaining bits - pad with zeros and create final codeword
    if(pending_count > 0 && cw_idx < max_codewords) {
        uint32_t payload = pending_bits & 0xFFFFF;
        uint32_t codeword = 0;

        // Reverse payload bits for MSB-first decoder
        uint32_t reversed_payload = 0;
        for(uint8_t j = 0; j < pending_count; j++) {
            if(payload & (1 << j)) {
                reversed_payload |= (1 << (19 - j));
            }
        }

        codeword = 1 << 31;
        codeword |= (reversed_payload & 0xFFFFF) << 11;
        FURI_LOG_E(TAG, "Final: payload=0x%05lX, reversed_payload=0x%05lX, codeword=0x%08lX",
                   payload, reversed_payload, codeword);
        uint32_t bch = pocsag_bch_encode_31_21(codeword >> 11);
        codeword |= (bch & 0x3FF) << 1;
        codeword |= pocsag_even_parity(codeword);

        codewords[cw_idx++] = codeword;
    }

    return cw_idx;
}

// Allocate encoder
void* subghz_protocol_encoder_pocsag_alloc(SubGhzEnvironment* environment) {
    UNUSED(environment);
    SubGhzProtocolEncoderPOCSAG* instance = malloc(sizeof(SubGhzProtocolEncoderPOCSAG));
    instance->base.protocol = &subghz_protocol_pocsag;
    instance->repeat = 10; // Default repeat count
    instance->upload = NULL;
    instance->upload_size = 0;
    instance->is_running = false;
    memset(instance->message, 0, sizeof(instance->message));
    instance->ric = 0;
    instance->msg_len = 0;
    return instance;
}

// Free encoder
void subghz_protocol_encoder_pocsag_free(void* context) {
    furi_assert(context);
    SubGhzProtocolEncoderPOCSAG* instance = context;
    if(instance->upload) {
        free(instance->upload);
        instance->upload = NULL;
    }
    free(instance);
}

// Generate upload data for POCSAG transmission with bit timing correction
// Returns true on success
static bool pocsag_generate_upload(SubGhzProtocolEncoderPOCSAG* instance) {
    furi_assert(instance);

    // Calculate upload size (NRZ encoding: 1 transition per bit)
    // Preamble: 576 bits * 1 = 576
    // Sync: 32 bits * 1 = 32
    // 16 codewords * 32 bits * 1 = 512 per batch
    // 2 batches = 1024
    // Padding: ~100
    // Total: ~1750, round up to 2000
    size_t upload_size = 2000;

    if(instance->upload && instance->upload_size < upload_size) {
        free(instance->upload);
        instance->upload = NULL;
    }

    if(!instance->upload) {
        instance->upload = malloc(upload_size * sizeof(LevelDuration));
    }

    instance->upload_size = upload_size;
    instance->upload_idx = 0;

    size_t idx = 0;
    uint32_t te = POCSAG_TE_BASE;  // Base duration based on baud rate

    // Helper function to add a codeword with proper bit timing
    #define ADD_CODEWORD(cw) do { \
        for(int cw_bit = 31; cw_bit >= 0; cw_bit--) { \
            bool bit = ((cw) >> cw_bit) & 1; \
            instance->upload[idx++] = level_duration_make(!bit, te); \
        } \
    } while(0)

    // POCSAG preamble: 576 bits of alternating 1/0
    // NRZ: 1 = HIGH for entire bit, 0 = LOW for entire bit
    // Note: FSK inverts the signal, and decoder also inverts internally.
    // So we need to invert for correct FSK modulation.
    for(size_t i = 0; i < POCSAG_PREAMBLE_BITS; i++) {
        bool bit = (i % 2) == 0; // 1, 0, 1, 0, ...
        instance->upload[idx++] = level_duration_make(!bit, te);
    }

    // Calculate message codewords needed
    size_t num_msg_cw = (instance->msg_len * 7 + 19) / 20; // Ceiling

    // Target frame for address
    uint32_t target_frame = instance->ric % 8;

    // First batch: Sync + 8 frames (16 codewords)
    // Sync word (NRZ: bit 1 = HIGH for full bit, bit 0 = LOW for full bit)
    // Note: FSK inverts the signal, and decoder also inverts internally.
    // So we need to invert for correct FSK modulation.
    uint32_t sync_cw = POCSAG_SYNC_WORD;
    ADD_CODEWORD(sync_cw);

    // Frames 0-7
    uint32_t* msg_cws = malloc(num_msg_cw > 0 ? num_msg_cw * sizeof(uint32_t) : 1);
    if(num_msg_cw > 0) {
        memset(msg_cws, 0, num_msg_cw * sizeof(uint32_t));
    }
    size_t msg_cw_idx = 0;

    // Pre-populate message codewords
    if(num_msg_cw > 0) {
        msg_cw_idx = pocsag_encode_message_codewords(
            instance->message, instance->msg_len, msg_cws, num_msg_cw);
        FURI_LOG_E(TAG, "Generated %zu message codewords", msg_cw_idx);
        for(size_t i = 0; i < msg_cw_idx; i++) {
            FURI_LOG_E(TAG, "Msg CW[%zu] = 0x%08lX", i, msg_cws[i]);
        }
    }
    msg_cw_idx = 0; // Reset index for use below

    for(uint32_t f = 0; f < 8; f++) {
        // Frame A (first codeword)
        if(f < target_frame) {
            // Idle codeword (NRZ encoding)
            // Note: FSK inverts the signal, and decoder also inverts internally.
            // So we need to invert for correct FSK modulation.
            ADD_CODEWORD(POCSAG_IDLE_CODE_WORD);
            // Frame B (second codeword) - also idle for frames before target
            ADD_CODEWORD(POCSAG_IDLE_CODE_WORD);
        } else if(f == target_frame) {
            // Address codeword (NRZ encoding)
            // Note: FSK inverts the signal, and decoder also inverts internally.
            // So we need to invert for correct FSK modulation.
            ADD_CODEWORD(pocsag_encode_address_codeword(instance->ric, 3)); // 3 = alphanumeric
            if(msg_cw_idx < num_msg_cw) {
                ADD_CODEWORD(msg_cws[msg_cw_idx++]);
            } else {
                ADD_CODEWORD(POCSAG_IDLE_CODE_WORD);
            }
        } else {
            // First message codeword (if available) - NRZ encoding
            // Note: FSK inverts the signal, and decoder also inverts internally.
            // So we need to invert for correct FSK modulation.
            if(msg_cw_idx < num_msg_cw) {
                ADD_CODEWORD(msg_cws[msg_cw_idx++]);
            } else {
                ADD_CODEWORD(POCSAG_IDLE_CODE_WORD);
            }
            // Frame B (second codeword) - NRZ encoding
            // Note: FSK inverts the signal, and decoder also inverts internally.
            // So we need to invert for correct FSK modulation.
            if(msg_cw_idx < num_msg_cw) {
                ADD_CODEWORD(msg_cws[msg_cw_idx++]);
            } else {
                ADD_CODEWORD(POCSAG_IDLE_CODE_WORD);
            }
        }
    }

    // Second batch: start with sync (NRZ encoding)
    // Note: FSK inverts the signal, and decoder also inverts internally.
    // So we need to invert for correct FSK modulation.
    ADD_CODEWORD(POCSAG_SYNC_WORD);

    // Fill with remaining message codewords or idle - NRZ encoding
    // Note: FSK inverts the signal, and decoder also inverts internally.
    // So we need to invert for correct FSK modulation.
    for(uint32_t f = 0; f < 8 && msg_cw_idx < num_msg_cw; f++) {
        for(int cw_idx = 0; cw_idx < 2 && msg_cw_idx < num_msg_cw; cw_idx++) {
            ADD_CODEWORD(msg_cws[msg_cw_idx++]);
        }
    }

    // Pad with idle codewords until EOM - NRZ encoding
    // Note: FSK inverts the signal, and decoder also inverts internally.
    // So we need to invert for correct FSK modulation.
    for(int i = 0; i < 32; i++) {
        ADD_CODEWORD(POCSAG_IDLE_CODE_WORD);
    }

    free(msg_cws);
    instance->upload_size = idx;
    FURI_LOG_E(TAG, "Upload generated: %zu transitions", idx);
    return idx > 0;
}

// Deserialize encoder from FlipperFormat
SubGhzProtocolStatus subghz_protocol_encoder_pocsag_deserialize(
    void* context,
    FlipperFormat* flipper_format) {
    furi_assert(context);
    SubGhzProtocolEncoderPOCSAG* instance = context;
    SubGhzProtocolStatus ret = SubGhzProtocolStatusError;

    FURI_LOG_E(TAG, "Starting deserialize");

    do {
        // Read RIC
        uint32_t ric = 0;
        if(!flipper_format_read_uint32(flipper_format, "RIC", &ric, 1)) {
            FURI_LOG_E(TAG, "Missing RIC");
            break;
        }
        instance->ric = ric;
        FURI_LOG_E(TAG, "RIC=%lu", ric);

        // Read message
        FuriString* msg = furi_string_alloc();
        if(!flipper_format_read_string(flipper_format, "Message", msg)) {
            FURI_LOG_E(TAG, "Missing Message");
            furi_string_free(msg);
            break;
        }
        strncpy(instance->message, furi_string_get_cstr(msg), sizeof(instance->message) - 1);
        instance->msg_len = strlen(instance->message);
        furi_string_free(msg);
        FURI_LOG_E(TAG, "Message='%s', len=%zu", instance->message, instance->msg_len);

        // Read repeat count
        uint32_t repeat = 10;
        flipper_format_read_uint32(flipper_format, "Repeat", &repeat, 1);
        instance->repeat = repeat;

        FURI_LOG_E(TAG, "About to generate upload");
        // Generate upload data
        if(!pocsag_generate_upload(instance)) {
            FURI_LOG_E(TAG, "Failed to generate upload");
            break;
        }
        FURI_LOG_E(TAG, "Upload generated, size=%zu", instance->upload_size);

        instance->is_running = true;
        ret = SubGhzProtocolStatusOk;
    } while(false);

    FURI_LOG_E(TAG, "Deserialize done, ret=%d", ret);
    return ret;
}

// Stop encoder
void subghz_protocol_encoder_pocsag_stop(void* context) {
    furi_assert(context);
    SubGhzProtocolEncoderPOCSAG* instance = context;
    instance->is_running = false;
}

// Yield next level duration
LevelDuration subghz_protocol_encoder_pocsag_yield(void* context) {
    furi_assert(context);
    SubGhzProtocolEncoderPOCSAG* instance = context;

    if(!instance->is_running || instance->upload_idx >= instance->upload_size) {
        instance->is_running = false;
        return level_duration_reset();
    }

    LevelDuration ret = instance->upload[instance->upload_idx++];

    // Handle repeat - reset when done
    if(instance->upload_idx >= instance->upload_size) {
        if(instance->repeat > 0) {
            instance->repeat--;
            instance->upload_idx = 0;
        } else {
            instance->is_running = false;
        }
    }

    return ret;
}

// Serialize encoder to FlipperFormat
SubGhzProtocolStatus subghz_protocol_encoder_pocsag_serialize(
    void* context,
    FlipperFormat* flipper_format,
    SubGhzRadioPreset* preset) {
    furi_assert(context);
    SubGhzProtocolEncoderPOCSAG* instance = context;
    SubGhzProtocolStatus ret = SubGhzProtocolStatusError;
    FuriString* preset_str = furi_string_alloc();

    do {
        // Write header
        if(!flipper_format_write_header_cstr(
               flipper_format, "Flipper POCSAG Pager Key File", 1)) {
            FURI_LOG_E(TAG, "Failed to write header");
            break;
        }

        // Write frequency
        if(!flipper_format_write_uint32(flipper_format, "Frequency", &preset->frequency, 1)) {
            FURI_LOG_E(TAG, "Failed to write frequency");
            break;
        }

        // Write preset
        const char* preset_name = furi_string_get_cstr(preset->name);
        if(!strcmp(preset_name, "AM270")) {
            furi_string_set(preset_str, "FuriHalSubGhzPresetOok270Async");
        } else if(!strcmp(preset_name, "AM650")) {
            furi_string_set(preset_str, "FuriHalSubGhzPresetOok650Async");
        } else if(!strcmp(preset_name, "FM238")) {
            furi_string_set(preset_str, "FuriHalSubGhzPreset2FSKDev238Async");
        } else if(!strcmp(preset_name, "FM12K")) {
            furi_string_set(preset_str, "FuriHalSubGhzPreset2FSKDev12KAsync");
        } else if(!strcmp(preset_name, "FM476")) {
            furi_string_set(preset_str, "FuriHalSubGhzPreset2FSKDev476Async");
        } else {
            furi_string_set(preset_str, "FuriHalSubGhzPresetCustom");
        }
        if(!flipper_format_write_string(
               flipper_format, "Preset", preset_str)) {
            FURI_LOG_E(TAG, "Failed to write preset");
            break;
        }

        // Write protocol
        if(!flipper_format_write_string_cstr(flipper_format, "Protocol", "POCSAG")) {
            FURI_LOG_E(TAG, "Failed to write protocol");
            break;
        }

        // Write RIC
        if(!flipper_format_write_uint32(flipper_format, "RIC", &instance->ric, 1)) {
            FURI_LOG_E(TAG, "Failed to write RIC");
            break;
        }

        // Write message
        if(!flipper_format_write_string_cstr(flipper_format, "Message", instance->message)) {
            FURI_LOG_E(TAG, "Failed to write message");
            break;
        }

        // Write repeat count
        if(!flipper_format_write_uint32(flipper_format, "Repeat", &instance->repeat, 1)) {
            FURI_LOG_E(TAG, "Failed to write Repeat");
            break;
        }

        // Write TE (symbol duration) - use configured baud rate
        uint32_t te = POCSAG_TE_BASE;
        if(!flipper_format_write_uint32(flipper_format, "TE", &te, 1)) {
            FURI_LOG_E(TAG, "Failed to write TE");
            break;
        }

        ret = SubGhzProtocolStatusOk;
    } while(false);

    furi_string_free(preset_str);
    return ret;
}

const SubGhzProtocolEncoder subghz_protocol_pocsag_encoder = {
    .alloc = subghz_protocol_encoder_pocsag_alloc,
    .free = subghz_protocol_encoder_pocsag_free,
    .deserialize = subghz_protocol_encoder_pocsag_deserialize,
    .stop = subghz_protocol_encoder_pocsag_stop,
    .yield = subghz_protocol_encoder_pocsag_yield,
};

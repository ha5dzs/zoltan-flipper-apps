#include "pocsag.h"

#include <inttypes.h>
#include <lib/flipper_format/flipper_format_i.h>
#include <furi/core/string.h>
#include <furi_hal.h>
#include "../protocols/pcsg_generic.h"

#define TAG "POCSAG"

// 1200 baud POCSAG timing:
// Bit time = 1/1200 Hz = 833.333... microseconds
// Using integer 833 causes drift (~1 bit error per 2500 bits)
// We use a counter-based approach where every 3rd bit gets +1us correction
static const SubGhzBlockConst pocsag_const = {
    .te_short = 833,  // Base duration; corrected by counter in decoder
    .te_delta = 100,
};
static const SubGhzBlockConst pocsag512_const = {
    .te_short = 1950,
    .te_long = 1950,
    .te_delta = 120,
};
static const SubGhzBlockConst pocsag2400_const = {
    .te_short = 410,
    .te_long = 410,
    .te_delta = 60,
};

// Minimal amount of sync bits (interleaving zeros and ones)
#define POCSAG_MIN_SYNC_BITS 24
#define POCSAG_CW_BITS 32
#define POCSAG_CW_MASK 0xFFFFFFFF
#define POCSAG_FRAME_SYNC_CODE 0x7CD215D8
#define POCSAG_IDLE_CODE_WORD 0x7A89C197

#define POCSAG_FUNC_NUM 0
#define POCSAG_FUNC_ALERT1 1
#define POCSAG_FUNC_ALERT2 2
#define POCSAG_FUNC_ALPHANUM 3

static const char* func_msg[] = {"Num: ", "Alert", "Alert: ", "Msg: "};
static const char* bcd_chars = "*U -)(";

struct SubGhzProtocolDecoderPocsag {
    SubGhzProtocolDecoderBase base;

    SubGhzBlockDecoder decoder;
    PCSGBlockGeneric generic;

    uint8_t codeword_idx;
    uint32_t ric;
    uint8_t func;

    // partially decoded character
    uint8_t char_bits;
    uint8_t char_data;

    // message being decoded
    FuriString* msg;

    // Done messages, ready to be serialized/deserialized
    FuriString* done_msg;

    SubGhzBlockConst* pocsag_timing;
    uint32_t version;
};

typedef struct SubGhzProtocolDecoderPocsag SubGhzProtocolDecoderPocsag;

typedef enum {
    PocsagDecoderStepReset = 0,
    PocsagDecoderStepFoundSync,
    PocsagDecoderStepFoundPreamble,
    PocsagDecoderStepMessage,
} PocsagDecoderStep;

void* subghz_protocol_decoder_pocsag_alloc(SubGhzEnvironment* environment) {
    UNUSED(environment);

    SubGhzProtocolDecoderPocsag* instance = malloc(sizeof(SubGhzProtocolDecoderPocsag));
    instance->base.protocol = &subghz_protocol_pocsag;
    instance->generic.protocol_name = instance->base.protocol->name;
    instance->msg = furi_string_alloc();
    instance->done_msg = furi_string_alloc();
    instance->pocsag_timing = NULL; //not synced yet
    if(instance->generic.result_msg == NULL) {
        instance->generic.result_msg = furi_string_alloc();
    }
    if(instance->generic.result_ric == NULL) {
        instance->generic.result_ric = furi_string_alloc();
    }

    return instance;
}

void subghz_protocol_decoder_pocsag_free(void* context) {
    furi_assert(context);
    SubGhzProtocolDecoderPocsag* instance = context;
    furi_string_free(instance->msg);
    furi_string_free(instance->done_msg);
    if(instance->generic.result_msg != NULL) {
        furi_string_free(instance->generic.result_msg);
    }
    if(instance->generic.result_ric != NULL) {
        furi_string_free(instance->generic.result_ric);
    }
    free(instance);
}

void subghz_protocol_decoder_pocsag_reset(void* context) {
    furi_assert(context);
    SubGhzProtocolDecoderPocsag* instance = context;

    instance->decoder.parser_step = PocsagDecoderStepReset;
    instance->decoder.decode_data = 0UL;
    instance->decoder.decode_count_bit = 0;
    instance->codeword_idx = 0;
    instance->char_bits = 0;
    instance->char_data = 0;
    furi_string_reset(instance->msg);
    furi_string_reset(instance->done_msg);
    furi_string_reset(instance->generic.result_msg);
    furi_string_reset(instance->generic.result_ric);
    instance->generic.timestamp = 0;
}

static void pocsag_decode_address_word(SubGhzProtocolDecoderPocsag* instance, uint32_t data) {
    // Start new message - clear the done_msg buffer
    furi_string_reset(instance->done_msg);

    // Extract address payload from bits 30-13 (18 bits)
    // The function code is at bits 11-12, BCH at 10-1, parity at 0
    // We need to clear the lower 13 bits to get just the address payload
    uint32_t address_payload = (data >> 13) & 0x3FFFF;  // Mask to 18 bits
    uint32_t frame_num = instance->codeword_idx >> 1;
    instance->ric = (address_payload << 3) | frame_num;
    instance->func = (data >> 11) & 0b11;

    FURI_LOG_D(TAG, "Address codeword: data=0x%08lX, payload=%lu, frame=%lu, RIC=%lu, func=%u",
               data, address_payload, frame_num, instance->ric, instance->func);
}

static bool decode_message_alphanumeric(SubGhzProtocolDecoderPocsag* instance, uint32_t data) {
    // The encoder processes the 20-bit payload (bits 11-30 of codeword) MSB-first.
    // Bit 30 of codeword = bit 19 of payload, bit 11 = bit 0 of payload.
    // The encoder packs characters LSB-first into payload, so we need to read MSB-first.
    for(uint8_t i = 0; i < 20; i++) {
        instance->char_data >>= 1;
        // Extract bit from payload (MSB first, starting from bit 30)
        if(data & (1 << 30)) {
            instance->char_data |= 1 << 6;
        }
        instance->char_bits++;
        if(instance->char_bits == 7) {
            if(instance->char_data == 0) {
                FURI_LOG_D(TAG, "Zero char detected, end of message");
                return false;
            }
            furi_string_push_back(instance->msg, instance->char_data);
            instance->char_data = 0;
            instance->char_bits = 0;
        }
        data <<= 1;  // Shift to bring next bit into position 30
    }
    return true;
}

static void decode_message_numeric(SubGhzProtocolDecoderPocsag* instance, uint32_t data) {
    // 5 groups with 4 bits each
    uint8_t val;
    for(uint8_t i = 0; i < 5; i++) {
        val = (data >> (27 - i * 4)) & 0b1111;
        // reverse the order of 4 bits
        val = (val & 0x5) << 1 | (val & 0xA) >> 1;
        val = (val & 0x3) << 2 | (val & 0xC) >> 2;

        if(val <= 9)
            val += '0';
        else
            val = bcd_chars[val - 10];
        furi_string_push_back(instance->msg, val);
    }
}

// decode message word, maintaining instance state for partial decoding. Return true if more data
// might follow or false if end of message reached.
static bool pocsag_decode_message_word(SubGhzProtocolDecoderPocsag* instance, uint32_t data) {
    switch(instance->func) {
    case POCSAG_FUNC_ALERT2:
    case POCSAG_FUNC_ALPHANUM:
        if(!decode_message_alphanumeric(instance, data)) {
            // End of message (zero char) - append remaining chars from msg to done_msg
            if(furi_string_size(instance->msg) > 0) {
                furi_string_cat(instance->done_msg, instance->msg);
            }
            return false;
        }
        // Append decoded characters to done_msg for accumulation across words
        if(furi_string_size(instance->msg) > 0) {
            furi_string_cat(instance->done_msg, instance->msg);
            furi_string_reset(instance->msg);
        }
        return true;

    case POCSAG_FUNC_NUM:
        decode_message_numeric(instance, data);
        // Append to done_msg after decoding the complete numeric word
        if(furi_string_size(instance->msg) > 0) {
            furi_string_cat(instance->done_msg, instance->msg);
            furi_string_cat_str(instance->done_msg, " ");
        }
        furi_string_reset(instance->msg);
        return true;
    }
    return false;
}

// Function called when current message got decoded, but other messages might follow
static void pocsag_message_done(SubGhzProtocolDecoderPocsag* instance) {
    // Check if we have a complete message to send
    size_t msg_len = furi_string_size(instance->done_msg);
    if(instance->base.callback && msg_len > 0) {
        // Capture timestamp from RTC (date/time)
        // Compact format in 32 bits:
        //   seconds/2 (5 bits): bits 0-4
        //   minutes (6 bits): bits 5-10
        //   hours (5 bits): bits 11-15
        //   day (5 bits): bits 16-20
        //   month (4 bits): bits 21-24
        //   year (7 bits): bits 25-31 (year % 100, 0-99)
        DateTime datetime = {0};
        furi_hal_rtc_get_datetime(&datetime);

        // year as last 2 digits (0-99 in 7 bits)
        uint8_t year_short = datetime.year % 100;
        instance->generic.timestamp = ((uint32_t)year_short << 25) |
           (uint32_t)datetime.month << 21 |
           (uint32_t)datetime.day << 16 |
           (uint32_t)datetime.hour << 11 |
           (uint32_t)datetime.minute << 5 |
           (datetime.second >> 1); // Store 5 bits (seconds / 2)

        // Clear result strings first
        furi_string_reset(instance->generic.result_ric);
        furi_string_reset(instance->generic.result_msg);

        // result_ric: RIC info with function indicator (plain text, no escape sequences)
        furi_string_printf(
            instance->generic.result_ric,
            "[P%lu]RIC: %" PRIu32 " | ",
            instance->version,
            instance->ric);
        furi_string_cat_str(instance->generic.result_ric, func_msg[instance->func]);

        // result_msg: just the message content (no prefix)
        furi_string_set_str(instance->generic.result_msg, furi_string_get_cstr(instance->done_msg));

        instance->base.callback(&instance->base, instance->base.context);

        // Clear result strings after callback for next message
        furi_string_reset(instance->generic.result_msg);
        furi_string_reset(instance->generic.result_ric);
        instance->generic.timestamp = 0;
    }

    // Clear done_msg AFTER callback to prevent corruption
    furi_string_reset(instance->done_msg);
    // reset the state for next message word
    instance->char_bits = 0;
    instance->char_data = 0;
    furi_string_reset(instance->msg);
}

void subghz_protocol_decoder_pocsag_feed(void* context, bool level, uint32_t duration) {
    furi_assert(context);
    SubGhzProtocolDecoderPocsag* instance = context;

    // reset state - waiting for 32 bits of interleaving 1s and 0s
    if(instance->decoder.parser_step == PocsagDecoderStepReset) {
        if(DURATION_DIFF(duration, pocsag_const.te_short) < pocsag_const.te_delta) {
            if(instance->pocsag_timing != &pocsag_const) {
                //timing changed, so reset before, and override
                subghz_protocol_decoder_pocsag_reset(context);
                instance->pocsag_timing = (SubGhzBlockConst*)&pocsag_const;
                instance->version = 1200;
                //FURI_LOG_I(TAG, "Detected POCSAG 1200 baud");
            }
            // POCSAG signals are inverted
            subghz_protocol_blocks_add_bit(&instance->decoder, !level);

            if(instance->decoder.decode_count_bit == POCSAG_MIN_SYNC_BITS) {
                instance->decoder.parser_step = PocsagDecoderStepFoundSync;
                FURI_LOG_D(TAG, "Found sync bits");
            }
        } else if(DURATION_DIFF(duration, pocsag512_const.te_short) < pocsag512_const.te_delta) {
            if(instance->pocsag_timing != &pocsag512_const) {
                //timing changed, so reset before, and override
                subghz_protocol_decoder_pocsag_reset(context);
                instance->pocsag_timing = (SubGhzBlockConst*)&pocsag512_const;
                instance->version = 512;
                //FURI_LOG_I(TAG, "Detected POCSAG 512 baud");
            }
            // POCSAG signals are inverted
            subghz_protocol_blocks_add_bit(&instance->decoder, !level);

            if(instance->decoder.decode_count_bit == POCSAG_MIN_SYNC_BITS) {
                instance->decoder.parser_step = PocsagDecoderStepFoundSync;
                FURI_LOG_D(TAG, "Found sync bits");
            }
        } else if(DURATION_DIFF(duration, pocsag2400_const.te_short) < pocsag2400_const.te_delta) {
            if(instance->pocsag_timing != &pocsag2400_const) {
                //timing changed, so reset before, and override
                subghz_protocol_decoder_pocsag_reset(context);
                instance->pocsag_timing = (SubGhzBlockConst*)&pocsag2400_const;
                instance->version = 2400;
                //FURI_LOG_I(TAG, "Detected POCSAG 2400 baud");
            }
            // POCSAG signals are inverted
            subghz_protocol_blocks_add_bit(&instance->decoder, !level);

            if(instance->decoder.decode_count_bit == POCSAG_MIN_SYNC_BITS) {
                instance->decoder.parser_step = PocsagDecoderStepFoundSync;
                FURI_LOG_D(TAG, "Found sync bits");
            }
        } else if(instance->decoder.decode_count_bit > 0) {
            subghz_protocol_decoder_pocsag_reset(context);
        }
        return;
    }

    int bits_count = duration / instance->pocsag_timing->te_short;
    uint32_t extra = duration - instance->pocsag_timing->te_short * bits_count;

    if(DURATION_DIFF(extra, instance->pocsag_timing->te_short) < instance->pocsag_timing->te_delta)
        bits_count++;
    else if(extra > instance->pocsag_timing->te_delta) {
        // in non-reset state we faced the error signal - we reached the end of the packet, flush data
        if(furi_string_size(instance->done_msg) > 0) {
            if(instance->base.callback)
                instance->base.callback(&instance->base, instance->base.context);
        }
        subghz_protocol_decoder_pocsag_reset(context);
        return;
    }

    uint32_t codeword;

    // handle state machine for every incoming bit
    while(bits_count-- > 0) {
        subghz_protocol_blocks_add_bit(&instance->decoder, !level);

        switch(instance->decoder.parser_step) {
        case PocsagDecoderStepFoundSync:
            if((instance->decoder.decode_data & POCSAG_CW_MASK) == POCSAG_FRAME_SYNC_CODE) {
                instance->decoder.parser_step = PocsagDecoderStepFoundPreamble;
                instance->decoder.decode_count_bit = 0;
                instance->decoder.decode_data = 0UL;
                FURI_LOG_I(TAG, "Found frame sync code 0x7CD215D8");
            }
            break;
        case PocsagDecoderStepFoundPreamble:
            // handle codewords
            if(instance->decoder.decode_count_bit == POCSAG_CW_BITS) {
                codeword = (uint32_t)(instance->decoder.decode_data & POCSAG_CW_MASK);
                FURI_LOG_D(TAG, "Codeword idx=%d, data=0x%08lX", instance->codeword_idx, codeword);
                switch(codeword) {
                case POCSAG_IDLE_CODE_WORD:
                    instance->codeword_idx++;
                    break;
                case POCSAG_FRAME_SYNC_CODE:
                    instance->codeword_idx = 0;
                    break;
                default:
                    // Here we expect only address messages
                    if(codeword >> 31 == 0) {
                        pocsag_decode_address_word(instance, codeword);
                        instance->decoder.parser_step = PocsagDecoderStepMessage;
                    }
                    instance->codeword_idx++;
                }
                instance->decoder.decode_count_bit = 0;
                instance->decoder.decode_data = 0UL;
            }
            break;

        case PocsagDecoderStepMessage:
            if(instance->decoder.decode_count_bit == POCSAG_CW_BITS) {
                codeword = (uint32_t)(instance->decoder.decode_data & POCSAG_CW_MASK);
                switch(codeword) {
                case POCSAG_IDLE_CODE_WORD:
                    // Idle during the message stops the message
                    instance->codeword_idx++;
                    instance->decoder.parser_step = PocsagDecoderStepFoundPreamble;
                    pocsag_message_done(instance);
                    break;
                case POCSAG_FRAME_SYNC_CODE:
                    instance->codeword_idx = 0;
                    break;
                default:
                    // In this state, both address and message words can arrive
                    if(codeword >> 31 == 0) {
                        pocsag_message_done(instance);
                        pocsag_decode_address_word(instance, codeword);
                    } else {
                        if(!pocsag_decode_message_word(instance, codeword)) {
                            instance->decoder.parser_step = PocsagDecoderStepFoundPreamble;
                            pocsag_message_done(instance);
                        }
                    }
                    instance->codeword_idx++;
                }
                instance->decoder.decode_count_bit = 0;
                instance->decoder.decode_data = 0UL;
            }
            break;
        }
    }
}

uint8_t subghz_protocol_decoder_pocsag_get_hash_data(void* context) {
    furi_assert(context);
    SubGhzProtocolDecoderPocsag* instance = context;
    uint8_t hash = 0;
    for(size_t i = 0; i < furi_string_size(instance->done_msg); i++)
        hash ^= furi_string_get_char(instance->done_msg, i);
    return hash;
}

bool subghz_protocol_decoder_pocsag_get_ric(void* context, uint32_t* ric) {
    furi_assert(context);
    furi_assert(ric);
    SubGhzProtocolDecoderPocsag* instance = context;
    *ric = instance->ric;
    return true;
}

SubGhzProtocolStatus subghz_protocol_decoder_pocsag_serialize(
    void* context,
    FlipperFormat* flipper_format,
    SubGhzRadioPreset* preset) {
    furi_assert(context);
    SubGhzProtocolDecoderPocsag* instance = context;
    uint32_t msg_len;

    if(SubGhzProtocolStatusOk !=
       pcsg_block_generic_serialize(&instance->generic, flipper_format, preset))
        return SubGhzProtocolStatusError;

    // Save numeric RIC value for easy comparison
    if(!flipper_format_write_uint32(flipper_format, "RIC", &instance->ric, 1)) {
        FURI_LOG_E(TAG, "Error adding RIC");
        return SubGhzProtocolStatusError;
    }

    msg_len = furi_string_size(instance->done_msg);
    if(!flipper_format_write_uint32(flipper_format, "MsgLen", &msg_len, 1)) {
        FURI_LOG_E(TAG, "Error adding MsgLen");
        return SubGhzProtocolStatusError;
    }

    if(!flipper_format_write_uint32(flipper_format, "PocsagVer", &instance->version, 1)) {
        FURI_LOG_E(TAG, "Error adding PocsagVer");
        return SubGhzProtocolStatusError;
    }

    uint8_t* s = (uint8_t*)furi_string_get_cstr(instance->done_msg);
    if(!flipper_format_write_hex(flipper_format, "Msg", s, msg_len)) {
        FURI_LOG_E(TAG, "Error adding Msg");
        return SubGhzProtocolStatusError;
    }
    return SubGhzProtocolStatusOk;
}

SubGhzProtocolStatus
    subghz_protocol_decoder_pocsag_deserialize(void* context, FlipperFormat* flipper_format) {
    furi_assert(context);
    SubGhzProtocolDecoderPocsag* instance = context;
    SubGhzProtocolStatus ret = SubGhzProtocolStatusError;
    uint32_t msg_len;
    uint8_t* buf;
    uint32_t ric = 0;

    do {
        if(SubGhzProtocolStatusOk !=
           pcsg_block_generic_deserialize(&instance->generic, flipper_format)) {
            break;
        }

        // Read RIC value (optional for backwards compatibility)
        if(flipper_format_read_uint32(flipper_format, "RIC", &ric, 1)) {
            instance->ric = ric;
        } else {
            // RIC not found, this is an old record - extract from Ric string
            FuriString* ric_str = furi_string_alloc();
            if(flipper_format_read_string(flipper_format, "Ric", ric_str)) {
                const char* ric_start = strstr(furi_string_get_cstr(ric_str), "RIC: ");
                if(ric_start) {
                    instance->ric = strtoul(ric_start + 5, NULL, 10);
                }
            }
            furi_string_free(ric_str);
        }

        if(!flipper_format_read_uint32(flipper_format, "MsgLen", &msg_len, 1)) {
            FURI_LOG_E(TAG, "Missing MsgLen");
            break;
        }
        //optional, so compatible backwards
        instance->version = 1200;
        flipper_format_read_uint32(flipper_format, "PocsagVer", &instance->version, 1);

        buf = malloc(msg_len);
        if(!flipper_format_read_hex(flipper_format, "Msg", buf, msg_len)) {
            FURI_LOG_E(TAG, "Missing Msg");
            free(buf);
            break;
        }
        furi_string_set_strn(instance->done_msg, (const char*)buf, msg_len);
        free(buf);

        ret = SubGhzProtocolStatusOk;
    } while(false);
    return ret;
}

void subhz_protocol_decoder_pocsag_get_string(void* context, FuriString* output) {
    furi_assert(context);
    SubGhzProtocolDecoderPocsag* instance = context;
    furi_string_cat_printf(
        output, "%s %lu\r\n", instance->generic.protocol_name, instance->version);
    furi_string_cat_printf(output, "Addr: %lu\r\n", instance->ric);
    furi_string_cat(output, instance->done_msg);
}

const SubGhzProtocolDecoder subghz_protocol_pocsag_decoder = {
    .alloc = subghz_protocol_decoder_pocsag_alloc,
    .free = subghz_protocol_decoder_pocsag_free,
    .reset = subghz_protocol_decoder_pocsag_reset,
    .feed = subghz_protocol_decoder_pocsag_feed,
    .get_hash_data = subghz_protocol_decoder_pocsag_get_hash_data,
    .serialize = subghz_protocol_decoder_pocsag_serialize,
    .deserialize = subghz_protocol_decoder_pocsag_deserialize,
    .get_string = subhz_protocol_decoder_pocsag_get_string,
};

const SubGhzProtocol subghz_protocol_pocsag = {
    .name = SUBGHZ_PROTOCOL_POCSAG_NAME,
    .type = SubGhzProtocolTypeStatic,
    .flag = SubGhzProtocolFlag_FM | SubGhzProtocolFlag_Decodable | SubGhzProtocolFlag_Save |
            SubGhzProtocolFlag_Load | SubGhzProtocolFlag_Send,

    .decoder = &subghz_protocol_pocsag_decoder,
    .encoder = &subghz_protocol_pocsag_encoder,
};

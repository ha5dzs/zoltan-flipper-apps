#include "pocsag_pager_app_i.h"
#include "protocols/pcsg_generic.h"
#include "protocols/pocsag.h"
#include "helpers/pocsag_config.h"

#define TAG "POCSAGPager"
#include <furi.h>
#include <furi/core/timer.h>
#include <furi_hal.h>
#include <flipper_format/flipper_format_i.h>
#include <lib/subghz/transmitter.h>
#include <lib/toolbox/level_duration.h>
#include <lib/subghz/devices/devices.h>
#include <gui/modules/submenu.h>
#include <gui/modules/number_input.h>
#include <gui/modules/text_input.h>
#include <gui/view_dispatcher.h>
#include <notification/notification_messages.h>
#include <string.h>

// POCSAG constants
#define POCSAG_PREAMBLE_BITS 576
#define POCSAG_SYNC_WORD 0x7CD215D8
#define POCSAG_IDLE_CODE_WORD 0x7A89C197

// Baud rate configuration - select one: 512, 1200, or 2400
// POCSAG supports 512, 1200, and 2400 baud rates
// 512 baud: 1953us per bit (1/512 = 1953.125us)
// 1200 baud: 833us per bit (1/1200 = 833.333us)
// 2400 baud: 417us per bit (1/2400 = 416.667us)
#ifndef POCSAG_BAUD_RATE
#define POCSAG_BAUD_RATE 512
#endif

// Select timing based on baud rate
#if POCSAG_BAUD_RATE == 512
#define POCSAG_TE_BASE 1953  // 512 baud: 1953.125us per bit
#elif POCSAG_BAUD_RATE == 1200
#define POCSAG_TE_BASE 833   // 1200 baud: 833.333us per bit
#elif POCSAG_BAUD_RATE == 2400
#define POCSAG_TE_BASE 417   // 2400 baud: 416.667us per bit
#else
#error "Invalid POCSAG_BAUD_RATE - must be 512, 1200, or 2400"
#endif

// Static TX state - OBSOLETE
// These were part of an earlier TX implementation. Current TX uses pcsg_tx_start()
// which manages its own local LevelDuration array. These static variables are NOT used.
static LevelDuration* pocsag_tx_data = NULL;        // OBSOLETE - not used
static size_t pocsag_tx_size = 0;                   // OBSOLETE - not used
static volatile size_t pocsag_tx_idx = 0;           // OBSOLETE - not used
static volatile bool pocsag_tx_running = false;     // OBSOLETE - not used
static volatile bool pocsag_tx_wait_sent = false;   // OBSOLETE - not used

// Static pointer to app for watchdog callback - USED
// This is set when watchdog is started and used in the watchdog callback
static POCSAGPagerApp* pocsag_tx_app = NULL;        // USED for watchdog

// Forward declarations for watchdog functions - USED
// Watchdog timer code IS called in pcsg_tx_start() and pcsg_tx_stop()
static void pocsag_tx_watchdog_init(void);          // USED
static void pocsag_tx_watchdog_start(POCSAGPagerApp* app);  // USED
static void pocsag_tx_watchdog_stop(void);          // USED
static void pocsag_tx_watchdog_callback(void* context);  // UNUSED

void pcsg_preset_init(
    void* context,
    const char* preset_name,
    uint32_t frequency,
    uint8_t* preset_data,
    size_t preset_data_size) {
    furi_assert(context);
    POCSAGPagerApp* app = context;
    furi_string_set(app->txrx->preset->name, preset_name);
    app->txrx->preset->frequency = frequency;
    app->txrx->preset->data = preset_data;
    app->txrx->preset->data_size = preset_data_size;
}

void pcsg_get_frequency_modulation(
    POCSAGPagerApp* app,
    FuriString* frequency,
    FuriString* modulation) {
    furi_assert(app);
    if(frequency != NULL) {
        furi_string_printf(
            frequency,
            "%03ld.%02ld",
            app->txrx->preset->frequency / 1000000 % 1000,
            app->txrx->preset->frequency / 10000 % 100);
    }
    if(modulation != NULL) {
        furi_string_printf(modulation, "%.2s", furi_string_get_cstr(app->txrx->preset->name));
    }
}

void pcsg_begin(POCSAGPagerApp* app, uint8_t* preset_data) {
    furi_assert(app);

    subghz_devices_reset(app->txrx->radio_device);
    subghz_devices_idle(app->txrx->radio_device);
    FURI_LOG_I(TAG, "Loading preset data: %p, first bytes: %02X %02X %02X %02X",
               preset_data, preset_data[0], preset_data[1], preset_data[2], preset_data[3]);
    subghz_devices_load_preset(app->txrx->radio_device, FuriHalSubGhzPresetCustom, preset_data);
    FURI_LOG_I(TAG, "Preset loaded successfully");

    app->txrx->txrx_state = PCSGTxRxStateIDLE;
}

uint32_t pcsg_rx(POCSAGPagerApp* app, uint32_t frequency) {
    furi_assert(app);
    if(!subghz_devices_is_frequency_valid(app->txrx->radio_device, frequency)) {
        FURI_LOG_E(TAG, "Incorrect RX frequency: %lu", frequency);
        furi_crash("POCSAGPager: Incorrect RX frequency.");
    }
    furi_assert(
        app->txrx->txrx_state != PCSGTxRxStateRx && app->txrx->txrx_state != PCSGTxRxStateSleep);

    FURI_LOG_I(TAG, "Setting up RX at %lu Hz", frequency);

    subghz_devices_idle(app->txrx->radio_device);

    // Get preset data by name (this gets the current preset, not relying on stored pointer)
    const char* preset_name = furi_string_get_cstr(app->txrx->preset->name);
    uint8_t* preset_data = subghz_setting_get_preset_data_by_name(app->setting, preset_name);
    FURI_LOG_I(TAG, "Preset data ptr: %p, size: %zu", preset_data, app->txrx->preset->data_size);
    if(preset_data) {
        FURI_LOG_I(TAG, "First preset bytes: %02X %02X %02X %02X %02X %02X %02X %02X",
            preset_data[0], preset_data[1], preset_data[2], preset_data[3],
            preset_data[4], preset_data[5], preset_data[6], preset_data[7]);
    }
    subghz_devices_load_preset(app->txrx->radio_device, FuriHalSubGhzPresetCustom, preset_data);
    FURI_LOG_I(TAG, "Preset loaded for RX");

    uint32_t value = subghz_devices_set_frequency(app->txrx->radio_device, frequency);
    FURI_LOG_I(TAG, "Frequency set to: %lu Hz (actual: %lu Hz)", frequency, value);

    subghz_devices_flush_rx(app->txrx->radio_device);
    subghz_devices_set_rx(app->txrx->radio_device);
    FURI_LOG_I(TAG, "RX mode set");

    subghz_devices_start_async_rx(
        app->txrx->radio_device, subghz_worker_rx_callback, app->txrx->worker);
    subghz_worker_start(app->txrx->worker);
    app->txrx->txrx_state = PCSGTxRxStateRx;
    FURI_LOG_I(TAG, "RX started, state=%d", app->txrx->txrx_state);
    return value;
}

void pcsg_idle(POCSAGPagerApp* app) {
    furi_assert(app);
    furi_assert(app->txrx->txrx_state != PCSGTxRxStateSleep);
    subghz_devices_idle(app->txrx->radio_device);
    app->txrx->txrx_state = PCSGTxRxStateIDLE;
}

void pcsg_rx_end(POCSAGPagerApp* app) {
    furi_assert(app);
    furi_assert(app->txrx->txrx_state == PCSGTxRxStateRx);
    if(subghz_worker_is_running(app->txrx->worker)) {
        subghz_worker_stop(app->txrx->worker);
        subghz_devices_stop_async_rx(app->txrx->radio_device);
    }
    subghz_devices_idle(app->txrx->radio_device);
    app->txrx->txrx_state = PCSGTxRxStateIDLE;
}

void pcsg_sleep(POCSAGPagerApp* app) {
    furi_assert(app);
    subghz_devices_sleep(app->txrx->radio_device);
    app->txrx->txrx_state = PCSGTxRxStateSleep;
}

void pcsg_hopper_update(POCSAGPagerApp* app) {
    furi_assert(app);

    switch(app->txrx->hopper_state) {
    case PCSGHopperStateOFF:
        return;
        break;
    case PCSGHopperStatePause:
        return;
        break;
    case PCSGHopperStateRSSITimeOut:
        if(app->txrx->hopper_timeout != 0) {
            app->txrx->hopper_timeout--;
            return;
        }
        break;
    default:
        break;
    }
    float rssi = -127.0f;
    if(app->txrx->hopper_state != PCSGHopperStateRSSITimeOut) {
        rssi = subghz_devices_get_rssi(app->txrx->radio_device);

        if(rssi > -90.0f) {
            app->txrx->hopper_timeout = 10;
            app->txrx->hopper_state = PCSGHopperStateRSSITimeOut;
            return;
        }
    } else {
        app->txrx->hopper_state = PCSGHopperStateRunnig;
    }
    if(app->txrx->hopper_idx_frequency <
       subghz_setting_get_hopper_frequency_count(app->setting) - 1) {
        app->txrx->hopper_idx_frequency++;
    } else {
        app->txrx->hopper_idx_frequency = 0;
    }

    if(app->txrx->txrx_state == PCSGTxRxStateRx) {
        pcsg_rx_end(app);
    };
    if(app->txrx->txrx_state == PCSGTxRxStateIDLE) {
        subghz_receiver_reset(app->txrx->receiver);
        app->txrx->preset->frequency =
            subghz_setting_get_hopper_frequency(app->setting, app->txrx->hopper_idx_frequency);
        pcsg_rx(app, app->txrx->preset->frequency);
    }
}

bool pcsg_validate_ric(uint32_t ric) {
    return (ric <= 9999999); // valid values are between 0 and 9999999
}

bool pcsg_validate_message(const char* message, size_t* out_length) {
    if(message == NULL) {
        return false;
    }
    size_t len = strlen(message);
    if(len > 120) {
        return false;
    }
    if(out_length) {
        *out_length = len;
    }
    return true;
}

// BCH(31,21) polynomial: x^10 + x^9 + x^8 + x^6 + x^5 + x^3 + 1
#define BCH_POLY 0x769

static uint32_t pocsag_bch_encode_31_21(uint32_t data) {
    uint32_t reg = 0;

    for(int i = 0; i < 21; i++) {
        bool bit = (data >> i) & 1;
        bool reg_msb = (reg >> 10) & 1;
        reg = (reg << 1) | bit;
        if(reg_msb) {
            reg ^= BCH_POLY;
        }
    }

    for(int i = 0; i < 10; i++) {
        bool reg_msb = (reg >> 10) & 1;
        reg = (reg << 1);
        if(reg_msb) {
            reg ^= BCH_POLY;
        }
    }

    return reg & 0x3FF;
}

static uint32_t pocsag_even_parity(uint32_t data) {
    uint32_t count = 0;
    for(int i = 0; i < 32; i++) {
        if(data & (1 << i)) count++;
    }
    return count & 1;
}

static uint32_t pocsag_encode_address_codeword(uint32_t ric, uint8_t func) {
    uint32_t codeword = 0;
    uint32_t address_payload = ric >> 3;

    // Bit 31 = 0 (address marker)
    // According to POCSAG spec:
    // Bits 30-13: 18-bit address payload (RIC / 8)
    // Bits 12-11: 2-bit function code
    // Bits 10-1: 10-bit BCH
    // Bit 0: even parity

    // Build the codeword with address payload and function
    codeword = (address_payload & 0x3FFFF) << 13;  // Address at bits 30-13
    codeword |= (func & 0x3) << 11;                 // Function at bits 12-11

    // BCH(31,21) encodes 21 bits of data to produce 10 bits of ECC
    // The 21 data bits are: bit 20 = 0 (address marker), bits 19-2 = address payload, bits 1-0 = function
    // This corresponds to bits 30-11 of the codeword (after shifting)
    // To get the 21 data bits for BCH, we shift right by 11: bits 30-11 become bits 19-0
    uint32_t bch = pocsag_bch_encode_31_21(codeword >> 11);
    codeword |= (bch & 0x3FF) << 1;

    // Bit 0: even parity
    codeword |= pocsag_even_parity(codeword);

    return codeword;
}

// Encode alphanumeric message into message codewords
// Each message codeword contains 20 bits of payload in bits 30-11, with bit 31 = 1
// The decoder reads payload MSB-first and builds characters LSB-first
// (first bit read -> bit 0 of char, last bit read -> bit 6 of char)
// Returns number of codewords generated
static size_t pocsag_encode_message_codewords(
    const char* message,
    size_t msg_len,
    uint32_t* codewords,
    size_t max_codewords) {
    size_t cw_idx = 0;
    uint32_t pending_bits = 0;
    uint8_t pending_count = 0;

    // Pack characters LSB-first so they decode correctly
    // The decoder reads MSB first from codeword but builds char LSB-first
    // So we send char LSB first, which means: char bit 0 -> codeword MSB, char bit 6 -> codeword LSB
    for(size_t i = 0; i < msg_len; i++) {
        uint8_t ch = message[i] & 0x7F;
        // Convert character to LSB-first bit order (bit 0 -> MSB, bit 6 -> LSB)
        uint8_t ch_bits = 0;
        for(uint8_t j = 0; j < 7; j++) {
            if(ch & (1 << j)) {
                ch_bits |= (1 << (6 - j));
            }
        }

        // Pack: add character bits at MSB position of pending
        pending_bits = (pending_bits << 7) | ch_bits;
        pending_count += 7;

        // Extract 20-bit codewords as they become available
        while(pending_count >= 20 && cw_idx < max_codewords) {
            // Extract top 20 bits from pending
            uint32_t payload = (pending_bits >> (pending_count - 20)) & 0xFFF5F;
            payload = (pending_bits >> (pending_count - 20)) & 0xFFFFF;

            // Build codeword: bit 31 = 1 (message marker), bits 30-11 = payload
            uint32_t codeword = 1 << 31;
            codeword |= (payload & 0xFFF5F) << 11;
            codeword |= (payload & 0xFFFFF) << 11;

            // Add BCH(31,21) error correction (10 bits)
            uint32_t bch = pocsag_bch_encode_31_21(codeword >> 11);
            codeword |= (bch & 0x3FF) << 1;

            // Add even parity bit
            codeword |= pocsag_even_parity(codeword);

            codewords[cw_idx++] = codeword;
            pending_bits &= (1UL << (pending_count - 20)) - 1;
            pending_count -= 20;
        }
    }

    // Handle remaining bits (partial codeword)
    if(pending_count > 0 && cw_idx < max_codewords) {
        // Pad to 20 bits with zeros
        uint32_t payload = pending_bits << (20 - pending_count);
        uint32_t codeword = 1 << 31;
        codeword |= (payload & 0xFFF5F) << 11;
        codeword |= (payload & 0xFFFFF) << 11;

        uint32_t bch = pocsag_bch_encode_31_21(codeword >> 11);
        codeword |= (bch & 0x3FF) << 1;
        codeword |= pocsag_even_parity(codeword);

        codewords[cw_idx++] = codeword;
    }

    return cw_idx;
}

static void pocsag_add_codeword(LevelDuration* tx_data, size_t* idx, uint32_t cw, uint32_t te) {
    // POCSAG NRZ encoding (codeword MSB first, te based on configured baud rate)
    // NRZ: bit 1 = LOW level, bit 0 = HIGH level (decoder inverts internally)
    for(int i = 31; i >= 0; i--) {
        bool bit = (cw >> i) & 1;
        // bit=1 -> send LOW (level=1), bit=0 -> send HIGH (level=2)
        tx_data[*idx] = level_duration_make(!bit, te);
        (*idx)++;
    }
}

// Generate POCSAG transmission data
static bool pocsag_generate_tx_data(
    uint32_t ric,
    const char* message,
    size_t msg_len,
    LevelDuration** data,
    size_t* size) {
    FURI_LOG_I(TAG, "Generating TX data");

    size_t num_msg_cw = (msg_len * 7 + 19) / 20;
    FURI_LOG_I(TAG, "Message codewords needed: %zu", num_msg_cw);

    uint32_t target_frame = ric % 8;
    FURI_LOG_I(TAG, "Target frame: %lu", target_frame);

    uint32_t* msg_cws = malloc(num_msg_cw > 0 ? num_msg_cw * sizeof(uint32_t) : 1);
    if(msg_cws && num_msg_cw > 0) {
        pocsag_encode_message_codewords(message, msg_len, msg_cws, num_msg_cw);
    }

    // Calculate size: preamble(576) + sync(32) + 16 cw(512) + sync(32) + 16 cw(512) + padding
    // 576 + 32 + 512 + 32 + 512 + 32*32(idle) = 576 + 32 + 512 + 32 + 512 + 1024 = 2688
    // Add extra for safety
    size_t total_size = 3000;

    LevelDuration* tx_data = malloc(total_size * sizeof(LevelDuration));
    if(!tx_data) {
        FURI_LOG_E(TAG, "Failed to allocate TX data");
        free(msg_cws);
        return false;
    }

    size_t idx = 0;
    uint32_t te = POCSAG_TE_BASE;

    // Preamble (NRZ encoding: 1 = HIGH for full bit, 0 = LOW for full bit)
    // Note: Decoder expects inverted signal (applies !level internally)
    for(size_t i = 0; i < POCSAG_PREAMBLE_BITS; i++) {
        bool bit = (i % 2) == 0;
        tx_data[idx++] = level_duration_make(!bit, te);
    }

    // First batch
    pocsag_add_codeword(tx_data, &idx, POCSAG_SYNC_WORD, te);

    size_t msg_cw_idx = 0;
    FURI_LOG_I(TAG, "Before frame loop: msg_cw_idx=%zu", msg_cw_idx);
    for(uint32_t f = 0; f < 8; f++) {
        // Frame A
        if(f < target_frame) {
            pocsag_add_codeword(tx_data, &idx, POCSAG_IDLE_CODE_WORD, te);
            // Frame B - also idle for frames before target
            pocsag_add_codeword(tx_data, &idx, POCSAG_IDLE_CODE_WORD, te);
        } else if(f == target_frame) {
            uint32_t addr_cw = pocsag_encode_address_codeword(ric, 3);
            pocsag_add_codeword(tx_data, &idx, addr_cw, te);
            if(msg_cw_idx < num_msg_cw) {
                pocsag_add_codeword(tx_data, &idx, msg_cws[msg_cw_idx++], te);
            } else {
                pocsag_add_codeword(tx_data, &idx, POCSAG_IDLE_CODE_WORD, te);
            }
        } else {
            if(msg_cw_idx < num_msg_cw) {
                pocsag_add_codeword(tx_data, &idx, msg_cws[msg_cw_idx++], te);
            } else {
                pocsag_add_codeword(tx_data, &idx, POCSAG_IDLE_CODE_WORD, te);
            }
            // Frame B
            if(msg_cw_idx < num_msg_cw) {
                pocsag_add_codeword(tx_data, &idx, msg_cws[msg_cw_idx++], te);
            } else {
                pocsag_add_codeword(tx_data, &idx, POCSAG_IDLE_CODE_WORD, te);
            }
        }
        FURI_LOG_I(TAG, "After frame %lu", f);
    }

    // Second batch
    pocsag_add_codeword(tx_data, &idx, POCSAG_SYNC_WORD, te);

    // Remaining message codewords
    for(uint32_t f = 0; f < 8 && msg_cw_idx < num_msg_cw; f++) {
        for(int cw_idx = 0; cw_idx < 2 && msg_cw_idx < num_msg_cw; cw_idx++) {
            pocsag_add_codeword(tx_data, &idx, msg_cws[msg_cw_idx++], te);
        }
    }

    // Pad with idle codewords
    for(int i = 0; i < 32; i++) {
        pocsag_add_codeword(tx_data, &idx, POCSAG_IDLE_CODE_WORD, te);
    }

    // Add a LOW level entry to ensure middleware state is properly reset
    // This ensures when RESET is returned, is_odd_level is false, so middleware returns 0
    if(idx < total_size) {
        tx_data[idx++] = level_duration_make(0, te);  // LOW level to reset middleware state
    }

    // Add LEVEL_DURATION_WAIT at the end to properly terminate transmission
    if(idx < total_size) {
        tx_data[idx++] = level_duration_wait();
    }

    free(msg_cws);
    *data = tx_data;
    *size = idx;
    FURI_LOG_I(TAG, "TX data generated: %zu transitions", idx);
    return idx > 0;
}

// Watchdog timer for transmission timeout - USED
// This watchdog is started in pcsg_tx_start() and stopped in pcsg_tx_stop()
// It prevents infinite transmission if hardware doesn't complete properly.
// Timeout is based on baud rate: 15s for 512, ~7.5s for 1200, ~3.75s for 2400
static FuriTimer* pocsag_tx_watchdog_timer = NULL;  // USED
// Timeout: 15s for 512 baud, less for higher rates (1200: ~6.25s, 2400: ~3s)
#define POCSAG_TX_TIMEOUT_MS (15000 / (POCSAG_BAUD_RATE / 512))

// Initialize the watchdog timer - USED in pcsg_tx_start()
static void pocsag_tx_watchdog_init(void) {         // USED
    if(pocsag_tx_watchdog_timer == NULL) {
        pocsag_tx_watchdog_timer = furi_timer_alloc(pocsag_tx_watchdog_callback, FuriTimerTypeOnce, NULL);
    }
}

// Start the watchdog timer - USED in pcsg_tx_start()
static void pocsag_tx_watchdog_start(POCSAGPagerApp* app) {  // USED
    furi_assert(pocsag_tx_watchdog_timer != NULL);
    pocsag_tx_app = app;  // Store app pointer for watchdog callback
    furi_timer_start(pocsag_tx_watchdog_timer, POCSAG_TX_TIMEOUT_MS);
}

// Stop the watchdog timer - USED in pcsg_tx_stop()
static void pocsag_tx_watchdog_stop(void) {         // USED
    if(pocsag_tx_watchdog_timer != NULL) {
        furi_timer_stop(pocsag_tx_watchdog_timer);
    }
}

// Watchdog timer callback - called when transmission times out - USED
// This is called if TX runs too long (timeout based on baud rate)
// Prevents infinite transmission if hardware doesn't complete properly
static void pocsag_tx_watchdog_callback(void* context) {  // USED
    UNUSED(context);
    // Use the static app pointer that was set when transmission started
    POCSAGPagerApp* app = pocsag_tx_app;
    if(app == NULL) {
        FURI_LOG_E(TAG, "TX Watchdog: No app context available");
        return;
    }

    FURI_LOG_E(TAG, "TX Watchdog: Transmission timeout after %d ms", POCSAG_TX_TIMEOUT_MS);
    FURI_LOG_E(TAG, "State: running=%d, data=%p, idx=%zu, size=%zu",
               pocsag_tx_running, pocsag_tx_data, pocsag_tx_idx, pocsag_tx_size);

    // Stop transmission
    pocsag_tx_running = false;
    if(pocsag_tx_data) {
        free((LevelDuration*)pocsag_tx_data);
        pocsag_tx_data = NULL;
        pocsag_tx_size = 0;
        pocsag_tx_idx = 0;
    }

    // Stop radio async TX
    if(app->txrx->txrx_state == PCSGTxRxStateTx) {
        subghz_devices_stop_async_tx(app->txrx->radio_device);
        subghz_devices_idle(app->txrx->radio_device);
        app->txrx->txrx_state = PCSGTxRxStateIDLE;
    }

    // Return to Send menu
    scene_manager_search_and_switch_to_previous_scene(app->scene_manager, POCSAGPagerSceneSend);

    FURI_LOG_E(TAG, "TX Watchdog: Transmission stopped, returning to Send menu");
}

// Async TX callback - this runs in interrupt context! - USED for legacy TX path
// This callback was part of an earlier TX implementation. Current TX uses pcsg_tx_start()
// which manages its own LevelDuration array directly through subghz_devices_start_async_tx()
// The static state variables (pocsag_tx_data, etc.) are NOT used, but this callback
// definition remains for potential fallback or alternative TX paths.
static LevelDuration pocsag_tx_yield_callback(void* context) {  // OBSOLETE - not used
    UNUSED(context);

    // CRITICAL: Log first thing - this MUST be printed
    //FURI_LOG_W(TAG, "CB_ENTER: running=%d data=%p idx=%zu size=%zu", pocsag_tx_running, pocsag_tx_data, pocsag_tx_idx, pocsag_tx_size);

    // Check if TX should stop
    if(!pocsag_tx_running) {
        FURI_LOG_W(TAG, "CB_EXIT: TX not running");
        LevelDuration reset = {0};
        reset.level = LEVEL_DURATION_RESET;
        reset.duration = 0;
        return reset;
    }

    if(!pocsag_tx_data) {
        FURI_LOG_E(TAG, "CB_EXIT: TX data is NULL");
        pocsag_tx_running = false;
        LevelDuration reset = {0};
        reset.level = LEVEL_DURATION_RESET;
        reset.duration = 0;
        return reset;
    }

    if(pocsag_tx_idx >= pocsag_tx_size) {
        FURI_LOG_W(TAG, "CB_EXIT: TX complete (idx=%zu >= size=%zu)", pocsag_tx_idx, pocsag_tx_size);
        pocsag_tx_running = false;
        // Use WAIT to signal end of transmission, then RESET to stop
        if(!pocsag_tx_wait_sent) {
            pocsag_tx_wait_sent = true;
            return level_duration_wait();
        }
        pocsag_tx_wait_sent = false;
        LevelDuration reset = {0};
        reset.level = LEVEL_DURATION_RESET;
        reset.duration = 0;
        return reset;
    }

    // Yield next level duration
    LevelDuration result = pocsag_tx_data[pocsag_tx_idx++];
    return result;
}

bool pcsg_tx_is_complete(void) {
    return !pocsag_tx_running;
}

void pcsg_tx_start(POCSAGPagerApp* app, uint32_t ric, const char* message) {
    furi_assert(app);
    furi_assert(message);

    FURI_LOG_I(TAG, ">>> PCSG_TX_START CALLED: RIC=%lu, message='%s'", ric, message);
    FURI_LOG_I(TAG, "pocsag_tx_data=%p, pocsag_tx_size=%zu, pocsag_tx_idx=%zu",
               pocsag_tx_data, pocsag_tx_size, pocsag_tx_idx);

    // Clean up any previous TX state
    if(pocsag_tx_data) {
        free((LevelDuration*)pocsag_tx_data);
        pocsag_tx_data = NULL;
        pocsag_tx_size = 0;
        pocsag_tx_idx = 0;
        pocsag_tx_running = false;
    }

    if(app->txrx->txrx_state == PCSGTxRxStateRx) {
        FURI_LOG_I(TAG, "Stopping RX");
        pcsg_rx_end(app);
    }
    pcsg_idle(app);

    const char* preset_name = furi_string_get_cstr(app->txrx->preset->name);
    uint8_t* preset_data = subghz_setting_get_preset_data_by_name(app->setting, preset_name);
    FURI_LOG_I(TAG, "Preset data: %p", preset_data);

    FURI_LOG_I(TAG, "Generating TX data...");
    size_t msg_len = strlen(message);
    FURI_LOG_I(TAG, "Message length: %zu", msg_len);
    size_t tx_data_size = 0;
    if(!pocsag_generate_tx_data(ric, message, msg_len, &pocsag_tx_data, &tx_data_size)) {
        FURI_LOG_E(TAG, "Failed to generate TX data (pocsag_tx_data=%p, size=%zu)", pocsag_tx_data, tx_data_size);
        return;
    }
    pocsag_tx_size = tx_data_size;
    FURI_LOG_I(TAG, "pocsag_generate_tx_data returned true");
    FURI_LOG_I(TAG, "TX data generated, size=%zu", pocsag_tx_size);
    FURI_LOG_I(TAG, "Before start_async_tx: data=%p, idx=%zu", pocsag_tx_data, pocsag_tx_idx);

    FURI_LOG_I(TAG, "Configuring radio");
    // Use device wrapper functions for external CC1101 device
    subghz_devices_reset(app->txrx->radio_device);
    subghz_devices_idle(app->txrx->radio_device);

    FURI_LOG_I(TAG, "Preset name: %s", preset_name);
    FURI_LOG_I(TAG, "Preset data ptr before load: %p", preset_data);
    if(preset_data) {
        FURI_LOG_I(TAG, "First 16 preset bytes: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
            preset_data[0], preset_data[1], preset_data[2], preset_data[3],
            preset_data[4], preset_data[5], preset_data[6], preset_data[7],
            preset_data[8], preset_data[9], preset_data[10], preset_data[11],
            preset_data[12], preset_data[13], preset_data[14], preset_data[15]);
    }
    subghz_devices_load_preset(app->txrx->radio_device, FuriHalSubGhzPresetCustom, preset_data);
    FURI_LOG_I(TAG, "Preset loaded for TX");

    uint32_t actual_freq = subghz_devices_set_frequency(app->txrx->radio_device, app->txrx->preset->frequency);
    FURI_LOG_I(TAG, "Frequency set to: %lu Hz (actual: %lu Hz)", app->txrx->preset->frequency, actual_freq);

    if(!subghz_devices_set_tx(app->txrx->radio_device)) {
        FURI_LOG_E(TAG, "Failed to set TX mode");
        if(pocsag_tx_data) {
            free((LevelDuration*)pocsag_tx_data);
            pocsag_tx_data = NULL;
        }
        return;
    }

    app->txrx->txrx_state = PCSGTxRxStateTx;
    pocsag_tx_running = true;
    pocsag_tx_idx = 0;
    pocsag_tx_wait_sent = false;  // Reset wait flag for new transmission
    FURI_LOG_I(TAG, "After setting idx: data=%p, idx=%zu, size=%zu", pocsag_tx_data, pocsag_tx_idx, pocsag_tx_size);

    FURI_LOG_I(TAG, "Starting async TX with data=%p, size=%zu, callback=%p", pocsag_tx_data, pocsag_tx_size, (void*)pocsag_tx_yield_callback);
    FURI_LOG_I(TAG, "Before start_async_tx: running=%d, idx=%zu", pocsag_tx_running, pocsag_tx_idx);
    // Use device wrapper for async TX with external devices
    bool tx_started = subghz_devices_start_async_tx(app->txrx->radio_device, pocsag_tx_yield_callback, NULL);
    FURI_LOG_I(TAG, "Async TX started: %d, data=%p, size=%zu, idx=%zu, running=%d", tx_started, pocsag_tx_data, pocsag_tx_size, pocsag_tx_idx, pocsag_tx_running);

    // If start_async_tx returned false, TX didn't start
    if(!tx_started) {
        FURI_LOG_E(TAG, "start_async_tx failed!");
        pocsag_tx_running = false;
        if(pocsag_tx_data) {
            free((LevelDuration*)pocsag_tx_data);
            pocsag_tx_data = NULL;
        }
        return;
    }

    // Initialize and start watchdog timer
    pocsag_tx_watchdog_init();
    pocsag_tx_watchdog_start(app);

    // Check if callback was called synchronously during start_async_tx
    FURI_LOG_I(TAG, "After start_async_tx: running=%d, idx=%zu", pocsag_tx_running, pocsag_tx_idx);
}

void pcsg_tx_stop(POCSAGPagerApp* app) {
    furi_assert(app);

    FURI_LOG_I(TAG, "Stopping TX");

    // Stop the watchdog timer first
    pocsag_tx_watchdog_stop();

    pocsag_tx_running = false;
    pocsag_tx_wait_sent = false;  // Reset wait flag

    if(pocsag_tx_data) {
        free((LevelDuration*)pocsag_tx_data);
        pocsag_tx_data = NULL;
        pocsag_tx_size = 0;
        pocsag_tx_idx = 0;
    }

    if(app->txrx->txrx_state == PCSGTxRxStateTx) {
        subghz_devices_stop_async_tx(app->txrx->radio_device);
        subghz_devices_idle(app->txrx->radio_device);
        app->txrx->txrx_state = PCSGTxRxStateIDLE;
    }
}

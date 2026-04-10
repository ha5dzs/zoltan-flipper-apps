#include "pocsag_config.h"

#include <furi.h>
#include <furi_hal.h>
#include <string.h>
#include <strings.h>
#include <flipper_format/flipper_format.h>
#include <storage/storage.h>

#include "../pocsag_pager_app_i.h"
#include "../protocols/pcsg_generic.h"

#define TAG "POCSAGConfig"

#define PCSG_SETTINGS_DIR "/ext/pocsag"
#define PCSG_SETTINGS_FILE "/ext/pocsag/pocsag_settings.txt"
#define PCSG_HISTORY_FILE "/ext/pocsag/pocsag_received_pages.csv"

bool pocsag_pager_ensure_dir_exists(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);

    // storage_simply_mkdir returns true if directory exists or was created successfully
    bool dir_exists = storage_simply_mkdir(storage, PCSG_SETTINGS_DIR);
    FURI_LOG_D(TAG, "Directory %s exists/created: %s", PCSG_SETTINGS_DIR, dir_exists ? "yes" : "no");

    furi_record_close(RECORD_STORAGE);
    return dir_exists;
}

bool pocsag_pager_app_save_settings(POCSAGPagerApp* app) {
    furi_assert(app);

    FURI_LOG_I(TAG, "Saving settings to %s", PCSG_SETTINGS_FILE);
    FURI_LOG_I(TAG, "  freq=%lu, preset=%s, hopping=%u, addr=%lu",
               app->txrx->preset->frequency, furi_string_get_cstr(app->txrx->preset->name),
               app->txrx->hopper_state, app->txrx->own_address);

    Storage* storage = furi_record_open(RECORD_STORAGE);

    bool result = false;

    do {
        // Ensure directory exists
        if(!pocsag_pager_ensure_dir_exists()) {
            FURI_LOG_E(TAG, "Failed to create directory");
            break;
        }

        FlipperFormat* ff = flipper_format_file_alloc(storage);

        if(!flipper_format_file_open_always(ff, PCSG_SETTINGS_FILE)) {
            FURI_LOG_E(TAG, "Failed to open file for writing");
            flipper_format_free(ff);
            break;
        }

        // Write header
        if(!flipper_format_write_header_cstr(ff, "POCSAG Pager Settings", 1)) {
            FURI_LOG_E(TAG, "Failed to write header");
            flipper_format_free(ff);
            break;
        }

        // Write frequency
        uint32_t frequency = app->txrx->preset->frequency;
        if(!flipper_format_write_uint32(ff, "Frequency", &frequency, 1)) {
            FURI_LOG_E(TAG, "Failed to write frequency");
            flipper_format_free(ff);
            break;
        }

        // Write hopping state
        uint32_t hopping = (uint32_t)app->txrx->hopper_state;
        if(!flipper_format_write_uint32(ff, "Hopping", &hopping, 1)) {
            FURI_LOG_E(TAG, "Failed to write hopping");
            flipper_format_free(ff);
            break;
        }

        // Write preset name
        if(!flipper_format_write_string_cstr(ff, "Preset", furi_string_get_cstr(app->txrx->preset->name))) {
            FURI_LOG_E(TAG, "Failed to write preset");
            flipper_format_free(ff);
            break;
        }

        // Write own address
        uint32_t own_address = app->txrx->own_address;
        if(!flipper_format_write_uint32(ff, "OwnAddress", &own_address, 1)) {
            FURI_LOG_E(TAG, "Failed to write own address");
            flipper_format_free(ff);
            break;
        }

        result = true;
        FURI_LOG_I(TAG, "Settings saved successfully to %s", PCSG_SETTINGS_FILE);

        flipper_format_free(ff);

    } while(false);

    furi_record_close(RECORD_STORAGE);

    return result;
}

bool pocsag_pager_app_load_settings(POCSAGPagerApp* app) {
    furi_assert(app);

    Storage* storage = furi_record_open(RECORD_STORAGE);

    bool result = false;
    bool file_exists = storage_file_exists(storage, PCSG_SETTINGS_FILE);

    if(!file_exists) {
        FURI_LOG_W(TAG, "Settings file not found, using defaults");
        // Don't save here - the settings will be saved when user exits config
        result = true;
        furi_record_close(RECORD_STORAGE);
        return result;
    }

    do {
        FlipperFormat* ff = flipper_format_file_alloc(storage);

        if(!flipper_format_file_open_existing(ff, PCSG_SETTINGS_FILE)) {
            FURI_LOG_E(TAG, "Failed to open file for reading");
            flipper_format_free(ff);
            break;
        }

        // Read and validate header
        uint32_t header_version = 0;
        FuriString* header_str = furi_string_alloc();
        bool header_valid = flipper_format_read_header(ff, header_str, &header_version);

        if(!header_valid || furi_string_cmp_str(header_str, "POCSAG Pager Settings") != 0) {
            FURI_LOG_W(TAG, "Invalid or missing header, using defaults");
            furi_string_free(header_str);
            flipper_format_free(ff);
            result = true;
            break;
        }
        furi_string_free(header_str);

        // Read frequency
        uint32_t frequency = PCSG_DEFAULT_FREQUENCY;
        if(!flipper_format_read_uint32(ff, "Frequency", &frequency, 1)) {
            FURI_LOG_W(TAG, "Missing Frequency, using default");
            frequency = PCSG_DEFAULT_FREQUENCY;
        }

        // Validate frequency is within CC1101 range (300-928 MHz)
        if(frequency < 300000000 || frequency > 928000000) {
            FURI_LOG_W(TAG, "Frequency out of range (%lu), using default", frequency);
            frequency = PCSG_DEFAULT_FREQUENCY;
        }

        // Store valid frequency for later use
        app->txrx->preset->frequency = frequency;

        // Read hopping state
        uint32_t hopping_val = PCSG_DEFAULT_HOPPING;
        if(!flipper_format_read_uint32(ff, "Hopping", &hopping_val, 1)) {
            FURI_LOG_W(TAG, "Missing Hopping, using default");
            hopping_val = PCSG_DEFAULT_HOPPING;
        }
        if(hopping_val > PCSGHopperStateRSSITimeOut) {
            FURI_LOG_W(TAG, "Invalid Hopping value, using default");
            hopping_val = PCSGHopperStateOFF;
        }
        app->txrx->hopper_state = (PCSGHopperState)hopping_val;

        // Read preset name
        FuriString* preset_name = furi_string_alloc();
        if(!flipper_format_read_string(ff, "Preset", preset_name)) {
            FURI_LOG_W(TAG, "Missing Preset, using default");
            furi_string_set_str(preset_name, PCSG_DEFAULT_PRESET);
        }

        // Read own address
        uint32_t own_address = PCSG_DEFAULT_ADDRESS;
        if(!flipper_format_read_uint32(ff, "OwnAddress", &own_address, 1)) {
            FURI_LOG_W(TAG, "Missing OwnAddress, using default");
            own_address = PCSG_DEFAULT_ADDRESS;
        }
        app->txrx->own_address = own_address;

        // Apply loaded settings
        furi_string_set(app->txrx->preset->name, preset_name);
        // Frequency was already applied above

        FURI_LOG_D(TAG, "Settings loaded: freq=%lu, preset=%s, hopping=%lu, addr=%lu",
                   frequency, furi_string_get_cstr(preset_name), hopping_val, own_address);

        furi_string_free(preset_name);
        flipper_format_free(ff);

        result = true;

    } while(false);

    furi_record_close(RECORD_STORAGE);

    return result;
}

bool pocsag_pager_log_page(uint32_t ric, const char* message) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    bool result = false;

    do {
        // Ensure directory exists
        if(!pocsag_pager_ensure_dir_exists()) {
            FURI_LOG_E(TAG, "Failed to create directory for logging");
            break;
        }

        // Check if file exists, if not write header
        bool file_exists = storage_file_exists(storage, PCSG_HISTORY_FILE);
        FURI_LOG_D(TAG, "File exists: %d", file_exists);

        // Open file for append
        if(!storage_file_open(file, PCSG_HISTORY_FILE, FSAM_WRITE, FSOM_OPEN_APPEND)) {
            FURI_LOG_E(TAG, "Failed to open log file for appending");
            break;
        }

        // Get current time from RTC and system ticks
        DateTime datetime = {0};
        furi_hal_rtc_get_datetime(&datetime);

        // Calculate Unix timestamp from RTC datetime (in seconds)
        // This is a simplified calculation - for production use, consider a full calendar library
        uint32_t year = datetime.year;
        uint32_t month = datetime.month;
        uint32_t day = datetime.day;
        uint32_t hour = datetime.hour;
        uint32_t minute = datetime.minute;
        uint32_t second = datetime.second;

        // Days since year start (simplified, not accounting for leap years perfectly)
        const uint16_t days_before_month[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
        uint16_t days_since_year_start = days_before_month[month - 1] + day - 1;

        // Count leap years since 1970
        uint32_t years_since_1970 = year - 1970;
        uint32_t leap_days = (year - 1969) / 4 - (year - 1901) / 100 + (year - 1601) / 400;

        // Total days since Unix epoch
        uint32_t total_days = years_since_1970 * 365 + leap_days + days_since_year_start;

        // Unix timestamp in seconds
        uint64_t unix_time_seconds = (uint64_t)total_days * 86400 + hour * 3600 + minute * 60 + second;

        // Get milliseconds since boot from system ticks
        uint32_t tick_ms = furi_get_tick();

        // Extract the millisecond fraction (tick % 1000) for sub-second precision
        uint32_t ms_fraction = tick_ms % 1000;

        // Combine for full millisecond precision
        uint64_t unix_time_ms = unix_time_seconds * 1000 + ms_fraction;

        FURI_LOG_D(TAG, "Logging page: time=%llu, ric=%lu, msg=%s", unix_time_ms, ric, message ? message : "");

        // Write header if file is new
        if(!file_exists) {
            storage_file_write(file, "UnixTime,Address,Message\n", 25); // The second argument is the length of the string. It was 24 bytes.
        }

        // Format and write the log entry
        // Quote the message to handle special characters properly
        FuriString* log_line = furi_string_alloc();
        furi_string_printf(log_line, "%llu,%07lu,\"%s\"\n", unix_time_ms, ric, message ? message : "");

        storage_file_write(file, furi_string_get_cstr(log_line), strlen(furi_string_get_cstr(log_line)));
        furi_string_free(log_line);

        result = true;
        storage_file_close(file);

    } while(false);

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    return result;
}

// Log buffer functions - LOG_BUFFER_FUNCTIONS
// These implement a circular buffer to queue log entries for async writing
// to CSV without blocking the RX callback which would cause dropped pages

void pocsag_log_buffer_init(PCSGLogBuffer* buffer) {  // LOG_BUFFER_FUNCTIONS
    furi_assert(buffer);
    memset(buffer, 0, sizeof(PCSGLogBuffer));
    buffer->head = 0;
    buffer->count = 0;
}

bool pocsag_log_buffer_add(PCSGLogBuffer* buffer, uint32_t ric, const char* message) {  // LOG_BUFFER_FUNCTIONS
    furi_assert(buffer);
    furi_assert(message);

    // Check if buffer is full
    if(buffer->count >= PCSG_LOG_BUFFER_SIZE) {
        FURI_LOG_W(TAG, "Log buffer full, discarding entry");
        return false;
    }

    // Add entry at head position
    buffer->entries[buffer->head].ric = ric;
    strlcpy(buffer->entries[buffer->head].message, message, PCSG_MAX_MESSAGE_LENGTH);

    // Advance head (circular)
    buffer->head = (buffer->head + 1) % PCSG_LOG_BUFFER_SIZE;
    buffer->count++;

    FURI_LOG_D(TAG, "Log buffer: count=%d, head=%d", buffer->count, buffer->head);

    return true;
}

bool pocsag_log_buffer_has_entries(PCSGLogBuffer* buffer) {  // LOG_BUFFER_FUNCTIONS
    furi_assert(buffer);
    return buffer->count > 0;
}

uint8_t pocsag_log_buffer_count(PCSGLogBuffer* buffer) {  // LOG_BUFFER_FUNCTIONS
    furi_assert(buffer);
    return buffer->count;
}

bool pocsag_log_buffer_get_next(PCSGLogBuffer* buffer, uint32_t* ric, char* message, size_t msg_size) {  // LOG_BUFFER_FUNCTIONS
    furi_assert(buffer);
    furi_assert(ric);
    furi_assert(message);

    // Check if buffer is empty
    if(buffer->count == 0) {
        return false;
    }

    // Calculate tail position (oldest entry)
    uint8_t tail = (buffer->head + PCSG_LOG_BUFFER_SIZE - buffer->count) % PCSG_LOG_BUFFER_SIZE;

    // Get entry
    *ric = buffer->entries[tail].ric;
    strlcpy(message, buffer->entries[tail].message, msg_size);

    // Remove entry by shifting (simple approach for small buffer)
    for(uint8_t i = tail; i != buffer->head; i = (i + 1) % PCSG_LOG_BUFFER_SIZE) {
        uint8_t next = (i + 1) % PCSG_LOG_BUFFER_SIZE;
        buffer->entries[i].ric = buffer->entries[next].ric;
        strlcpy(buffer->entries[i].message, buffer->entries[next].message, PCSG_MAX_MESSAGE_LENGTH);
    }

    // Clear last entry
    memset(&buffer->entries[(buffer->head + PCSG_LOG_BUFFER_SIZE - 1) % PCSG_LOG_BUFFER_SIZE], 0, sizeof(PCSGLogEntry));

    buffer->count--;
    FURI_LOG_D(TAG, "Log buffer: count=%d after get", buffer->count);

    return true;
}


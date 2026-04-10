#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <furi.h>

// Forward declarations
typedef struct POCSAGPagerApp POCSAGPagerApp;

#include "../helpers/pocsag_pager_types.h"

/**
 * Create the pocsag directory if it doesn't exist
 * @return true on success, false on failure
 */
bool pocsag_pager_ensure_dir_exists(void);

/**
 * Save POCSAG settings to config file
 * @param app Pointer to POCSAGPagerApp
 * @return true on success, false on failure
 */
bool pocsag_pager_app_save_settings(POCSAGPagerApp* app);

/**
 * Load POCSAG settings from config file
 * @param app Pointer to POCSAGPagerApp
 * @return true on success, false if file is invalid and needs to be recreated
 */
bool pocsag_pager_app_load_settings(POCSAGPagerApp* app);

/**
 * Initialize the CSV log buffer - LOG_BUFFER_FUNCTIONS
 * @param buffer Pointer to PCSGLogBuffer to initialize
 * NOTE: These functions implement a circular buffer for async logging
 * to avoid blocking RX callbacks with file I/O operations.
 */
void pocsag_log_buffer_init(PCSGLogBuffer* buffer);  // LOG_BUFFER_FUNCTIONS

/**
 * Add a page to the log buffer (non-blocking) - LOG_BUFFER_FUNCTIONS
 * @param buffer Pointer to PCSGLogBuffer
 * @param ric RIC (Receiver ID) of the page
 * @param message Message content
 * @return true if added, false if buffer is full
 * NOTE: These functions implement a circular buffer for async logging
 * to avoid blocking RX callbacks with file I/O operations.
 */
bool pocsag_log_buffer_add(PCSGLogBuffer* buffer, uint32_t ric, const char* message);  // LOG_BUFFER_FUNCTIONS

/**
 * Get the next entry from the log buffer (blocks until entry available or buffer empty) - LOG_BUFFER_FUNCTIONS
 * @param buffer Pointer to PCSGLogBuffer
 * @param ric Output RIC
 * @param message Output message buffer
 * @param msg_size Size of message buffer
 * @return true if entry retrieved, false if buffer empty
 * NOTE: These functions implement a circular buffer for async logging
 * to avoid blocking RX callbacks with file I/O operations.
 */
bool pocsag_log_buffer_get_next(PCSGLogBuffer* buffer, uint32_t* ric, char* message, size_t msg_size);  // LOG_BUFFER_FUNCTIONS

/**
 * Check if log buffer has entries pending - LOG_BUFFER_FUNCTIONS
 * @param buffer Pointer to PCSGLogBuffer
 * @return true if entries pending, false if empty
 * NOTE: These functions implement a circular buffer for async logging
 * to avoid blocking RX callbacks with file I/O operations.
 */
bool pocsag_log_buffer_has_entries(PCSGLogBuffer* buffer);  // LOG_BUFFER_FUNCTIONS

/**
 * Get count of pending entries in log buffer - LOG_BUFFER_FUNCTIONS
 * @param buffer Pointer to PCSGLogBuffer
 * @return Number of pending entries
 * NOTE: These functions implement a circular buffer for async logging
 * to avoid blocking RX callbacks with file I/O operations.
 */
uint8_t pocsag_log_buffer_count(PCSGLogBuffer* buffer);  // LOG_BUFFER_FUNCTIONS

/**
 * Append a received page to the CSV log (blocking - only call when hardware is idle) - LOG_BUFFER_FUNCTIONS
 * @param ric RIC (Receiver ID) of the page
 * @param message Message content
 * @return true on success, false on failure
 * NOTE: These functions implement a circular buffer for async logging
 * to avoid blocking RX callbacks with file I/O operations.
 */
bool pocsag_pager_log_page(uint32_t ric, const char* message);  // LOG_BUFFER_FUNCTIONS


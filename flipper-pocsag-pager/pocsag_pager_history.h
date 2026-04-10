
#pragma once

#include <math.h>
#include <furi.h>
#include <furi_hal.h>
#include <lib/flipper_format/flipper_format.h>
#include <lib/subghz/types.h>

typedef struct PCSGHistory PCSGHistory;

/** History state add key */
typedef enum {
    PCSGHistoryStateAddKeyUnknown,
    PCSGHistoryStateAddKeyTimeOut,
    PCSGHistoryStateAddKeyNewDada,
    PCSGHistoryStateAddKeyUpdateData,
    PCSGHistoryStateAddKeyOverflow,
} PCSGHistoryStateAddKey;

/** Allocate PCSGHistory
 * 
 * @return PCSGHistory* 
 */
PCSGHistory* pcsg_history_alloc(void);

/** Free PCSGHistory
 * 
 * @param instance - PCSGHistory instance
 */
void pcsg_history_free(PCSGHistory* instance);

/** Clear history
 * 
 * @param instance - PCSGHistory instance
 */
void pcsg_history_reset(PCSGHistory* instance);

/** Get frequency to history[idx]
 * 
 * @param instance  - PCSGHistory instance
 * @param idx       - record index  
 * @return frequency - frequency Hz
 */
uint32_t pcsg_history_get_frequency(PCSGHistory* instance, uint16_t idx);

SubGhzRadioPreset* pcsg_history_get_radio_preset(PCSGHistory* instance, uint16_t idx);

/** Get preset to history[idx]
 * 
 * @param instance  - PCSGHistory instance
 * @param idx       - record index  
 * @return preset   - preset name
 */
const char* pcsg_history_get_preset(PCSGHistory* instance, uint16_t idx);

/** Get history index write 
 * 
 * @param instance  - PCSGHistory instance
 * @return idx      - current record index  
 */
uint16_t pcsg_history_get_item(PCSGHistory* instance);

/** Get type protocol to history[idx] - UNUSED
 *
 * @param instance  - PCSGHistory instance
 * @param idx       - record index
 * @return type      - type protocol
 * NOTE: This function is defined but NOT called anywhere in the codebase
 */
uint8_t pcsg_history_get_type_protocol(PCSGHistory* instance, uint16_t idx);  // UNUSED

/** Get name protocol to history[idx] - UNUSED
 *
 * @param instance  - PCSGHistory instance
 * @param idx       - record index
 * @return name      - const char* name protocol
 * NOTE: This function is defined but NOT called anywhere in the codebase
 */
const char* pcsg_history_get_protocol_name(PCSGHistory* instance, uint16_t idx);  // UNUSED

/** Get string item menu to history[idx] - UNUSED
 *
 * @param instance  - PCSGHistory instance
 * @param output    - FuriString* output
 * @param idx       - record index
 * NOTE: This function is defined but NOT called anywhere in the codebase
 */
void pcsg_history_get_text_item_menu(PCSGHistory* instance, FuriString* output, uint16_t idx);  // UNUSED

/** Get string the remaining number of records to history
 * 
 * @param instance  - PCSGHistory instance
 * @param output    - FuriString* output
 * @return bool - is FUUL
 */
bool pcsg_history_get_text_space_left(PCSGHistory* instance, FuriString* output);

/** Add protocol to history
 * 
 * @param instance  - PCSGHistory instance
 * @param context    - SubGhzProtocolCommon context
 * @param preset    - SubGhzRadioPreset preset
 * @return PCSGHistoryStateAddKey;
 */
PCSGHistoryStateAddKey
    pcsg_history_add_to_history(PCSGHistory* instance, void* context, SubGhzRadioPreset* preset);

/** Get raw data to history[idx] - UNUSED
 *
 * @param instance  - PCSGHistory instance
 * @param idx       - record index
 * @return FlipperFormat* raw data
 * NOTE: This function is defined but NOT called anywhere in the codebase
 */
FlipperFormat* pcsg_history_get_raw_data(PCSGHistory* instance, uint16_t idx);  // UNUSED

/** Get RIC from history entry
 *
 * @param instance  - PCSGHistory instance
 * @param idx       - record index
 * @param ric       - output RIC value
 * @return true on success
 */
bool pcsg_history_get_ric(PCSGHistory* instance, uint16_t idx, uint32_t* ric);

/** Get message from history entry
 *
 * @param instance  - PCSGHistory instance
 * @param idx       - record index
 * @param message   - output message string
 * @return true on success
 */
bool pcsg_history_get_message(PCSGHistory* instance, uint16_t idx, FuriString* message);

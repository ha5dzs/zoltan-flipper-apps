#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <furi.h>

#include <lib/subghz/types.h>
#include <lib/subghz/protocols/base.h>

#include "../helpers/pocsag_pager_types.h"

/**
 * Serialize data PCSGBlockGeneric.
 * @param instance Pointer to a PCSGBlockGeneric instance
 * @param flipper_format Pointer to a FlipperFormat instance
 * @param preset The modulation on which the signal was received, SubGhzRadioPreset
 * @return true On success
 */
SubGhzProtocolStatus pcsg_block_generic_serialize(
    PCSGBlockGeneric* instance,
    FlipperFormat* flipper_format,
    SubGhzRadioPreset* preset);

/**
 * Deserialize data PCSGBlockGeneric.
 * @param instance Pointer to a PCSGBlockGeneric instance
 * @param flipper_format Pointer to a FlipperFormat instance
 * @return true On success
 */
SubGhzProtocolStatus
    pcsg_block_generic_deserialize(PCSGBlockGeneric* instance, FlipperFormat* flipper_format);

/**
 * Extract RIC from decoder base
 * @param decoder_base SubGhzProtocolDecoderBase pointer
 * @param output Output string for RIC
 * @return true on success
 */
bool subghz_decoder_base_get_ric(SubGhzProtocolDecoderBase* decoder_base, FuriString* output);

/**
 * Extract message from decoder base
 * @param decoder_base SubGhzProtocolDecoderBase pointer
 * @param output Output string for message
 * @return true on success
 */
bool subghz_decoder_base_get_message(SubGhzProtocolDecoderBase* decoder_base, FuriString* output);

/**
 * Convert TX preset name to SubGhz preset name
 * @param preset_name TX preset name (e.g., "FM95", "CUSTOM")
 * @param preset_str Output string for SubGhz preset name
 */
void pcsg_block_generic_get_preset_name(const char* preset_name, FuriString* preset_str);

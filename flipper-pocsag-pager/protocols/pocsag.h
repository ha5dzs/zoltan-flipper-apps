#pragma once

#include <lib/subghz/protocols/base.h>
#include <lib/subghz/blocks/const.h>
#include <lib/subghz/blocks/decoder.h>
#include <lib/subghz/blocks/encoder.h>
#include <lib/subghz/blocks/math.h>
#include "../helpers/pocsag_pager_types.h"
#include "pcsg_generic.h"

#define SUBGHZ_PROTOCOL_POCSAG_NAME "POCSAG"

// Baud rate configuration - must match POCSAG_BAUD_RATE in pocsag_pager_app_i.c
// Valid values: 512, 1200, 2400
// The decoder auto-detects the baud rate from incoming signals
#ifndef POCSAG_BAUD_RATE
#define POCSAG_BAUD_RATE 512
#endif

extern const SubGhzProtocol subghz_protocol_pocsag;
extern const SubGhzProtocolEncoder subghz_protocol_pocsag_encoder;

/** Get the RIC from the current POCSAG page being decoded
 * @param context  - SubGhzProtocolDecoderBase context
 * @param ric      - output RIC value
 * @return true if RIC was retrieved, false otherwise
 */
bool subghz_protocol_decoder_pocsag_get_ric(void* context, uint32_t* ric);

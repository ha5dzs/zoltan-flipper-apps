#pragma once

#include <furi.h>
#include <furi_hal.h>

#define PCSG_VERSION_APP FAP_VERSION
#define PCSG_DEVELOPED \
    "@xMasterX & @Shmuma\nProtocol improvements by:\n@htotoo\nIcons by:\n@Svaarich\nAdditional code butchering by:\n@ha5dzs"
#define PCSG_GITHUB "https://github.com/ha5dzs/zoltan-flipper-apps"

// Default values
#define PCSG_DEFAULT_FREQUENCY 433920000  // 433.92 MHz (testing frequency)
#define PCSG_DEFAULT_HOPPING PCSGHopperStateOFF
#define PCSG_DEFAULT_PRESET "FM95"
#define PCSG_DEFAULT_ADDRESS 0

#define PCSG_KEY_FILE_VERSION 1
#define PCSG_KEY_FILE_TYPE "Flipper POCSAG Pager Key File"

// CSV log buffer - holds pages pending to be logged when hardware is idle
#define PCSG_LOG_BUFFER_SIZE 10
#define PCSG_MAX_MESSAGE_LENGTH 256

typedef struct {
    uint32_t ric;
    char message[PCSG_MAX_MESSAGE_LENGTH];
} PCSGLogEntry;

typedef struct {
    PCSGLogEntry entries[PCSG_LOG_BUFFER_SIZE];
    uint8_t head;  // Next write position
    uint8_t count; // Number of entries in buffer
} PCSGLogBuffer;

/** PCSGRxKeyState state */
typedef enum {
    PCSGRxKeyStateIDLE,
    PCSGRxKeyStateBack,
    PCSGRxKeyStateStart,
    PCSGRxKeyStateAddKey,
} PCSGRxKeyState;

/** PCSGHopperState state */
typedef enum {
    PCSGHopperStateOFF,
    PCSGHopperStateRunnig,
    PCSGHopperStatePause,
    PCSGHopperStateRSSITimeOut,
} PCSGHopperState;

/** PCSGLock */
typedef enum {
    PCSGLockOff,
    PCSGLockOn,
} PCSGLock;

typedef enum {
    POCSAGPagerViewVariableItemList,
    POCSAGPagerViewSubmenu,
    POCSAGPagerViewReceiver,
    POCSAGPagerViewReceiverInfo,
    POCSAGPagerViewWidget,
    POCSAGPagerViewNumberInput,
    POCSAGPagerViewTextInput,
} POCSAGPagerView;

/** POCSAGPagerTxRx state */
typedef enum {
    PCSGTxRxStateIDLE,
    PCSGTxRxStateRx,
    PCSGTxRxStateTx,
    PCSGTxRxStateSleep,
} PCSGTxRxState;

/** PCSGBlockGeneric structure for storing decoded message data */
typedef struct PCSGBlockGeneric PCSGBlockGeneric;

struct PCSGBlockGeneric {
    const char* protocol_name;
    FuriString* result_ric;
    FuriString* result_msg;
    uint32_t timestamp;
};

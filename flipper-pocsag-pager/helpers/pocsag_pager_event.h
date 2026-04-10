#pragma once

typedef enum {
    //PCSGCustomEvent
    PCSGCustomEventStartId = 100,

    PCSGCustomEventSceneSettingLock,

    PCSGCustomEventViewReceiverOK,
    PCSGCustomEventViewReceiverConfig,
    PCSGCustomEventViewReceiverBack,
    PCSGCustomEventViewReceiverOffDisplay,
    PCSGCustomEventViewReceiverUnlock,

    // Sender events
    PCSGCustomEventSendRICOK = 200,
    PCSGCustomEventSendMessageOK,
} PCSGCustomEvent;

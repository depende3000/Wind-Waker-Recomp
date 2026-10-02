// Forwarder (phase 2, step 2.3): TWW's dolphin/os/OSAudioSystem.h over Aurora.
// Aurora has no counterpart. The SDK-internal declarations are kept for the names the decomp
// declares; nothing defines them yet (the audio hardware is step 2.6f).
#ifndef TWW_SDK_DOLPHIN_OS_OSAUDIOSYSTEM_H
#define TWW_SDK_DOLPHIN_OS_OSAUDIOSYSTEM_H

#include <dolphin/types.h>
#include "tww_sdk_extras.h"

#ifdef __cplusplus
extern "C" {
#endif

void __OSInitAudioSystem(void);
void __OSStopAudioSystem(void);

#ifdef __cplusplus
}
#endif

#endif

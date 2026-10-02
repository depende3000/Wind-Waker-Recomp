// Forwarder (phase 2, step 2.3): TWW's dolphin/os/OSSync.h over Aurora.
// Aurora has no counterpart. The decomp's file-static SystemCallVector is left out (it belongs to
// the SDK's own source); the two external SDK-internal names are kept.
#ifndef TWW_SDK_DOLPHIN_OS_OSSYNC_H
#define TWW_SDK_DOLPHIN_OS_OSSYNC_H

#include <dolphin/types.h>
#include "tww_sdk_extras.h"

#ifdef __cplusplus
extern "C" {
#endif

void __OSInitSystemCall(void);
void __OSSystemCallVectorEnd(void);

#ifdef __cplusplus
}
#endif

#endif

// Forwarder (phase 2, step 2.3): TWW's dolphin/pad/Padclamp.h over Aurora's <dolphin/pad.h>,
// which declares PADClamp and PADClampCircle. PADClampRegion, the clamp table type, is TWW-only
// (the decomp defines it in both Pad.h and Padclamp.h; here only once, and Pad.h includes this).
#ifndef TWW_SDK_DOLPHIN_PAD_PADCLAMP_H
#define TWW_SDK_DOLPHIN_PAD_PADCLAMP_H

#include <dolphin/pad.h>
#include "tww_sdk_extras.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct PADClampRegion {
    u8 minTrigger;
    u8 maxTrigger;
    s8 minStick;
    s8 maxStick;
    s8 xyStick;
    s8 minSubstick;
    s8 maxSubstick;
    s8 xySubstick;
    s8 radStick;
    s8 radSubstick;
} PADClampRegion;

#ifdef __cplusplus
}
#endif

#endif

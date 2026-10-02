// Forwarder (phase 2, step 2.4): TWW's dolphin/gba/GBA.h over Aurora's <dolphin/gba.h>, which
// declares the same constants, GBACallback and GBA API as the decomp (Aurora has no GBA library;
// JUTGba and the Tingle Tuner need host definitions later). Nothing TWW-only to add. Aurora's
// header includes a bare <types.h>, which native/include/sdk/types.h provides.
#ifndef TWW_SDK_DOLPHIN_GBA_GBA_H
#define TWW_SDK_DOLPHIN_GBA_GBA_H

#include <dolphin/types.h>
#include <dolphin/gba.h>

#endif

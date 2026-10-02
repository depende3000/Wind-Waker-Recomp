// Forwarder (phase 2, step 2.4): TWW's dolphin/dvd/dvderror.h. Aurora lacks this SDK internal; the
// declaration is the decomp's (nothing on the host defines it).
#ifndef TWW_SDK_DOLPHIN_DVD_DVDERROR_H
#define TWW_SDK_DOLPHIN_DVD_DVDERROR_H

#include <dolphin/types.h>

#ifdef __cplusplus
extern "C" {
#endif

void __DVDStoreErrorCode(u32 error);

#ifdef __cplusplus
}
#endif

#endif

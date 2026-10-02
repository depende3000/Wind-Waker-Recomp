// Forwarder (phase 2, step 2.4): TWW's dolphin/dvd/dvdFatal.h. Aurora declares
// DVDSetAutoFatalMessaging in <dolphin/dvd.h> but not this SDK internal; the declaration is the
// decomp's (nothing on the host defines it).
#ifndef TWW_SDK_DOLPHIN_DVD_DVDFATAL_H
#define TWW_SDK_DOLPHIN_DVD_DVDFATAL_H

#include <dolphin/dvd.h>

#ifdef __cplusplus
extern "C" {
#endif

void __DVDPrintFatalMessage(void);

#ifdef __cplusplus
}
#endif

#endif

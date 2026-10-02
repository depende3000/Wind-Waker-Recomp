// Forwarder (phase 2, step 2.4): TWW's dolphin/dvd/dvdfs.h. Aurora declares the DVDFS API in
// <dolphin/dvd.h> but not these SDK internals; the declarations are the decomp's (nothing on the
// host defines them).
#ifndef TWW_SDK_DOLPHIN_DVD_DVDFS_H
#define TWW_SDK_DOLPHIN_DVD_DVDFS_H

#include <dolphin/dvd.h>
#include <dolphin/os/OS.h>

#ifdef __cplusplus
extern "C" {
#endif

extern OSThreadQueue __DVDThreadQueue;
extern u32 __DVDLongFileNameFlag;

void __DVDFSInit(void);

#ifdef __cplusplus
}
#endif

#endif

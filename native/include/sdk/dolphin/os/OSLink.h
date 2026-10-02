// Forwarder (phase 2, step 2.3): TWW's dolphin/os/OSLink.h over Aurora.
// Aurora's <dolphin/os/OSModule.h> has the module structures and OSLink/OSLinkFixed/OSUnlink/
// OSSetStringTable. The decomp defines __OSModuleList and __OSStringTable in the header at fixed
// GameCube addresses (AT_ADDRESS, empty under clang, so a definition in every unit); here they are
// declarations only, and the SDK layer has to define them once (TODO(native phase 3): the REL
// list stays empty when the RELs are linked statically).
#ifndef TWW_SDK_DOLPHIN_OS_OSLINK_H
#define TWW_SDK_DOLPHIN_OS_OSLINK_H

#include <dolphin/os/OSModule.h>
#include <dolphin/os/OSUtil.h>

#ifdef __cplusplus
extern "C" {
#endif

extern OSModuleQueue __OSModuleList;
extern void* __OSStringTable;

void __OSModuleInit(void);

#ifdef __cplusplus
}
#endif

#endif

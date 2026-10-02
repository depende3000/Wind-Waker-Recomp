// tww_sdk: the TWW-specific part of the GameCube SDK over Aurora (native/sdk/README.md).
#ifndef TWW_SDK_SDK_H
#define TWW_SDK_SDK_H

#ifdef __cplusplus
extern "C" {
#endif

// The Aurora commit tww_sdk was built against (TWW_AURORA_COMMIT in native/cmake/Aurora.cmake).
const char* TWWSdkAuroraCommit(void);

#ifdef __cplusplus
}
#endif

#endif // TWW_SDK_SDK_H

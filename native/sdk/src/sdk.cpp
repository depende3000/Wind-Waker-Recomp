// tww_sdk: library-level information. The SDK functions themselves live in the subdirectories of
// native/sdk/src (one per SDK library), added by the phase 2 steps.
#include "tww_sdk/sdk.h"

#ifndef TWW_AURORA_COMMIT_STR
#error "TWW_AURORA_COMMIT_STR must be set by native/cmake/sdk.cmake"
#endif

extern "C" const char* TWWSdkAuroraCommit(void) {
    return TWW_AURORA_COMMIT_STR;
}

// Forwarder (phase 2, step 2.3): TWW's dolphin/mtx/vec.h over Aurora's <dolphin/mtx.h>.
// Aurora's GeoTypes.h has Vec, VecPtr, Point3d, Point3dPtr and S16Vec; the VEC functions are in
// <dolphin/mtx.h>. SVec, the decomp's name for a short vector, is TWW-only.
#ifndef TWW_SDK_DOLPHIN_MTX_VEC_H
#define TWW_SDK_DOLPHIN_MTX_VEC_H

#include <dolphin/mtx.h>
#include "tww_sdk_extras.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SVec {
    s16 x, y, z;
} SVec;

#ifdef __cplusplus
}
#endif

#endif

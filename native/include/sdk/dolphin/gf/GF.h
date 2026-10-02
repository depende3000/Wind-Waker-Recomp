// Forwarder (phase 2, step 2.4): TWW's dolphin/gf/GF.h over Aurora's <dolphin/gf.h> (GFGeometry,
// GFLight, GFPixel, GFTev) and <dolphin/gd/GDBase.h>, plus tww_gf_extras.h for the TWW-only GF
// declarations (GFTransform's functions, GFWriteCPCmd, GFWriteXFCmdHdr, GFSetArray...).
// GFWrite_u32, GFWriteXFCmd and GFWriteBPCmd are Aurora's (same bodies as the decomp's).
#ifndef TWW_SDK_DOLPHIN_GF_GF_H
#define TWW_SDK_DOLPHIN_GF_GF_H

#include <dolphin/gd/GDBase.h>
#include <dolphin/gf.h>
#include <dolphin/gf/GFTransform.h>
#include "tww_gf_extras.h"

#endif

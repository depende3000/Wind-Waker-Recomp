// SDK header check (phase 2, step 2.3; docs/NATIVE_PORT_PHASE2_3.md, decision D2).
//
// One unit that includes every SDK header name the decomp has under native/tww/include/dolphin,
// spelled as the game spells them, plus the top-level game headers that include SDK headers.
// It builds in both header modes (target tww_sdk_header_check):
// - TWW_SDK_HEADERS=decomp: the names resolve to the decomp's own headers;
// - TWW_SDK_HEADERS=aurora: each name resolves to Aurora (names both have) or to a forwarder in
//   native/include/sdk (TWW-only names), and check/check_sdk_shadow.sh fails if any dependency of
//   this unit resolves under native/tww/include/dolphin. That script also checks that every name
//   under native/tww/include/dolphin is listed here.
//
// The names whose forwarders come with step 2.4 are compiled in decomp mode only until then; the
// shadow script reports them as pending.

// What every game unit starts with, and the top-level headers that include SDK headers (a quoted
// include there is looked up next to the header first, i.e. in native/tww/include/dolphin).
#include "global.h"
#include "DynamicLink.h"
#include "weak_bss_3569.h"

// Names forwarded or provided by Aurora.
#include <dolphin/base/PPCArch.h>
#include <dolphin/card.h>
#include <dolphin/dsp.h>
#include <dolphin/gd/GDBase.h>
#include <dolphin/gd/GDGeometry.h>
#include <dolphin/gd/GDLight.h>
#include <dolphin/gf/GFGeometry.h>
#include <dolphin/gf/GFLight.h>
#include <dolphin/gf/GFPixel.h>
#include <dolphin/gf/GFTev.h>
#include <dolphin/gx/GXBump.h>
#include <dolphin/gx/GXDraw.h>
#include <dolphin/gx/GXEnum.h>
#include <dolphin/gx/GXFifo.h>
#include <dolphin/gx/GXGeometry.h>
#include <dolphin/gx/GXPerf.h>
#include <dolphin/gx/GXPixel.h>
#include <dolphin/gx/GXStruct.h>
#include <dolphin/gx/GXTev.h>
#include <dolphin/gx/GXTexture.h>
#include <dolphin/gx/GXTransform.h>
#include <dolphin/mtx/mtx.h>
#include <dolphin/mtx/mtx44.h>
#include <dolphin/mtx/mtxvec.h>
#include <dolphin/mtx/quat.h>
#include <dolphin/mtx/vec.h>
#include <dolphin/os/OS.h>
#include <dolphin/os/OSAlarm.h>
#include <dolphin/os/OSAlloc.h>
#include <dolphin/os/OSArena.h>
#include <dolphin/os/OSAudioSystem.h>
#include <dolphin/os/OSCache.h>
#include <dolphin/os/OSContext.h>
#include <dolphin/os/OSError.h>
#include <dolphin/os/OSFont.h>
#include <dolphin/os/OSInterrupt.h>
#include <dolphin/os/OSLink.h>
#include <dolphin/os/OSMemory.h>
#include <dolphin/os/OSMessage.h>
#include <dolphin/os/OSMutex.h>
#include <dolphin/os/OSReboot.h>
#include <dolphin/os/OSReset.h>
#include <dolphin/os/OSResetSW.h>
#include <dolphin/os/OSRtc.h>
#include <dolphin/os/OSStopwatch.h>
#include <dolphin/os/OSSync.h>
#include <dolphin/os/OSThread.h>
#include <dolphin/os/OSTime.h>
#include <dolphin/os/OSUtil.h>
#include <dolphin/pad/Pad.h>
#include <dolphin/thp.h>
#include <dolphin/types.h>
#include <dolphin/vi/vi.h>

// Names the decomp's own headers cannot compile here, so checked in aurora mode only:
// __start.h uses Metrowerks' __declspec(weak), and Padclamp.h defines PADClampRegion again after
// Pad.h (no game unit includes either; the forwarders compile in both orders).
#if defined(TWW_SDK_HEADERS_AURORA)
#include <dolphin/os/__start.h>
#include <dolphin/pad/Padclamp.h>
#endif

// Names whose forwarders are step 2.4 (TWW_SDK_PENDING_BEGIN/END delimit them for the script).
#if !defined(TWW_SDK_HEADERS_AURORA)
// TWW_SDK_PENDING_BEGIN
#include <dolphin/ai/ai.h>
#include <dolphin/amcstubs/AmcExi2Stubs.h>
#include <dolphin/ar/ar.h>
#include <dolphin/ar/arq.h>
#include <dolphin/db/db.h>
#include <dolphin/dvd/dvd.h>
#include <dolphin/dvd/dvderror.h>
#include <dolphin/dvd/dvdFatal.h>
#include <dolphin/dvd/dvdfs.h>
#include <dolphin/dvd/dvdidutils.h>
#include <dolphin/dvd/dvdlow.h>
#include <dolphin/dvd/dvdqueue.h>
#include <dolphin/dvd/fstload.h>
#include <dolphin/exi/EXIBios.h>
#include <dolphin/exi/EXIUart.h>
#include <dolphin/gba/GBA.h>
#include <dolphin/gba/GBAPriv.h>
#include <dolphin/gf/GF.h>
#include <dolphin/gf/GFTransform.h>
#include <dolphin/gx/GX.h>
#include <dolphin/gx/GXAttr.h>
#include <dolphin/gx/GXDisplayList.h>
#include <dolphin/gx/GXFrameBuf.h>
#include <dolphin/gx/GXInit.h>
#include <dolphin/gx/GXLight.h>
#include <dolphin/gx/GXMisc.h>
#include <dolphin/si/SIBios.h>
// TWW_SDK_PENDING_END
#endif

#if defined(TWW_SDK_HEADERS_AURORA)
// The TWW-only names the force-included tww_sdk_extras.h restores.
static_assert(sizeof(uint) == 4, "uint");
static_assert(FLOAT_MAX > 3.4e38f && FLOAT_MIN < 1.2e-38f, "FLOAT_MIN/FLOAT_MAX");
#endif

int tww_sdk_header_check(const u8* p) {
    u32 v = READU32_BE(p, 0)
    Mtx m;
    MTXIdentity(m);
    MtxP mp = m;
    return (int)(v + (u32)mp[0][0]) + (OSGetConsoleType() != 0);
}

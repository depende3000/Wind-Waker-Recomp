// tww_sdk_smoke: the tests of step 2.6f, the silent audio hardware and the MSL extras:
// "audio" (AI register model, DSP mailbox and task list, DTK playlist over Aurora's DVD stream
// commands; nothing may block) and "msl" (stricmp, strnicmp).
//
// The AI names come through the dolphin/ai/ai.h forwarder, as the game sees them.
#include "smoke.h"

#include <dolphin/ai/ai.h>
#include <dolphin/dsp.h>
#ifndef TWW_SDK_SMOKE_NO_DVD
#include <dolphin/dtk.h>
#endif
#include <dolphin/os.h>

#include <cstdint>

extern "C" {
// Declared by the decomp's dolphin/dsp.h and MSL's extras.h; Aurora's headers lack them.
extern DSPTaskInfo* __DSP_first_task;
extern DSPTaskInfo* __DSP_last_task;
extern DSPTaskInfo* __DSP_curr_task;
void __DSP_remove_task(DSPTaskInfo* task);
int stricmp(const char* str1, const char* str2);
int strnicmp(const char* str1, const char* str2, int n);
}

namespace {

void DmaCallback() {}
void StreamCallback(u32) {}

} // namespace

TWW_SMOKE_TEST(audio) {
    OSInit();

    // AI: before AIInit the DSP rate reads 48 kHz (the register's reset value); AIInit sets 32 kHz.
    TWW_SMOKE_CHECK(AIGetDSPSampleRate() == AI_SAMPLERATE_48KHZ);
    AIInit(nullptr);
    TWW_SMOKE_CHECK(AICheckInit());
    TWW_SMOKE_CHECK(AIGetDSPSampleRate() == AI_SAMPLERATE_32KHZ);
    TWW_SMOKE_CHECK(AIGetStreamSampleRate() == AI_SAMPLERATE_48KHZ);
    AISetDSPSampleRate(AI_SAMPLERATE_48KHZ);
    TWW_SMOKE_CHECK(AIGetDSPSampleRate() == AI_SAMPLERATE_48KHZ);
    AISetStreamSampleRate(AI_SAMPLERATE_32KHZ); // ignored, as by the SDK
    TWW_SMOKE_CHECK(AIGetStreamSampleRate() == AI_SAMPLERATE_48KHZ);
    TWW_SMOKE_CHECK(AIRegisterDMACallback(DmaCallback) == nullptr);
    TWW_SMOKE_CHECK(AIRegisterDMACallback(nullptr) == DmaCallback);
    TWW_SMOKE_CHECK(AIRegisterStreamCallback(StreamCallback) == nullptr);
    TWW_SMOKE_CHECK(AIRegisterStreamCallback(nullptr) == StreamCallback);
    AIInitDMA(0x1000, 0x280);
    TWW_SMOKE_CHECK(AIGetDMAStartAddr() == 0x1000 && AIGetDMALength() == 0x280);
    TWW_SMOKE_CHECK(!AIGetDMAEnableFlag());
    AIStartDMA();
    TWW_SMOKE_CHECK(AIGetDMAEnableFlag() && AIGetDMABytesLeft() == 0);
    AIStopDMA();
    TWW_SMOKE_CHECK(!AIGetDMAEnableFlag());
    AISetStreamVolLeft(0x12);
    AISetStreamVolRight(0x34);
    TWW_SMOKE_CHECK(AIGetStreamVolLeft() == 0x12 && AIGetStreamVolRight() == 0x34);
    AISetStreamPlayState(AI_STREAM_START);
    TWW_SMOKE_CHECK(AIGetStreamPlayState() == AI_STREAM_START);
    AISetStreamPlayState(AI_STREAM_STOP);
    TWW_SMOKE_CHECK(AIGetStreamSampleCount() == 0);

    // DSP: the mailbox to the DSP is always free, nothing comes back.
    DSPInit();
    TWW_SMOKE_CHECK(DSPCheckInit());
    DSPSendMailToDSP(0xCDD10001);
    TWW_SMOKE_CHECK(DSPCheckMailToDSP() == 0);
    TWW_SMOKE_CHECK(DSPReadCPUToDSPMbox() == 0xCDD10001);
    TWW_SMOKE_CHECK(DSPCheckMailFromDSP() == 0);

    // Task list in priority order; booting the first task returns without waiting for the DSP.
    static DSPTaskInfo a, b, c;
    a.priority = 10;
    b.priority = 5;
    c.priority = 20;
    TWW_SMOKE_CHECK(DSPAddTask(&a) == &a);
    TWW_SMOKE_CHECK(__DSP_first_task == &a && __DSP_curr_task == &a);
    DSPAddTask(&b);
    DSPAddTask(&c);
    TWW_SMOKE_CHECK(__DSP_first_task == &b && b.next == &a && a.next == &c);
    TWW_SMOKE_CHECK(__DSP_last_task == &c && c.prev == &a && a.prev == &b);
    TWW_SMOKE_CHECK(a.state == 0 && a.flags == 1);
    __DSP_remove_task(&a);
    TWW_SMOKE_CHECK(b.next == &c && c.prev == &b && a.state == 3);
    __DSP_remove_task(&b);
    __DSP_remove_task(&c);
    TWW_SMOKE_CHECK(__DSP_first_task == nullptr && __DSP_last_task == nullptr);
    TWW_SMOKE_CHECK(DSPCancelTask(&a) == &a && (a.flags & 2) != 0);

#ifndef TWW_SDK_SMOKE_NO_DVD // the TSan build leaves DTK.cpp out (no aurora::dvd; sdk.cmake)
    // DTK: an empty playlist runs and stops at once; volume and repeat mode read back.
    DTKInit();
    TWW_SMOKE_CHECK(DTKGetState() == DTK_STATE_STOP && DTKGetCurrentTrack() == nullptr);
    DTKSetVolume(0x40, 0x80);
    TWW_SMOKE_CHECK(DTKGetVolume() == 0x4080);
    DTKSetRepeatMode(DTK_MODE_ALLREPEAT);
    TWW_SMOKE_CHECK(DTKGetRepeatMode() == DTK_MODE_ALLREPEAT);
    TWW_SMOKE_CHECK(DTKSetState(DTK_STATE_RUN) == 1 && DTKGetState() == DTK_STATE_RUN);
    TWW_SMOKE_CHECK(DTKFlushTracks(nullptr) == 1 && DTKGetState() == DTK_STATE_STOP);
    TWW_SMOKE_CHECK(DTKNextTrack() == 0); // no track
    DTKShutdown();                         // must not hang
    TWW_SMOKE_CHECK(DTKGetPosition() == 0);
#endif

    AIReset();
    TWW_SMOKE_CHECK(!AICheckInit());
    return true;
}

TWW_SMOKE_TEST(msl) {
    TWW_SMOKE_CHECK(stricmp("JKRArchive", "jkrarchive") == 0);
    TWW_SMOKE_CHECK(stricmp("abc", "ABD") == -1);
    TWW_SMOKE_CHECK(stricmp("abd", "ABC") == 1);
    TWW_SMOKE_CHECK(stricmp("ab", "abc") == -1);
    TWW_SMOKE_CHECK(stricmp("", "") == 0);
    TWW_SMOKE_CHECK(stricmp("\xe9", "a") == 1); // bytes >= 0x80 compare unsigned, as on PowerPC
    TWW_SMOKE_CHECK(strnicmp("Stage.ARC", "stage.bmd", 6) == 0);
    TWW_SMOKE_CHECK(strnicmp("Stage.ARC", "stage.bmd", 7) == -1);
    TWW_SMOKE_CHECK(strnicmp("abc", "ABC", 10) == 0);
    TWW_SMOKE_CHECK(strnicmp("abc", "abd", 0) == 0);
    return true;
}

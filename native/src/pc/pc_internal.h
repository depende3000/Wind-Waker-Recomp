// Shared state of the run harness (native/src/pc/pc_*.cpp, docs/NATIVE_PORT_PHASE4_6.md step 6.0).
// Public API: native/include/pc/pc_harness.h.
#pragma once

#include "pc/pc_harness.h"

#include <cstdint>

namespace pc {

struct Config {
    const char* disc = nullptr;      // TWW_DISC
    const char* smoke = nullptr;     // TWW_SMOKE
    const char* milestone = nullptr; // TWW_MILESTONE
    const char* trace = nullptr;     // TWW_TRACE
    const char* runDir = nullptr;    // TWW_RUN_DIR
    double timeoutS = 0;             // TWW_TIMEOUT_S, 0 = off
    double stallS = 0;               // TWW_STALL_S, 0 = off
    unsigned int frames = 0;         // TWW_FRAMES, 0 = off
    bool uncapped = false;           // TWW_UNCAPPED
    bool audio = true;               // TWW_AUDIO (off/0 -> false)
};

extern Config gConfig;

// Milliseconds since pc_harness_init (monotonic).
uint64_t elapsedMs();
uint64_t monotonicNs();

// pc_milestone.cpp
bool isKnownMilestone(const char* name);
void printMilestones(int fd);

// pc_crash.cpp
void installCrashHandler();
// Writes "scene=... frame=... retrace=... ms=... last_res=..." (one line, no prefix) to fd.
void writeState(int fd);
// Formats into a fixed buffer and writes to fd; usable from the crash handler.
void writef(int fd, const char* fmt, ...) __attribute__((format(printf, 2, 3)));
// Opens <TWW_RUN_DIR>/<name> for writing (truncated), or -1 without a run directory.
int openRunFile(const char* name);
// Every thread but the caller: suspended, then a frame-pointer backtrace of each (stall report).
void dumpAllThreads(int fd);

// pc_disc.cpp: 0 if TWW_DISC is a readable GZLE01 revision 0 image, else prints why and returns
// PC_EXIT_DISC.
int checkDisc();

// pc_smoke.cpp: runs TWW_SMOKE if it is a test that runs before the SDK (it never returns then);
// exits PC_EXIT_USAGE for an unknown name; returns for no TWW_SMOKE.
void runEarlySmoke();
bool isKnownSmoke(const char* name);
void printSmokes(int fd);

// pc_watchdog.cpp
void startWatchdog();

} // namespace pc

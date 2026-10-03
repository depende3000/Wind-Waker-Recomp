// Run harness of the native executable tww: environment, start-up order and exit
// (docs/NATIVE_PORT_PHASE4_6.md, step 6.0). Public API in native/include/pc/pc_harness.h.
#include "pc_internal.h"
#include "pc/pc_aspect.h"

#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <unistd.h>

#if defined(__SWITCH__)
#include "tww_switch.h"
#endif
#if defined(__APPLE__)
#include <pthread.h>
#include <pthread/qos.h>
#endif

namespace pc {

Config gConfig;

namespace {

uint64_t sStartNs = 0;
std::atomic<int> sExiting{0};

const char* envString(const char* name) {
    const char* v = getenv(name);
    return (v != nullptr && v[0] != '\0') ? v : nullptr;
}

double envSeconds(const char* name) {
    const char* v = envString(name);
    if (v == nullptr) {
        return 0;
    }
    char* end = nullptr;
    errno = 0;
    double d = strtod(v, &end);
    if (errno != 0 || end == v || *end != '\0' || d < 0) {
        writef(STDERR_FILENO, "[tww] %s=\"%s\" is not a number of seconds\n", name, v);
        pc_exit(PC_EXIT_USAGE);
    }
    return d;
}

unsigned int envCount(const char* name) {
    const char* v = envString(name);
    if (v == nullptr) {
        return 0;
    }
    char* end = nullptr;
    errno = 0;
    unsigned long n = strtoul(v, &end, 10);
    if (errno != 0 || end == v || *end != '\0' || n > 0xFFFFFFFFul) {
        writef(STDERR_FILENO, "[tww] %s=\"%s\" is not a count\n", name, v);
        pc_exit(PC_EXIT_USAGE);
    }
    return (unsigned int)n;
}

// Step 6.7: the game thread (the process main thread, which runs main01) at QoS USER_INTERACTIVE,
// so macOS keeps it on the P-cores. Threads it creates later (DVD, JAudio, Aurora's workers)
// inherit that class unless they set their own.
void setGameThreadQos() {
#if defined(__APPLE__)
    const qos_class_t before = qos_class_self();
    const int err = pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
    writef(STDERR_FILENO, "[tww] game thread: QoS 0x%x -> 0x%x%s\n", (unsigned int)before,
           (unsigned int)qos_class_self(), err == 0 ? " (user-interactive)" : " (refused)");
#endif
}

bool envFlag(const char* name, bool fallback) {
    const char* v = envString(name);
    if (v == nullptr) {
        return fallback;
    }
    return !(strcmp(v, "0") == 0 || strcmp(v, "off") == 0 || strcmp(v, "false") == 0 ||
             strcmp(v, "no") == 0);
}

} // namespace

void waitForever() {
    for (;;) {
#if defined(__SWITCH__)
        usleep(1000 * 1000);
#else
        pause();
#endif
    }
}

uint64_t monotonicNs() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

uint64_t elapsedMs() {
    return (monotonicNs() - sStartNs) / 1000000ull;
}

} // namespace pc

using namespace pc;

extern "C" {

void pc_harness_init(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    sStartNs = monotonicNs();

    // Line-buffered stdout: the game reports with printf/OSReport into a pipe or file; a watchdog
    // or crash exit must not lose the last lines in a full buffer.
    setvbuf(stdout, nullptr, _IOLBF, 0);

    gConfig.disc = envString("TWW_DISC");
    gConfig.smoke = envString("TWW_SMOKE");
    gConfig.milestone = envString("TWW_MILESTONE");
    gConfig.trace = envString("TWW_TRACE");
    gConfig.runDir = envString("TWW_RUN_DIR");
    gConfig.input = envString("TWW_INPUT");
    gConfig.bootStage = envString("TWW_BOOT_STAGE");
    gConfig.timeoutS = envSeconds("TWW_TIMEOUT_S");
    gConfig.stallS = envSeconds("TWW_STALL_S");
    gConfig.frames = envCount("TWW_FRAMES");
    gConfig.uncapped = envFlag("TWW_UNCAPPED", false);
    gConfig.audio = envFlag("TWW_AUDIO", true);
    gConfig.perfEvery = envCount("TWW_PERF_EVERY");
    gConfig.perfPath = envString("TWW_PERF");
    pc_aspect_init();
    gConfig.hitchMs = envCount("TWW_HITCH_MS");
    gConfig.heapCheckEvery = envCount("TWW_HEAP_CHECK");
    gConfig.fpsOverlay = envFlag("TWW_FPS_OVERLAY", false);

    writef(STDERR_FILENO,
           "[tww] harness: smoke=%s milestone=%s timeout=%gs stall=%gs frames=%u trace=%s "
           "uncapped=%d audio=%s input=%s boot_stage=%s disc=%s\n",
           gConfig.smoke ? gConfig.smoke : "-", gConfig.milestone ? gConfig.milestone : "-",
           gConfig.timeoutS, gConfig.stallS, gConfig.frames, gConfig.trace ? gConfig.trace : "-",
           gConfig.uncapped ? 1 : 0, gConfig.audio ? "on" : "off",
           gConfig.input ? gConfig.input : "-", gConfig.bootStage ? gConfig.bootStage : "-",
           gConfig.disc ? gConfig.disc : "-");

    if (gConfig.perfEvery != 0) {
        writef(STDERR_FILENO, "[tww] perf: game-thread frame times every %u frames (TWW_PERF_EVERY)\n",
               gConfig.perfEvery);
    }
#if defined(TWW_PERF_BUILD)
    writef(STDERR_FILENO, "[tww] perf: TWW_PERF_BUILD variant (-O2, generic ARMv8.0, no Tracy)\n");
#endif
    setGameThreadQos();

    if (gConfig.milestone != nullptr && !isKnownMilestone(gConfig.milestone)) {
        writef(STDERR_FILENO, "[tww] unknown TWW_MILESTONE \"%s\"; known:", gConfig.milestone);
        printMilestones(STDERR_FILENO);
        pc_exit(PC_EXIT_USAGE);
    }
    if (gConfig.smoke != nullptr && !isKnownSmoke(gConfig.smoke)) {
        writef(STDERR_FILENO, "[tww] unknown TWW_SMOKE \"%s\"; known:", gConfig.smoke);
        printSmokes(STDERR_FILENO);
        pc_exit(PC_EXIT_USAGE);
    }

    loadInput();
    loadBootStage();
    // A captured frame must show every draw, as the console does: Aurora's default skips a draw
    // until its pipeline is compiled, so what a shot shows would depend on how busy the machine
    // is (render audit A3: Orca's text missing in parallel boot-sweep runs).
    gConfig.syncPipelines = envFlag("TWW_SYNC_PIPELINES", loadShots());
    perfOpen();

    installCrashHandler();
    startWatchdog();

    // Smoke tests that run before any SDK call (static-init, the harness self-tests) end here.
    runEarlySmoke();

    int disc = checkDisc();
    if (disc != 0) {
        pc_exit(disc);
    }

    // Smoke tests that need the disc but not the game (disc-ls) end here.
    runDiscSmoke();
}

const char* pc_env_disc(void) {
    return gConfig.disc;
}

int pc_env_uncapped(void) {
    return gConfig.uncapped ? 1 : 0;
}

int pc_env_audio(void) {
    return gConfig.audio ? 1 : 0;
}

void pc_exit(int code) {
    int expected = 0;
    if (!sExiting.compare_exchange_strong(expected, 1)) {
        // Another thread is already leaving (a crash during a watchdog exit, two crashes...):
        // let it finish with its own code.
        waitForever();
    }
    // _Exit, not exit: exit runs the game's static destructors, which the GameCube never ran and
    // which assume a booted game (~dComIfG_inf_c reaches dVibration_c::Kill, which stops the motor
    // of a JUTGamePad that does not exist; found in step 3.9).
    // TODO(native phase 6): decide how the PC executable exits once the game boots (quit event).
    // The TWW_PERF rows still buffered (step 6.7).
    perfFlush();
    // Flush stdio without blocking on a lock some other (now frozen) thread may hold.
    FILE* streams[2] = {stdout, stderr};
    for (FILE* f : streams) {
        if (ftrylockfile(f) == 0) {
            fflush(f);
            funlockfile(f);
        }
    }
#if defined(__SWITCH__)
    // Writes the logs out (SD card, USB) and ends the process (switch/native/source).
    tww_switch_exit(code);
#else
    _Exit(code);
#endif
}

} // extern "C"

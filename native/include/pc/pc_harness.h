/*
 * pc_harness.h - the run harness of the native executable tww (docs/NATIVE_PORT_PHASE4_6.md,
 * step 6.0). Implemented in native/src/pc/pc_*.cpp, linked into tww (and into the link census
 * bundle) as the static library tww_pc. Only TARGET_PC code calls it.
 *
 * Environment (read once by pc_harness_init):
 *   TWW_DISC       path of the GZLE01 disc image (required unless the smoke test needs none)
 *   TWW_SMOKE      run one smoke test instead of the game (static-init, crash-test, ...)
 *   TWW_MILESTONE  exit 0 as soon as this milestone is logged (static-init, aurora-up, ...)
 *   TWW_TIMEOUT_S  in-process watchdog: exit 10 after this many seconds (0 or unset: off)
 *   TWW_STALL_S    exit 11 when the game frame counter is frozen this long (0 or unset: off)
 *   TWW_TRACE      comma list of trace channels: res, scene (or all)
 *   TWW_UNCAPPED   1: no frame pacing (used from step 6.2)
 *   TWW_AUDIO      off: audio stays silent (used from step 6.1)
 *   TWW_FRAMES     exit 0 after this many game frames
 *   TWW_RUN_DIR    directory for backtrace.txt / stall.txt (set by native/tools/tww_run.sh)
 *
 * Exit codes: see PC_EXIT_* below.
 */
#ifndef PC_HARNESS_H
#define PC_HARNESS_H

#ifdef __cplusplus
extern "C" {
#endif

enum {
    PC_EXIT_REACHED = 0,       /* milestone reached, smoke test passed, TWW_FRAMES done */
    PC_EXIT_CHECK_FAILED = 1,  /* a smoke test ran to its end and found errors */
    PC_EXIT_USAGE = 2,         /* unknown TWW_SMOKE / TWW_MILESTONE, malformed number */
    PC_EXIT_TIMEOUT = 10,      /* TWW_TIMEOUT_S elapsed */
    PC_EXIT_STALL = 11,        /* frame counter frozen for TWW_STALL_S */
    PC_EXIT_PANIC = 12,        /* OSPanic (JUT_ASSERT ends there too) */
    PC_EXIT_SIGNAL = 13,       /* fatal signal caught by the crash handler */
    PC_EXIT_DISC = 14,         /* TWW_DISC missing, unreadable or not GZLE01 revision 0 */
};

/* Called first thing in main: reads the environment, installs the crash handler and the
   watchdog, runs a TWW_SMOKE test that needs no SDK (and exits), then checks the disc
   (exit 14 on failure). Returns only when the game should boot. */
void pc_harness_init(int argc, char* argv[]);

/* Logs "[tww] MILESTONE <name> frame= retrace= ms=" and exits 0 if <name> is TWW_MILESTONE. */
void pc_milestone(const char* name);

/* One game frame done (called by the frame loop, step 6.2). Feeds the stall watchdog and
   TWW_FRAMES. */
void pc_frame_tick(void);
unsigned int pc_frame_count(void);

/* Trace channels (TWW_TRACE) and the state the crash handler prints. */
int pc_trace_enabled(const char* channel);
void pc_trace_scene(int procName);
void pc_trace_resource(const char* path, int entryNum);

/* Options for later steps. */
const char* pc_env_disc(void);
int pc_env_uncapped(void);
int pc_env_audio(void);

/* OSPanic on PC: prints the state and the host backtrace, exits 12. */
__attribute__((noreturn)) void pc_panic(const char* file, int line);

/* Flushes what it can (without blocking on a stdio lock another thread holds) and _Exit(code).
   Only the first caller exits; a concurrent second caller blocks forever. */
__attribute__((noreturn)) void pc_exit(int code);

#ifdef __cplusplus
}
#endif

#endif /* PC_HARNESS_H */

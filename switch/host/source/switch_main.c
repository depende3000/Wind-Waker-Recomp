// Switch entry point for the headless milestone: prepares the SD-card paths
// and environment main.c expects, sends stdout/stderr to the screen (libnx's
// text console), a log on the SD card and the live USB log, runs the host,
// and waits for + once it returns.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/iosupport.h>
#include <sys/stat.h>
#include <switch.h>

#include "usb_log.h"

#if defined(BLUEWAKE_SWITCH_AURORA)
#include <EGL/egl.h>
#endif

#define DATA_ROOT "sdmc:/switch/wind-waker-recomp"

// With Aurora the game owns the screen (Dawn presents to the default window),
// so the text console is off.
#if defined(BLUEWAKE_SWITCH_AURORA)
#define SHOW_CONSOLE 0
#define RENDERER "aurora"
#else
#define SHOW_CONSOLE 1
#define RENDERER "headless"
#endif

int bluewake_host_main(int argc, char** argv);

static FILE* g_log;
static const devoptab_t* g_console;  // libnx's console device, saved before the tee
static Mutex g_output_lock;           // the host and the DSP print from several threads
static u64 g_console_presented;       // tick of the last consoleUpdate

// Presents the console at most ten times a second: each update queues a
// framebuffer, and the guest must not wait on the display.
static void present_console(bool force) {
    const u64 now = armGetSystemTick();
    if (force || armTicksToNs(now - g_console_presented) >= 100000000ULL) {
        consoleUpdate(NULL);
        g_console_presented = now;
    }
}

static ssize_t tee_write(struct _reent* r, void* fd, const char* data, size_t size) {
    mutexLock(&g_output_lock);
    if (g_console != NULL && g_console->write_r != NULL) {
        g_console->write_r(r, fd, data, size);
        present_console(false);
    }
    if (g_log != NULL) {
        fwrite(data, 1, size, g_log);
        fflush(g_log);
    }
    usb_log_write(data, size);
    mutexUnlock(&g_output_lock);
    return (ssize_t)size;
}

static const devoptab_t g_tee = {
    .name = "tee",
    .write_r = tee_write,
};

// Sets name to DATA_ROOT/value unless the environment already has it.
static void default_path(const char* name, const char* value) {
    char path[512];
    snprintf(path, sizeof path, "%s/%s", DATA_ROOT, value);
    setenv(name, path, 0);
}

// Applies DATA_ROOT/env.txt: one NAME=value per line, # for comments. Set
// before the defaults below, so it overrides them, and lets a run be
// configured by copying one small file instead of rebuilding the NRO.
static void load_env_file(void) {
    FILE* file = fopen(DATA_ROOT "/env.txt", "r");
    if (file == NULL)
        return;
    char line[512];
    while (fgets(line, sizeof line, file) != NULL) {
        line[strcspn(line, "\r\n")] = '\0';
        char* equals = strchr(line, '=');
        if (line[0] == '#' || line[0] == '\0' || equals == NULL)
            continue;
        *equals = '\0';
        setenv(line, equals + 1, 1);
        fprintf(stderr, "[switch] env.txt: %s=%s\n", line, equals + 1);
    }
    fclose(file);
}

static bool file_exists(const char* path) {
    struct stat info;
    return stat(path, &info) == 0;
}

int main(int argc, char** argv) {
    (void)argc;
    mkdir(DATA_ROOT, 0777);
    mkdir(DATA_ROOT "/states", 0777);
    g_log = fopen(DATA_ROOT "/host.log", "w");
    const bool usb = usb_log_start();
    mutexInit(&g_output_lock);
    if (SHOW_CONSOLE) {
        consoleInit(NULL);
        g_console = devoptab_list[STD_OUT];
    }
    devoptab_list[STD_OUT] = &g_tee;
    devoptab_list[STD_ERR] = &g_tee;
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    if (SHOW_CONSOLE)
        fprintf(stderr, "Wind Waker Recomp for Switch - headless build: no picture, sound or\n"
                        "controls yet. The game runs for about a minute and logs its progress.\n\n");
    fprintf(stderr, "[switch] host starting; usb live log=%s, log=%s/host.log\n",
            usb ? "started" : "unavailable", DATA_ROOT);

    load_env_file();
    setenv("BLUEWAKE_ROOT", DATA_ROOT, 0);
    // One line a second of wall time with the guest's retrace rate (60 is
    // full speed).
    setenv("BLUEWAKE_PERF_LOG", "1", 0);
    setenv("BLUEWAKE_RENDERER", RENDERER, 0);
    // Aurora's shader and pipeline caches (sqlite). No "sdmc:" device prefix:
    // sqlite takes a path not starting with '/' as relative.
    setenv("DOL_AURORA_CACHE_DIR", "/switch/wind-waker-recomp/cache", 0);
    mkdir(DATA_ROOT "/cache", 0777);
#if defined(BLUEWAKE_SWITCH_AURORA)
    // First-run settings for the Switch: the game's 4:3 picture letterboxed
    // in the 16:9 screen, rendered at its own 480 lines, no in-between frames,
    // and the frame rate shown. Each can be overridden in the environment.
    setenv("DOL_AURORA_ASPECT_FIT", "1", 0);
    setenv("DOL_AURORA_RENDER_SCALE", "1", 0);
    setenv("DOL_AURORA_FRAME_INTERP", "0", 0);
    setenv("DOL_AURORA_SHOW_FPS", "1", 0);
#else
    setenv("BLUEWAKE_LIVE_PAD", "0", 0);
    // A bounded first run: about one minute of guest time at 60 retraces/s.
    setenv("BLUEWAKE_MAX_RETRACES", "3600", 0);
#endif
    default_path("BLUEWAKE_DOL", "game/main.dol");
    default_path("BLUEWAKE_RELS_DIR", "game/rels");
    default_path("BLUEWAKE_DISC", "GZLE01.iso");
    default_path("BLUEWAKE_DSP_IROM", "dsp_rom.bin");
    default_path("BLUEWAKE_DSP_COEF", "dsp_coef.bin");
    default_path("BLUEWAKE_CARD_PATH", "GZLE01.card");
    default_path("BLUEWAKE_STATE_DIR", "states");

    const char* required[] = {"BLUEWAKE_DOL", "BLUEWAKE_DISC", "BLUEWAKE_DSP_IROM",
                              "BLUEWAKE_DSP_COEF"};
    bool ready = true;
    for (size_t i = 0; i < sizeof required / sizeof required[0]; ++i) {
        const char* path = getenv(required[i]);
        if (!file_exists(path)) {
            fprintf(stderr, "[switch] missing %s: %s\n", required[i], path);
            ready = false;
        }
    }

    int status = 1;
    if (ready) {
        char* host_argv[] = {argv != NULL && argv[0] != NULL ? argv[0] : "bluewake", NULL};
        status = bluewake_host_main(1, host_argv);
    }
    fprintf(stderr, "[switch] host returned %d; press + to exit\n", status);

    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);
    while (appletMainLoop()) {
        padUpdate(&pad);
        if ((padGetButtonsDown(&pad) & HidNpadButton_Plus) != 0)
            break;
        if (SHOW_CONSOLE) {
            mutexLock(&g_output_lock);
            present_console(true);
            mutexUnlock(&g_output_lock);
        }
        svcSleepThread(16000000ULL);
    }
#if defined(BLUEWAKE_SWITCH_AURORA)
    // Mesa's EGL display outlives Dawn: without terminating it, the Homebrew Menu
    // that hbloader loads next into this process crashed on every exit
    // (nx-hbmenu + 0xf6b34, Atmosphère 2168-0002).
    eglTerminate(eglGetDisplay(EGL_DEFAULT_DISPLAY));
    eglReleaseThread();
#endif
    usb_log_stop(2000);
    if (SHOW_CONSOLE) {
        devoptab_list[STD_OUT] = g_console;
        devoptab_list[STD_ERR] = g_console;
        consoleExit(NULL);
    }
    if (g_log != NULL)
        fclose(g_log);
    return status;
}

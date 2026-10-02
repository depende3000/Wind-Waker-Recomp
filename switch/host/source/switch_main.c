// Switch entry point for the headless milestone: prepares the SD-card paths
// and environment main.c expects, sends stdout/stderr to a log on the SD card
// and to the live USB log, runs the host, and waits for + once it returns.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/iosupport.h>
#include <sys/stat.h>
#include <switch.h>

#include "usb_log.h"

#define DATA_ROOT "sdmc:/switch/wind-waker-recomp"

int bluewake_host_main(int argc, char** argv);

static FILE* g_log;

static ssize_t tee_write(struct _reent* r, void* fd, const char* data, size_t size) {
    (void)r;
    (void)fd;
    if (g_log != NULL) {
        fwrite(data, 1, size, g_log);
        fflush(g_log);
    }
    usb_log_write(data, size);
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
    devoptab_list[STD_OUT] = &g_tee;
    devoptab_list[STD_ERR] = &g_tee;
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    fprintf(stderr, "[switch] host starting; usb live log=%s, log=%s/host.log\n",
            usb ? "started" : "unavailable", DATA_ROOT);

    setenv("BLUEWAKE_ROOT", DATA_ROOT, 0);
    setenv("BLUEWAKE_RENDERER", "headless", 0);
    setenv("BLUEWAKE_LIVE_PAD", "0", 0);
    // A bounded first run: about one minute of guest time at 60 retraces/s.
    setenv("BLUEWAKE_MAX_RETRACES", "3600", 0);
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
        svcSleepThread(16000000ULL);
    }
    usb_log_stop(2000);
    if (g_log != NULL)
        fclose(g_log);
    return status;
}

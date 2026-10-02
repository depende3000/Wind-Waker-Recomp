#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/stat.h>

#include <switch.h>

#define BLUEWAKE_DATA_DIR "sdmc:/switch/wind-waker-recomp"
#define BLUEWAKE_LOG_PATH BLUEWAKE_DATA_DIR "/boot-probe.log"

static bool ensure_directory(const char* path) {
    if (mkdir(path, 0777) == 0)
        return true;
    if (errno != EEXIST)
        return false;

    struct stat info;
    return stat(path, &info) == 0 && S_ISDIR(info.st_mode);
}

static bool prepare_probe_log(FILE** log_file) {
    if (!ensure_directory("sdmc:/switch") ||
        !ensure_directory(BLUEWAKE_DATA_DIR))
        return false;

    *log_file = fopen(BLUEWAKE_LOG_PATH, "a");
    if (*log_file == NULL)
        return false;

    fputs("[probe] started\n", *log_file);
    fflush(*log_file);
    return true;
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    consoleInit(NULL);
    consoleClear();
    printf("Wind Waker Recomp - Switch bootstrap\n");
    printf("This probe is not the game or its renderer.\n\n");

    // libnx mounts sdmc automatically for normal NRO applications. Calling
    // fsdevMountSdmc() again tries to register the same device and libnx maps
    // the duplicate-device failure to LibnxError_OutOfMemory (0x559).
    const bool sd_mounted = fsdevGetDeviceFileSystem("sdmc") != NULL;
    printf("SD device: %s\n", sd_mounted ? "available (libnx auto-mount)"
                                         : "unavailable");

    FILE* log_file = NULL;
    const bool log_ready = sd_mounted && prepare_probe_log(&log_file);
    printf("Probe log: %s\n", log_ready ? BLUEWAKE_LOG_PATH : "unavailable");
    printf("Display: libnx framebuffer console initialized\n");
    printf("\nPress + to exit.\n");

    if (log_file != NULL) {
        fprintf(log_file, "[probe] sd_available=%u log_ready=%u\n",
            sd_mounted ? 1u : 0u, log_ready ? 1u : 0u);
        fflush(log_file);
    }

    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);

    while (appletMainLoop()) {
        padUpdate(&pad);
        const u64 pressed = padGetButtonsDown(&pad);
        consoleUpdate(NULL);
        if ((pressed & HidNpadButton_Plus) != 0)
            break;
    }

    if (log_file != NULL) {
        fputs("[probe] stopped\n", log_file);
        fclose(log_file);
    }

    consoleExit(NULL);
    return sd_mounted && log_ready ? 0 : 1;
}
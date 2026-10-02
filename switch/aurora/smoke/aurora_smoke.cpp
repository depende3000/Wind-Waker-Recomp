// Aurora smoke test for the Switch: the renderer stack without the game.
// Starts Aurora (the SDL 3 shim's window, Dawn's OpenGL ES device with
// gl_defer), presents frames through the libnx framebuffer with an ImGui
// window showing the GameCube pad Aurora reads from the controller, and plays
// a tone through the shim's audout stream while A is held. + (GameCube Start)
// exits. Log: sdmc:/switch/wind-waker-recomp/aurora-smoke.log and the live USB
// log.
#include <aurora/aurora.h>
#include <aurora/event.h>
#include <dolphin/pad.h>
#include <imgui.h>
#include <SDL3/SDL_audio.h>
#include <EGL/egl.h>

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <vector>

#include <switch.h>

#include "usb_log.h"

namespace {
constexpr char kDataRoot[] = "sdmc:/switch/wind-waker-recomp";
FILE* g_log = nullptr;

void log_line(const char* format, ...) {
  char line[1024];
  va_list args;
  va_start(args, format);
  const int length = std::vsnprintf(line, sizeof line, format, args);
  va_end(args);
  if (length <= 0) {
    return;
  }
  const size_t size = std::min(static_cast<size_t>(length), sizeof line - 1);
  if (g_log != nullptr) {
    std::fwrite(line, 1, size, g_log);
    std::fflush(g_log);
  }
  usb_log_write(line, size);
}

void aurora_log(AuroraLogLevel level, const char* module, const char* message, unsigned int length) {
  static const char* const kLevels[] = {"debug", "info", "warn", "error", "fatal"};
  log_line("[aurora:%s] %s: %.*s\n", kLevels[level <= LOG_FATAL ? level : LOG_FATAL], module,
           static_cast<int>(length), message);
}

// One 440 Hz tone chunk at 32 kHz stereo, pushed while A is held.
void push_tone(SDL_AudioStream* stream, double& phase) {
  constexpr int kRate = 32000;
  constexpr int kFrames = kRate / 60;
  std::vector<int16_t> samples(kFrames * 2);
  for (int i = 0; i < kFrames; ++i) {
    const auto value = static_cast<int16_t>(std::sin(phase) * 6000.0);
    samples[i * 2] = samples[i * 2 + 1] = value;
    phase += 2.0 * M_PI * 440.0 / kRate;
  }
  SDL_PutAudioStreamData(stream, samples.data(), static_cast<int>(samples.size() * sizeof(int16_t)));
}
} // namespace

int main(int argc, char** argv) {
  usb_log_start();
  g_log = std::fopen("sdmc:/switch/wind-waker-recomp/aurora-smoke.log", "w");
  log_line("[smoke] Aurora smoke test started\n");

  AuroraConfig config{};
  config.appName = "Wind Waker Recomp";
  config.userPath = kDataRoot;
  config.cachePath = kDataRoot;
  config.desiredBackend = BACKEND_OPENGLES;
  config.vsync = true;
  config.startFullscreen = true;
  config.windowWidth = 1280;
  config.windowHeight = 720;
  config.logCallback = aurora_log;
  config.logLevel = LOG_INFO;
  config.mem1Size = 0;
  config.mem2Size = 0;
  const AuroraInfo info = aurora_initialize(argc, argv, &config);
  log_line("[smoke] aurora_initialize: backend=%d window=%ux%u fb=%ux%u\n", static_cast<int>(info.backend),
           info.windowSize.width, info.windowSize.height, info.windowSize.fb_width, info.windowSize.fb_height);
  PADInit();

  const SDL_AudioSpec spec{SDL_AUDIO_S16, 2, 32000};
  SDL_AudioStream* audio = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
  log_line("[smoke] audio stream: %s\n", audio != nullptr ? "open" : SDL_GetError());
  if (audio != nullptr) {
    SDL_ResumeAudioStreamDevice(audio);
  }

  double phase = 0.0;
  uint64_t frames = 0;
  uint64_t presented = 0;
  const uint64_t start = armGetSystemTick();
  bool running = true;
  while (running) {
    for (const AuroraEvent* event = aurora_update(); event != nullptr && event->type != AURORA_NONE; ++event) {
      if (event->type == AURORA_EXIT) {
        running = false;
      }
    }
    PADStatus pads[4]{};
    PADRead(pads);
    const PADStatus& pad = pads[0];
    if ((pad.button & PAD_BUTTON_START) != 0) {
      running = false;
    }
    if (audio != nullptr && (pad.button & PAD_BUTTON_A) != 0 && SDL_GetAudioStreamQueued(audio) < 32000) {
      push_tone(audio, phase);
    }
    ++frames;
    if (!aurora_begin_frame()) {
      continue;
    }
    ++presented;
    const double seconds = armTicksToNs(armGetSystemTick() - start) / 1e9;
    ImGui::SetNextWindowPos(ImVec2{60.f, 60.f}, ImGuiCond_Always);
    ImGui::Begin("Wind Waker Recomp - Aurora on Switch", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::Text("Frames presented: %llu (%.1f fps)", static_cast<unsigned long long>(presented),
                seconds > 0 ? presented / seconds : 0.0);
    ImGui::Text("GameCube pad 0: err=%d buttons=0x%04X", pad.err, pad.button);
    ImGui::Text("Main stick %4d %4d   C stick %4d %4d", pad.stickX, pad.stickY, pad.substickX, pad.substickY);
    ImGui::Text("Triggers L %3u R %3u", pad.triggerLeft, pad.triggerRight);
    ImGui::Text("Hold A for a 440 Hz tone. + exits.");
    ImGui::End();
    aurora_end_frame();
    if (presented == 1 || presented % 300 == 0) {
      log_line("[smoke] presented=%llu loops=%llu fps=%.1f pad_err=%d buttons=0x%04X\n",
               static_cast<unsigned long long>(presented), static_cast<unsigned long long>(frames),
               seconds > 0 ? presented / seconds : 0.0, pad.err, pad.button);
    }
  }

  log_line("[smoke] exiting after %llu frames\n", static_cast<unsigned long long>(presented));
  if (audio != nullptr) {
    SDL_DestroyAudioStream(audio);
  }
  aurora_shutdown();
  // Mesa's EGL display outlives Dawn: without terminating it, the Homebrew Menu
  // that hbloader loads next into this process crashed on every exit
  // (nx-hbmenu + 0xf6b34, Atmosphère 2168-0002).
  eglTerminate(eglGetDisplay(EGL_DEFAULT_DISPLAY));
  eglReleaseThread();
  usb_log_stop(2000);
  if (g_log != nullptr) {
    std::fclose(g_log);
  }
  return 0;
}

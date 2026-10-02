#include "switch_present.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include <switch.h>

namespace aurora::switch_present {
namespace {
// libnx's default window; the image is scaled to it by Aurora's present pass.
constexpr uint32_t kScreenWidth = 1280;
constexpr uint32_t kScreenHeight = 720;
constexpr uint32_t kBytesPerPixel = 4;
constexpr uint64_t kMapTimeoutNs = 1'000'000'000;

wgpu::Instance g_instance;
wgpu::Device g_device;
wgpu::Texture g_texture;
wgpu::TextureView g_view;
wgpu::Buffer g_readback;
uint32_t g_width = 0;
uint32_t g_height = 0;
uint32_t g_rowBytes = 0;
bool g_copyPending = false;
Framebuffer g_framebuffer;
bool g_framebufferReady = false;

bool open_framebuffer() {
  if (g_framebufferReady) {
    return true;
  }
  if (R_FAILED(framebufferCreate(&g_framebuffer, nwindowGetDefault(), kScreenWidth, kScreenHeight,
                                 PIXEL_FORMAT_RGBA_8888, 2))) {
    std::fprintf(stderr, "[switch-present] framebufferCreate failed\n");
    return false;
  }
  if (R_FAILED(framebufferMakeLinear(&g_framebuffer))) {
    std::fprintf(stderr, "[switch-present] framebufferMakeLinear failed\n");
    framebufferClose(&g_framebuffer);
    return false;
  }
  g_framebufferReady = true;
  return true;
}
} // namespace

void initialize(const wgpu::Instance& instance, const wgpu::Device& device) {
  g_instance = instance;
  g_device = device;
  open_framebuffer();
}

void resize(uint32_t width, uint32_t height) {
  if (!g_device || width == 0 || height == 0 || (width == g_width && height == g_height && g_texture)) {
    return;
  }
  const wgpu::TextureDescriptor textureDescriptor{
      .label = "Switch present image",
      .usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopySrc,
      .size = {width, height, 1},
      .format = kFormat,
  };
  g_texture = g_device.CreateTexture(&textureDescriptor);
  g_view = g_texture.CreateView();
  // Buffer copies need rows aligned to 256 bytes.
  g_rowBytes = (width * kBytesPerPixel + 255u) & ~255u;
  const wgpu::BufferDescriptor bufferDescriptor{
      .label = "Switch present readback",
      .usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::MapRead,
      .size = static_cast<uint64_t>(g_rowBytes) * height,
  };
  g_readback = g_device.CreateBuffer(&bufferDescriptor);
  g_width = width;
  g_height = height;
  g_copyPending = false;
}

bool acquire(wgpu::Texture& texture, wgpu::TextureView& view) {
  if (!g_texture) {
    return false;
  }
  texture = g_texture;
  view = g_view;
  return true;
}

void encode_readback(const wgpu::CommandEncoder& encoder) {
  if (!g_texture) {
    return;
  }
  const wgpu::TexelCopyTextureInfo source{.texture = g_texture};
  const wgpu::TexelCopyBufferInfo destination{
      .layout = {.bytesPerRow = g_rowBytes, .rowsPerImage = g_height},
      .buffer = g_readback,
  };
  const wgpu::Extent3D extent{g_width, g_height, 1};
  encoder.CopyTextureToBuffer(&source, &destination, &extent);
  g_copyPending = true;
}

bool present() {
  if (!g_copyPending || !open_framebuffer()) {
    return false;
  }
  g_copyPending = false;
  bool mapped = false;
  const uint64_t size = static_cast<uint64_t>(g_rowBytes) * g_height;
  const wgpu::Future future = g_readback.MapAsync(
      wgpu::MapMode::Read, 0, size, wgpu::CallbackMode::WaitAnyOnly,
      [&mapped](wgpu::MapAsyncStatus status, wgpu::StringView) { mapped = status == wgpu::MapAsyncStatus::Success; });
  if (g_instance.WaitAny(future, kMapTimeoutNs) != wgpu::WaitStatus::Success || !mapped) {
    std::fprintf(stderr, "[switch-present] frame readback failed\n");
    return false;
  }
  const auto* pixels = static_cast<const uint8_t*>(g_readback.GetConstMappedRange(0, size));
  uint32_t stride = 0;
  auto* screen = static_cast<uint8_t*>(framebufferBegin(&g_framebuffer, &stride));
  if (pixels != nullptr && screen != nullptr) {
    const uint32_t rows = std::min(g_height, kScreenHeight);
    const uint32_t rowBytes = std::min(g_width, kScreenWidth) * kBytesPerPixel;
    for (uint32_t y = 0; y < rows; ++y) {
      std::memcpy(screen + static_cast<size_t>(y) * stride, pixels + static_cast<size_t>(y) * g_rowBytes, rowBytes);
    }
  }
  framebufferEnd(&g_framebuffer);
  g_readback.Unmap();
  return pixels != nullptr && screen != nullptr;
}

void shutdown() {
  if (g_framebufferReady) {
    framebufferClose(&g_framebuffer);
    g_framebufferReady = false;
  }
  g_readback = {};
  g_view = {};
  g_texture = {};
  g_device = {};
  g_instance = {};
  g_width = g_height = 0;
}
} // namespace aurora::switch_present

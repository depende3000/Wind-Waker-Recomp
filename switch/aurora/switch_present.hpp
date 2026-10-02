// Presentation for Aurora on the Switch. Dawn has no surface for libnx's
// NWindow, so an offscreen texture stands in for the swapchain image: Aurora
// draws its present and ImGui passes into it, the frame is copied to a mapped
// buffer, and the CPU copies it to the libnx framebuffer (the path the Dawn
// probe proved on the console). Used by patches to lib/webgpu/gpu.cpp and
// lib/aurora.cpp.
#pragma once

#include <cstdint>
#include <webgpu/webgpu_cpp.h>

namespace aurora::switch_present {
// The format of the stand-in image, and of the libnx framebuffer.
inline constexpr wgpu::TextureFormat kFormat = wgpu::TextureFormat::RGBA8Unorm;

void initialize(const wgpu::Instance& instance, const wgpu::Device& device);
// (Re)creates the image at the configured surface size.
void resize(uint32_t width, uint32_t height);
// The image to draw this frame's present into.
bool acquire(wgpu::Texture& texture, wgpu::TextureView& view);
// Records the copy of the image to the readback buffer; call after its passes.
void encode_readback(const wgpu::CommandEncoder& encoder);
// After the frame is submitted: waits for the copy and shows it on screen.
bool present();
void shutdown();
} // namespace aurora::switch_present

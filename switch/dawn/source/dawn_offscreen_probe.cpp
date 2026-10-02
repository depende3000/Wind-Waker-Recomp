#include <EGL/egl.h>
#include <dawn/native/DawnNative.h>
#include <dawn/native/OpenGLBackend.h>
#include <switch.h>
#include <webgpu/webgpu_cpp.h>

#include "usb_log.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>
#include <functional>
#include <pthread.h>
#include <vector>

namespace {

constexpr char kDataDirectory[] = "sdmc:/switch/wind-waker-recomp";
constexpr char kLogPath[] = "sdmc:/switch/wind-waker-recomp/dawn-probe.log";
constexpr uint32_t kRenderWidth = 320;
constexpr uint32_t kRenderHeight = 180;
constexpr uint32_t kDisplayWidth = 1280;
constexpr uint32_t kDisplayHeight = 720;
constexpr uint32_t kBytesPerPixel = 4;
constexpr uint32_t kBytesPerRow = kRenderWidth * kBytesPerPixel;
constexpr size_t kReadbackSize =
    static_cast<size_t>(kBytesPerRow) * kRenderHeight;

FILE* g_log = nullptr;

struct Vertex {
    float position[3];
    float uv[2];
    float color[4];
    uint32_t mode;
};

constexpr char kShader[] = R"(
struct VertexOut {
    @builtin(position) position : vec4f,
    @location(0) uv : vec2f,
    @location(1) color : vec4f,
    @location(2) @interpolate(flat, either) mode : u32,
}

@group(0) @binding(0) var testTexture : texture_2d<f32>;
@group(0) @binding(1) var testSampler : sampler;

@vertex
fn vs_main(
    @location(0) position : vec3f,
    @location(1) uv : vec2f,
    @location(2) color : vec4f,
    @location(3) mode : u32,
) -> VertexOut {
    var output : VertexOut;
    output.position = vec4f(position, 1.0);
    output.uv = uv;
    output.color = color;
    output.mode = mode;
    return output;
}

@fragment
fn fs_main(input : VertexOut) -> @location(0) vec4f {
    // WGSL requires textureSample in uniform control flow, so sample first.
    let sampled = textureSample(testTexture, testSampler, input.uv);
    if (input.mode == 0u) {
        return sampled;
    }
    return input.color;
}
)";

void log_message(const char* format, ...) {
    va_list args;
    va_start(args, format);
    va_list stderr_args;
    va_copy(stderr_args, args);
    vfprintf(stderr, format, stderr_args);
    va_end(stderr_args);
    if (g_log != nullptr) {
        va_list file_args;
        va_copy(file_args, args);
        vfprintf(g_log, format, file_args);
        va_end(file_args);
        fflush(g_log);
    }
    char usb_line[1024];
    const int usb_length = vsnprintf(usb_line, sizeof(usb_line), format, args);
    if (usb_length > 0)
        usb_log_write(usb_line, std::min(static_cast<size_t>(usb_length), sizeof(usb_line) - 1));
    va_end(args);
}

void log_wgpu_message(const char* prefix, wgpu::StringView message) {
    log_message("%s", prefix);
    if (message.data != nullptr && message.length != 0) {
        const size_t length = message.length == WGPU_STRLEN
                                  ? std::strlen(message.data)
                                  : message.length;
        fwrite(message.data, 1, length, stderr);
        if (g_log != nullptr) {
            fwrite(message.data, 1, length, g_log);
            fflush(g_log);
        }
        usb_log_write(message.data, length);
    }
    log_message("\n");
}

bool ensure_directory(const char* path) {
    if (mkdir(path, 0777) == 0)
        return true;

    if (errno != EEXIST)
        return false;

    struct stat info;
    return stat(path, &info) == 0 && S_ISDIR(info.st_mode);
}

FILE* open_probe_log() {
    if (!ensure_directory("sdmc:/switch") || !ensure_directory(kDataDirectory))
        return nullptr;
    return fopen(kLogPath, "w");
}

Vertex make_vertex(float x, float y, float z, float u, float v,
                   const std::array<float, 4>& color, uint32_t mode) {
    return {{x, y, z}, {u, v}, {color[0], color[1], color[2], color[3]}, mode};
}

void append_quad(std::array<Vertex, 18>& vertices, size_t first,
                 float left, float bottom, float right, float top, float depth,
                 const std::array<float, 4>& color, uint32_t mode) {
    const std::array<Vertex, 4> corners = {
        make_vertex(left, bottom, depth, 0.0f, 1.0f, color, mode),
        make_vertex(right, bottom, depth, 1.0f, 1.0f, color, mode),
        make_vertex(left, top, depth, 0.0f, 0.0f, color, mode),
        make_vertex(right, top, depth, 1.0f, 0.0f, color, mode),
    };
    constexpr std::array<size_t, 6> kIndices = {0, 1, 2, 2, 1, 3};
    for (size_t i = 0; i < kIndices.size(); ++i)
        vertices[first + i] = corners[kIndices[i]];
}

wgpu::ShaderModule create_shader_module(const wgpu::Device& device) {
    wgpu::ShaderSourceWGSL wgsl = {};
    wgsl.code = kShader;
    wgpu::ShaderModuleDescriptor descriptor = {};
    descriptor.nextInChain = &wgsl;
    return device.CreateShaderModule(&descriptor);
}

wgpu::RenderPipeline create_pipeline(const wgpu::Device& device,
                                     const wgpu::ShaderModule& shader) {
    const std::array<wgpu::VertexAttribute, 4> attributes = {{
        {nullptr, wgpu::VertexFormat::Float32x3, offsetof(Vertex, position), 0},
        {nullptr, wgpu::VertexFormat::Float32x2, offsetof(Vertex, uv), 1},
        {nullptr, wgpu::VertexFormat::Float32x4, offsetof(Vertex, color), 2},
        {nullptr, wgpu::VertexFormat::Uint32, offsetof(Vertex, mode), 3},
    }};

    wgpu::VertexBufferLayout vertex_buffer = {};
    vertex_buffer.arrayStride = sizeof(Vertex);
    vertex_buffer.stepMode = wgpu::VertexStepMode::Vertex;
    vertex_buffer.attributeCount = attributes.size();
    vertex_buffer.attributes = attributes.data();

    wgpu::BlendState blend = {};
    blend.color.srcFactor = wgpu::BlendFactor::SrcAlpha;
    blend.color.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
    blend.color.operation = wgpu::BlendOperation::Add;
    blend.alpha.srcFactor = wgpu::BlendFactor::One;
    blend.alpha.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
    blend.alpha.operation = wgpu::BlendOperation::Add;

    wgpu::ColorTargetState color_target = {};
    color_target.format = wgpu::TextureFormat::RGBA8Unorm;
    color_target.blend = &blend;

    wgpu::FragmentState fragment = {};
    fragment.module = shader;
    fragment.entryPoint = "fs_main";
    fragment.targetCount = 1;
    fragment.targets = &color_target;

    wgpu::DepthStencilState depth = {};
    depth.format = wgpu::TextureFormat::Depth24Plus;
    depth.depthWriteEnabled = true;
    depth.depthCompare = wgpu::CompareFunction::Less;

    wgpu::RenderPipelineDescriptor descriptor = {};
    descriptor.vertex.module = shader;
    descriptor.vertex.entryPoint = "vs_main";
    descriptor.vertex.bufferCount = 1;
    descriptor.vertex.buffers = &vertex_buffer;
    descriptor.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
    descriptor.primitive.frontFace = wgpu::FrontFace::CCW;
    descriptor.primitive.cullMode = wgpu::CullMode::None;
    descriptor.depthStencil = &depth;
    descriptor.fragment = &fragment;
    return device.CreateRenderPipeline(&descriptor);
}

bool pixel_near(const uint8_t* actual, const std::array<uint8_t, 4>& expected,
                uint8_t tolerance) {
    for (size_t channel = 0; channel < 4; ++channel) {
        const int difference = static_cast<int>(actual[channel]) - expected[channel];
        if (difference < -tolerance || difference > tolerance)
            return false;
    }
    return true;
}

bool verify_readback(const std::vector<uint8_t>& pixels) {
    const std::array<std::array<uint8_t, 4>, 4> expected = {{
        {{255, 0, 0, 255}},       // Opaque red.
        {{5, 138, 20, 255}},      // Half-alpha green blended over the clear color.
        {{5, 10, 148, 255}},      // Half-alpha blue blended over the clear color.
        {{255, 255, 0, 255}},     // Opaque yellow.
    }};
    const std::array<std::array<uint32_t, 2>, 4> sample_points = {{
        {{kRenderWidth / 4, kRenderHeight / 4}},
        {{3 * kRenderWidth / 4, kRenderHeight / 4}},
        {{kRenderWidth / 4, 3 * kRenderHeight / 4}},
        {{3 * kRenderWidth / 4, 3 * kRenderHeight / 4}},
    }};

    std::array<bool, 4> found = {};
    for (const auto& point : sample_points) {
        const size_t offset =
            (static_cast<size_t>(point[1]) * kRenderWidth + point[0]) * kBytesPerPixel;
        const uint8_t* pixel = pixels.data() + offset;
        log_message("[verify] quadrant sample (%u,%u) RGBA=%u,%u,%u,%u\n",
                    point[0], point[1], pixel[0], pixel[1], pixel[2], pixel[3]);
        for (size_t candidate = 0; candidate < expected.size(); ++candidate) {
            if (!found[candidate] && pixel_near(pixel, expected[candidate], 28)) {
                found[candidate] = true;
                break;
            }
        }
    }

    bool quadrants_pass = true;
    for (bool color_found : found)
        quadrants_pass = quadrants_pass && color_found;

    const size_t center_offset =
        (static_cast<size_t>(kRenderHeight / 2) * kRenderWidth + kRenderWidth / 2) *
        kBytesPerPixel;
    const uint8_t* center = pixels.data() + center_offset;
    const bool depth_pass = pixel_near(center, {{0, 255, 0, 255}}, 10);
    log_message("[verify] unique textured/blended quadrants=%s; depth occlusion=%s "
                "(center RGBA=%u,%u,%u,%u)\n",
                quadrants_pass ? "passed" : "failed",
                depth_pass ? "passed" : "failed", center[0], center[1], center[2],
                center[3]);
    return quadrants_pass && depth_pass;
}

bool egl_extension_present(const char* extensions, const char* wanted) {
    if (extensions == nullptr || wanted == nullptr || *wanted == '\0')
        return false;

    const size_t wanted_length = std::strlen(wanted);
    const char* current = extensions;
    while ((current = std::strstr(current, wanted)) != nullptr) {
        const bool left_boundary = current == extensions || current[-1] == ' ';
        const char after = current[wanted_length];
        const bool right_boundary = after == '\0' || after == ' ';
        if (left_boundary && right_boundary)
            return true;
        current += wanted_length;
    }
    return false;
}

bool log_egl_capabilities() {
    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display == EGL_NO_DISPLAY) {
        log_message("[egl] eglGetDisplay(EGL_DEFAULT_DISPLAY) failed: 0x%04X\n",
                    eglGetError());
        return false;
    }

    EGLint major = 0;
    EGLint minor = 0;
    if (eglInitialize(display, &major, &minor) != EGL_TRUE) {
        log_message("[egl] eglInitialize failed: 0x%04X\n", eglGetError());
        return false;
    }

    const char* vendor = eglQueryString(display, EGL_VENDOR);
    const char* version = eglQueryString(display, EGL_VERSION);
    const char* extensions = eglQueryString(display, EGL_EXTENSIONS);
    const bool robustness_extension = egl_extension_present(
        extensions, "EGL_EXT_create_context_robustness");
    const bool robustness_available = robustness_extension || major > 1 ||
                                      (major == 1 && minor >= 5);
    const EGLint pbuffer_attributes[] = {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_NONE,
    };
    EGLConfig pbuffer_config = nullptr;
    EGLint pbuffer_config_count = 0;
    const bool pbuffer_available =
        eglChooseConfig(display, pbuffer_attributes, &pbuffer_config, 1,
                        &pbuffer_config_count) == EGL_TRUE &&
        pbuffer_config_count > 0;
    log_message("[egl] display=%p version=%d.%d vendor=%s version_string=%s\n",
                display, major, minor, vendor != nullptr ? vendor : "unavailable",
                version != nullptr ? version : "unavailable");
    log_message("[egl] robustness_ext_listed=%s robustness_available=%s fence_sync=%s "
                "reusable_sync=%s native_fence=%s surfaceless=%s pbuffer_config=%s\n",
                robustness_extension ? "yes" : "no",
                robustness_available ? "yes" : "no",
                egl_extension_present(extensions, "EGL_KHR_fence_sync") ? "yes" : "no",
                egl_extension_present(extensions, "EGL_KHR_reusable_sync") ? "yes" : "no",
                egl_extension_present(extensions, "EGL_ANDROID_native_fence_sync")
                    ? "yes" : "no",
                egl_extension_present(extensions, "EGL_KHR_surfaceless_context")
                    ? "yes" : "no",
                pbuffer_available ? "available" : "unavailable");
    if (extensions != nullptr)
        log_message("[egl] extensions=%s\n", extensions);
    else
        log_message("[egl] extension string unavailable (0x%04X)\n", eglGetError());
    return robustness_available;
}

void log_adapter_limits(const wgpu::Adapter& adapter) {
    wgpu::CompatibilityModeLimits compat = {};
    wgpu::Limits limits = {};
    limits.nextInChain = &compat;
    if (!adapter.GetLimits(&limits)) {
        log_message("[limits] GetLimits failed\n");
        return;
    }
    log_message("[limits] storage_buffers_in_vertex=%u in_fragment=%u per_stage=%u "
                "(Aurora needs 2 in vertex)\n",
                compat.maxStorageBuffersInVertexStage, compat.maxStorageBuffersInFragmentStage,
                limits.maxStorageBuffersPerShaderStage);
    log_message("[limits] texture2d=%u bind_groups=%u uniform_per_stage=%u "
                "sampled_per_stage=%u samplers_per_stage=%u\n",
                limits.maxTextureDimension2D, limits.maxBindGroups,
                limits.maxUniformBuffersPerShaderStage, limits.maxSampledTexturesPerShaderStage,
                limits.maxSamplersPerShaderStage);
    log_message("[limits] uniform_binding=%llu storage_binding=%llu uniform_align=%u "
                "storage_align=%u inter_stage=%u vertex_attributes=%u\n",
                static_cast<unsigned long long>(limits.maxUniformBufferBindingSize),
                static_cast<unsigned long long>(limits.maxStorageBufferBindingSize),
                limits.minUniformBufferOffsetAlignment, limits.minStorageBufferOffsetAlignment,
                limits.maxInterStageShaderVariables, limits.maxVertexAttributes);
}

// Aurora's per-frame cost on this path: draw at its 960x720 EFB size, copy to
// a mapped buffer and wait (glFinish per submission), from a worker thread.
void run_threaded_frames(const wgpu::Instance& instance, const wgpu::Device& device,
                         const wgpu::RenderPipeline& pipeline, const wgpu::BindGroup& bind_group,
                         const wgpu::Buffer& vertex_buffer) {
    constexpr uint32_t kWidth = 960;
    constexpr uint32_t kHeight = 720;
    constexpr uint32_t kRowBytes = kWidth * kBytesPerPixel;  // a multiple of 256
    constexpr int kFrames = 300;

    wgpu::TextureDescriptor color_descriptor = {};
    color_descriptor.size = {kWidth, kHeight, 1};
    color_descriptor.format = wgpu::TextureFormat::RGBA8Unorm;
    color_descriptor.usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopySrc;
    wgpu::Texture color = device.CreateTexture(&color_descriptor);
    wgpu::TextureDescriptor depth_descriptor = {};
    depth_descriptor.size = {kWidth, kHeight, 1};
    depth_descriptor.format = wgpu::TextureFormat::Depth24Plus;
    depth_descriptor.usage = wgpu::TextureUsage::RenderAttachment;
    wgpu::Texture depth = device.CreateTexture(&depth_descriptor);
    wgpu::TextureView color_view = color.CreateView();
    wgpu::TextureView depth_view = depth.CreateView();
    wgpu::BufferDescriptor readback_descriptor = {};
    readback_descriptor.size = static_cast<uint64_t>(kRowBytes) * kHeight;
    readback_descriptor.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::MapRead;
    wgpu::Buffer readback = device.CreateBuffer(&readback_descriptor);

    u64 slowest = 0;
    int completed = 0;
    const u64 start = armGetSystemTick();
    for (int frame = 0; frame < kFrames; ++frame) {
        const u64 frame_start = armGetSystemTick();
        wgpu::RenderPassColorAttachment color_attachment = {};
        color_attachment.view = color_view;
        color_attachment.loadOp = wgpu::LoadOp::Clear;
        color_attachment.storeOp = wgpu::StoreOp::Store;
        color_attachment.clearValue = {0.04, 0.08, 0.16, 1.0};
        wgpu::RenderPassDepthStencilAttachment depth_attachment = {};
        depth_attachment.view = depth_view;
        depth_attachment.depthLoadOp = wgpu::LoadOp::Clear;
        depth_attachment.depthStoreOp = wgpu::StoreOp::Store;
        depth_attachment.depthClearValue = 1.0f;
        wgpu::RenderPassDescriptor pass_descriptor = {};
        pass_descriptor.colorAttachmentCount = 1;
        pass_descriptor.colorAttachments = &color_attachment;
        pass_descriptor.depthStencilAttachment = &depth_attachment;

        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&pass_descriptor);
        pass.SetPipeline(pipeline);
        pass.SetBindGroup(0, bind_group);
        pass.SetVertexBuffer(0, vertex_buffer);
        pass.Draw(18, 1, 0, 0);
        pass.End();
        wgpu::TexelCopyTextureInfo source = {};
        source.texture = color;
        wgpu::TexelCopyBufferInfo destination = {};
        destination.buffer = readback;
        destination.layout.bytesPerRow = kRowBytes;
        destination.layout.rowsPerImage = kHeight;
        const wgpu::Extent3D extent = {kWidth, kHeight, 1};
        encoder.CopyTextureToBuffer(&source, &destination, &extent);
        wgpu::CommandBuffer commands = encoder.Finish();
        device.GetQueue().Submit(1, &commands);

        bool mapped = false;
        const wgpu::Future future = readback.MapAsync(
            wgpu::MapMode::Read, 0, readback_descriptor.size, wgpu::CallbackMode::WaitAnyOnly,
            [&mapped](wgpu::MapAsyncStatus status, wgpu::StringView) {
                mapped = status == wgpu::MapAsyncStatus::Success;
            });
        wgpu::WaitStatus wait = wgpu::WaitStatus::TimedOut;
        for (int attempt = 0; attempt < 5000 && wait == wgpu::WaitStatus::TimedOut; ++attempt) {
            wait = instance.WaitAny(future, 0);
            if (wait == wgpu::WaitStatus::TimedOut)
                svcSleepThread(100'000);
        }
        if (wait != wgpu::WaitStatus::Success || !mapped) {
            log_message("[perf] frame %d readback failed (wait=%u)\n", frame,
                        static_cast<unsigned>(wait));
            break;
        }
        readback.Unmap();
        const u64 ticks = armGetSystemTick() - frame_start;
        if (ticks > slowest)
            slowest = ticks;
        ++completed;
    }
    const u64 total_ns = armTicksToNs(armGetSystemTick() - start);
    if (completed == 0)
        return;
    log_message("[perf] worker thread: %d/%d frames at %ux%u with readback; "
                "avg %.2f ms, slowest %.2f ms (%.1f fps)\n",
                completed, kFrames, kWidth, kHeight,
                total_ns / 1e6 / completed, armTicksToNs(slowest) / 1e6,
                completed * 1e9 / static_cast<double>(total_ns));
}

// Runs work on a new thread and waits for it. libnx gives std::thread a
// 128 KiB stack, which Tint's recursive WGSL parser and resolver overflow
// (crash report 2168-0002 in tint::resolver), so use an explicit 4 MiB.
void run_on_worker(const std::function<void()>& work) {
    pthread_attr_t attributes;
    pthread_attr_init(&attributes);
    pthread_attr_setstacksize(&attributes, 4 * 1024 * 1024);
    pthread_t thread;
    auto* job = const_cast<std::function<void()>*>(&work);
    if (pthread_create(&thread, &attributes,
                       [](void* argument) -> void* {
                           (*static_cast<std::function<void()>*>(argument))();
                           return nullptr;
                       },
                       job) != 0) {
        log_message("[thread] pthread_create failed\n");
    } else {
        pthread_join(thread, nullptr);
    }
    pthread_attr_destroy(&attributes);
}

// One scenario: a fresh device, optionally with one of Dawn's GL threading
// toggles (gl_allow_context_on_multi_threads, or gl_defer, which defers all GL
// work to Queue::Submit and binds the one context only while it runs), and the
// test scene drawn either on the thread that created the device or on a worker
// (as Aurora does). measure_frames also times Aurora-sized frames.
bool render_with_dawn(std::vector<uint8_t>& pixels, const char* label,
                      const char* threading_toggle, bool render_on_worker,
                      bool measure_frames) {
    log_message("[scenario] %s: toggle %s, draw on %s\n", label,
                threading_toggle != nullptr ? threading_toggle : "none",
                render_on_worker ? "worker thread" : "creating thread");
    // Initialize the shared default display first so the log captures the
    // platform extension set even if Dawn rejects adapter discovery.
    static const bool egl_robustness_available = log_egl_capabilities();

    dawn::native::DawnInstanceDescriptor dawn_descriptor;
    dawn_descriptor.SetLoggingCallback(
        [](wgpu::LoggingType type, wgpu::StringView message) {
            char prefix[48];
            snprintf(prefix, sizeof(prefix), "[dawn:%u] ",
                     static_cast<unsigned>(type));
            log_wgpu_message(prefix, message);
        });
    wgpu::InstanceDescriptor instance_descriptor = {};
    instance_descriptor.nextInChain = &dawn_descriptor;
    dawn::native::Instance native_instance(&instance_descriptor);
    if (native_instance.Get() == nullptr) {
        log_message("[dawn] instance creation failed\n");
        return false;
    }
    // native_instance keeps its own reference; take a second one rather than
    // Acquire, which would release the same reference twice at scope exit.
    wgpu::Instance instance(native_instance.Get());

    dawn::native::opengl::RequestAdapterOptionsGetGLProc get_gl_proc;
    get_gl_proc.getProc = reinterpret_cast<dawn::native::opengl::EGLGetProcProc>(
        eglGetProcAddress);
    get_gl_proc.display = EGL_NO_DISPLAY;
    wgpu::RequestAdapterOptions adapter_options = {};
    adapter_options.nextInChain = &get_gl_proc;
    adapter_options.backendType = wgpu::BackendType::OpenGLES;
    adapter_options.featureLevel = wgpu::FeatureLevel::Compatibility;

    const std::vector<dawn::native::Adapter> adapters =
        native_instance.EnumerateAdapters(&adapter_options);
    if (adapters.empty()) {
        log_message("[dawn] no OpenGLES adapter; inspect preceding EGL/Dawn diagnostics\n");
        instance = nullptr;
        return false;
    }

    wgpu::Adapter adapter(adapters.front().Get());
    wgpu::AdapterInfo adapter_info = {};
    const wgpu::Status info_status = adapter.GetInfo(&adapter_info);
    log_message("[dawn] GLES adapter count=%zu info_status=%u backend=%u type=%u\n",
                adapters.size(), static_cast<unsigned>(info_status),
                static_cast<unsigned>(adapter_info.backendType),
                static_cast<unsigned>(adapter_info.adapterType));
    log_wgpu_message("[dawn] adapter description: ", adapter_info.description);
    log_wgpu_message("[dawn] adapter vendor: ", adapter_info.vendor);
    log_wgpu_message("[dawn] adapter device: ", adapter_info.device);
    static bool limits_logged = false;
    if (!limits_logged) {
        log_adapter_limits(adapter);
        limits_logged = true;
    }

    wgpu::DeviceDescriptor device_descriptor = {};
    wgpu::DawnTogglesDescriptor device_toggles = {};
    std::array<const char*, 2> enabled_toggles = {};
    size_t toggle_count = 0;
    if (threading_toggle != nullptr)
        enabled_toggles[toggle_count++] = threading_toggle;
    if (!egl_robustness_available)
        enabled_toggles[toggle_count++] = "disable_robustness";
    device_toggles.enabledToggleCount = toggle_count;
    device_toggles.enabledToggles = enabled_toggles.data();
    device_descriptor.nextInChain = &device_toggles;
    if (!egl_robustness_available) {
        log_message("[dawn] WARNING: disabling WebGPU robust buffer access for this diagnostic "
                    "because EGL has no robust-context support; not suitable for production\n");
    }
    device_descriptor.SetUncapturedErrorCallback(
        [](const wgpu::Device&, wgpu::ErrorType type, wgpu::StringView message) {
            char prefix[48];
            snprintf(prefix, sizeof(prefix), "[dawn:error:%u] ",
                     static_cast<unsigned>(type));
            log_wgpu_message(prefix, message);
        });
    wgpu::Device device = adapter.CreateDevice(&device_descriptor);
    if (device == nullptr) {
        log_message("[dawn] device creation failed\n");
        instance = nullptr;
        return false;
    }
    device.SetLoggingCallback([](wgpu::LoggingType type, wgpu::StringView message) {
        char prefix[48];
        snprintf(prefix, sizeof(prefix), "[dawn:device:%u] ",
                 static_cast<unsigned>(type));
        log_wgpu_message(prefix, message);
    });

    const auto render = [&]() -> bool {
    const std::array<uint8_t, 16> texture_pixels = {{
        255, 0, 0, 255,     0, 255, 0, 128,
        0, 0, 255, 128,     255, 255, 0, 255,
    }};
    wgpu::TextureDescriptor input_texture_descriptor = {};
    input_texture_descriptor.size = {2, 2, 1};
    input_texture_descriptor.format = wgpu::TextureFormat::RGBA8Unorm;
    input_texture_descriptor.usage =
        wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst;
    wgpu::Texture input_texture = device.CreateTexture(&input_texture_descriptor);
    if (input_texture == nullptr) {
        log_message("[dawn] input texture allocation failed\n");
        device = nullptr;
        instance = nullptr;
        return false;
    }

    wgpu::TexelCopyTextureInfo input_copy = {};
    input_copy.texture = input_texture;
    wgpu::TexelCopyBufferLayout input_layout = {};
    input_layout.bytesPerRow = 2 * kBytesPerPixel;
    input_layout.rowsPerImage = 2;
    const wgpu::Extent3D input_extent = {2, 2, 1};
    device.GetQueue().WriteTexture(&input_copy, texture_pixels.data(),
                                   texture_pixels.size(), &input_layout, &input_extent);

    wgpu::SamplerDescriptor sampler_descriptor = {};
    sampler_descriptor.addressModeU = wgpu::AddressMode::ClampToEdge;
    sampler_descriptor.addressModeV = wgpu::AddressMode::ClampToEdge;
    sampler_descriptor.magFilter = wgpu::FilterMode::Nearest;
    sampler_descriptor.minFilter = wgpu::FilterMode::Nearest;
    sampler_descriptor.mipmapFilter = wgpu::MipmapFilterMode::Nearest;
    wgpu::Sampler sampler = device.CreateSampler(&sampler_descriptor);

    wgpu::TextureDescriptor color_descriptor = {};
    color_descriptor.size = {kRenderWidth, kRenderHeight, 1};
    color_descriptor.format = wgpu::TextureFormat::RGBA8Unorm;
    color_descriptor.usage =
        wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopySrc;
    wgpu::Texture color_texture = device.CreateTexture(&color_descriptor);
    wgpu::TextureView color_view = color_texture.CreateView();

    wgpu::TextureDescriptor depth_descriptor = {};
    depth_descriptor.size = {kRenderWidth, kRenderHeight, 1};
    depth_descriptor.format = wgpu::TextureFormat::Depth24Plus;
    depth_descriptor.usage = wgpu::TextureUsage::RenderAttachment;
    wgpu::Texture depth_texture = device.CreateTexture(&depth_descriptor);
    wgpu::TextureView depth_view = depth_texture.CreateView();

    wgpu::ShaderModule shader = create_shader_module(device);
    wgpu::RenderPipeline pipeline = create_pipeline(device, shader);
    if (sampler == nullptr || color_texture == nullptr || color_view == nullptr ||
        depth_texture == nullptr || depth_view == nullptr || shader == nullptr ||
        pipeline == nullptr) {
        log_message("[dawn] sampler/texture/shader/pipeline creation failed\n");
        device = nullptr;
        instance = nullptr;
        return false;
    }

    wgpu::TextureView input_view = input_texture.CreateView();
    std::array<wgpu::BindGroupEntry, 2> bind_entries = {};
    bind_entries[0].binding = 0;
    bind_entries[0].textureView = input_view;
    bind_entries[1].binding = 1;
    bind_entries[1].sampler = sampler;
    wgpu::BindGroupLayout bind_layout = pipeline.GetBindGroupLayout(0);
    wgpu::BindGroupDescriptor bind_descriptor = {};
    bind_descriptor.layout = bind_layout;
    bind_descriptor.entryCount = bind_entries.size();
    bind_descriptor.entries = bind_entries.data();
    wgpu::BindGroup bind_group = device.CreateBindGroup(&bind_descriptor);
    if (bind_group == nullptr) {
        log_message("[dawn] sampled texture bind group creation failed\n");
        device = nullptr;
        instance = nullptr;
        return false;
    }

    const std::array<float, 4> white = {{1.0f, 1.0f, 1.0f, 1.0f}};
    const std::array<float, 4> green = {{0.0f, 1.0f, 0.0f, 1.0f}};
    const std::array<float, 4> red = {{1.0f, 0.0f, 0.0f, 1.0f}};
    std::array<Vertex, 18> vertices = {};
    append_quad(vertices, 0, -1.0f, -1.0f, 1.0f, 1.0f, 0.5f, white, 0);
    append_quad(vertices, 6, -0.30f, -0.30f, 0.30f, 0.30f, 0.25f, green, 1);
    append_quad(vertices, 12, -0.30f, -0.30f, 0.30f, 0.30f, 0.75f, red, 1);
    wgpu::BufferDescriptor vertex_buffer_descriptor = {};
    vertex_buffer_descriptor.size = sizeof(vertices);
    vertex_buffer_descriptor.usage =
        wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst;
    wgpu::Buffer vertex_buffer = device.CreateBuffer(&vertex_buffer_descriptor);
    device.GetQueue().WriteBuffer(vertex_buffer, 0, vertices.data(), sizeof(vertices));

    wgpu::RenderPassColorAttachment color_attachment = {};
    color_attachment.view = color_view;
    color_attachment.loadOp = wgpu::LoadOp::Clear;
    color_attachment.storeOp = wgpu::StoreOp::Store;
    color_attachment.clearValue = {0.04, 0.08, 0.16, 1.0};
    wgpu::RenderPassDepthStencilAttachment depth_attachment = {};
    depth_attachment.view = depth_view;
    depth_attachment.depthLoadOp = wgpu::LoadOp::Clear;
    depth_attachment.depthStoreOp = wgpu::StoreOp::Store;
    depth_attachment.depthClearValue = 1.0f;
    wgpu::RenderPassDescriptor pass_descriptor = {};
    pass_descriptor.colorAttachmentCount = 1;
    pass_descriptor.colorAttachments = &color_attachment;
    pass_descriptor.depthStencilAttachment = &depth_attachment;

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&pass_descriptor);
    pass.SetPipeline(pipeline);
    pass.SetBindGroup(0, bind_group);
    pass.SetVertexBuffer(0, vertex_buffer);
    pass.Draw(6, 1, 0, 0);
    pass.Draw(6, 1, 6, 0);
    pass.Draw(6, 1, 12, 0);
    pass.End();

    wgpu::BufferDescriptor readback_descriptor = {};
    readback_descriptor.size = kReadbackSize;
    readback_descriptor.usage =
        wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::MapRead;
    wgpu::Buffer readback_buffer = device.CreateBuffer(&readback_descriptor);
    wgpu::TexelCopyTextureInfo source = {};
    source.texture = color_texture;
    wgpu::TexelCopyBufferInfo destination = {};
    destination.buffer = readback_buffer;
    destination.layout.bytesPerRow = kBytesPerRow;
    destination.layout.rowsPerImage = kRenderHeight;
    const wgpu::Extent3D copy_extent = {kRenderWidth, kRenderHeight, 1};
    encoder.CopyTextureToBuffer(&source, &destination, &copy_extent);
    wgpu::CommandBuffer commands = encoder.Finish();
    device.GetQueue().Submit(1, &commands);
    log_message("[dawn] submitted textured WGSL pass: %ux%u RGBA8, depth24+, alpha blend\n",
                kRenderWidth, kRenderHeight);

    bool map_succeeded = false;
    const wgpu::Future map_future = readback_buffer.MapAsync(
        wgpu::MapMode::Read, 0, kReadbackSize, wgpu::CallbackMode::WaitAnyOnly,
        [&map_succeeded](wgpu::MapAsyncStatus status, wgpu::StringView message) {
            map_succeeded = status == wgpu::MapAsyncStatus::Success;
            if (!map_succeeded)
                log_wgpu_message("[dawn] readback map failed: ", message);
        });
    // Timed waits need the TimedWaitAny instance feature, which the GL backend
    // without EGL sync cannot back, so poll with a zero timeout instead.
    wgpu::WaitStatus wait_status = wgpu::WaitStatus::TimedOut;
    for (int attempt = 0; attempt < 5000 && wait_status == wgpu::WaitStatus::TimedOut;
         ++attempt) {
        wait_status = instance.WaitAny(map_future, 0);
        if (wait_status == wgpu::WaitStatus::TimedOut)
            svcSleepThread(1'000'000);
    }
    if (wait_status != wgpu::WaitStatus::Success || !map_succeeded) {
        log_message("[dawn] GPU readback wait failed (status=%u)\n",
                    static_cast<unsigned>(wait_status));
        device = nullptr;
        instance = nullptr;
        return false;
    }

    const auto* mapped = static_cast<const uint8_t*>(
        readback_buffer.GetConstMappedRange(0, kReadbackSize));
    if (mapped == nullptr) {
        log_message("[dawn] mapped readback pointer is null\n");
        readback_buffer.Unmap();
        device = nullptr;
        instance = nullptr;
        return false;
    }
    pixels.assign(mapped, mapped + kReadbackSize);
    readback_buffer.Unmap();

    const bool verified = verify_readback(pixels);
    log_message("[dawn] Dawn OpenGLES offscreen test %s\n",
                verified ? "passed" : "failed pixel checks");
    if (verified && measure_frames)
        run_threaded_frames(instance, device, pipeline, bind_group, vertex_buffer);
    return verified;
    };

    bool verified = false;
    if (render_on_worker) {
        run_on_worker([&] { verified = render(); });
    } else {
        verified = render();
    }
    log_message("[scenario] %s: %s\n", label, verified ? "PASS" : "FAIL");
    device = nullptr;
    instance = nullptr;
    return verified;
}

void fill_failure_frame(uint8_t* framebuffer, uint32_t stride) {
    for (uint32_t y = 0; y < kDisplayHeight; ++y) {
        uint8_t* row = framebuffer + static_cast<size_t>(y) * stride;
        for (uint32_t x = 0; x < kDisplayWidth; ++x) {
            const bool stripe = ((x / 80) + (y / 80)) % 2 == 0;
            uint8_t* pixel = row + static_cast<size_t>(x) * kBytesPerPixel;
            pixel[0] = stripe ? 150 : 45;
            pixel[1] = 12;
            pixel[2] = stripe ? 25 : 10;
            pixel[3] = 255;
        }
    }
}

bool present_readback(const std::vector<uint8_t>& pixels, bool success) {
    Framebuffer framebuffer = {};
    Result result = framebufferCreate(&framebuffer, nwindowGetDefault(),
                                     kDisplayWidth, kDisplayHeight,
                                     PIXEL_FORMAT_RGBA_8888, 2);
    if (R_FAILED(result)) {
        log_message("[display] framebufferCreate failed: 0x%08X\n", result);
        return false;
    }
    result = framebufferMakeLinear(&framebuffer);
    if (R_FAILED(result)) {
        log_message("[display] framebufferMakeLinear failed: 0x%08X\n", result);
        framebufferClose(&framebuffer);
        return false;
    }

    uint32_t stride = 0;
    uint8_t* output = static_cast<uint8_t*>(framebufferBegin(&framebuffer, &stride));
    if (output == nullptr || stride < kDisplayWidth * kBytesPerPixel) {
        log_message("[display] invalid framebuffer or stride (%u)\n", stride);
        framebufferEnd(&framebuffer);
        framebufferClose(&framebuffer);
        return false;
    }

    if (!success || pixels.size() < kReadbackSize) {
        fill_failure_frame(output, stride);
    } else {
        for (uint32_t y = 0; y < kDisplayHeight; ++y) {
            const uint32_t source_y = y * kRenderHeight / kDisplayHeight;
            uint8_t* output_row = output + static_cast<size_t>(y) * stride;
            const uint8_t* source_row =
                pixels.data() + static_cast<size_t>(source_y) * kBytesPerRow;
            for (uint32_t x = 0; x < kDisplayWidth; ++x) {
                const uint32_t source_x = x * kRenderWidth / kDisplayWidth;
                memcpy(output_row + static_cast<size_t>(x) * kBytesPerPixel,
                       source_row + static_cast<size_t>(source_x) * kBytesPerPixel,
                       kBytesPerPixel);
            }
        }
    }

    framebufferEnd(&framebuffer);
    log_message("[display] %s frame presented via libnx framebuffer; press + to exit\n",
                success ? "Dawn readback" : "failure indicator");
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);
    while (appletMainLoop()) {
        padUpdate(&pad);
        if ((padGetButtonsDown(&pad) & HidNpadButton_Plus) != 0)
            break;
        svcSleepThread(16'000'000);
    }
    framebufferClose(&framebuffer);
    return true;
}

}  // namespace

int main(int, char**) {
    const bool usb_log_ready = usb_log_start();
    const bool sd_mounted = fsdevGetDeviceFileSystem("sdmc") != nullptr;
    g_log = sd_mounted ? open_probe_log() : nullptr;
    log_message("[probe] Dawn OpenGLES offscreen probe started; sdmc=%s log=%s\n",
                sd_mounted ? "available (libnx auto-mount)" : "unavailable",
                g_log != nullptr ? kLogPath : "unavailable");
    log_message("[probe] usb live log=%s (scripts/switch/usb_log.py on the host)\n",
                usb_log_ready ? "started" : "unavailable");
    if (!sd_mounted || g_log == nullptr)
        log_message("[probe] diagnostics may be incomplete because SD logging is unavailable\n");

    std::vector<uint8_t> pixels;
    // The first scenario is the reference that is displayed. The others answer
    // whether Aurora's threading model works on this GL stack.
    const bool success = render_with_dawn(pixels, "A", nullptr, false, false);
    std::vector<uint8_t> scratch;
    render_with_dawn(scratch, "B", nullptr, true, false);
    render_with_dawn(scratch, "C", "gl_allow_context_on_multi_threads", true, false);
    // Aurora's model unchanged, if gl_defer works here: device on this thread,
    // GPU work from another.
    render_with_dawn(scratch, "E", "gl_defer", true, true);
    // The model Aurora would need here: one GPU thread that creates the device
    // and does all of the GPU work, with no multi-thread toggle.
    log_message("[scenario] D runs entirely on a worker thread (device created there)\n");
    run_on_worker([&] { render_with_dawn(scratch, "D", nullptr, false, true); });
    log_message("[probe] result=%s\n", success ? "PASS" : "FAIL");
    const bool display_ok = present_readback(pixels, success);
    // Mesa's EGL display outlives Dawn: without terminating it, the Homebrew Menu
    // that hbloader loads next into this process crashed on every exit
    // (nx-hbmenu + 0xf6b34, Atmosphère 2168-0002).
    eglTerminate(eglGetDisplay(EGL_DEFAULT_DISPLAY));
    eglReleaseThread();
    log_message("[probe] display=%s; shutting down\n",
                display_ok ? "presented" : "failed");
    if (g_log != nullptr) {
        fclose(g_log);
        g_log = nullptr;
    }
    usb_log_stop(2000);
    return success && display_ok ? 0 : 1;
}
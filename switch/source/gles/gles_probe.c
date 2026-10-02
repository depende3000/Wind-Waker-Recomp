#define SDL_MAIN_HANDLED

#include <GLES2/gl2.h>
#include <SDL2/SDL.h>
#include <switch.h>

#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/stat.h>

#define PROBE_DATA_DIR "sdmc:/switch/wind-waker-recomp"
#define PROBE_LOG_PATH PROBE_DATA_DIR "/gles-probe.log"

static FILE* g_log;

static void log_message(const char* format, ...) {
    va_list args;
    va_start(args, format);
    va_list stderr_args;
    va_copy(stderr_args, args);
    vfprintf(stderr, format, stderr_args);
    va_end(stderr_args);
    if (g_log != NULL) {
        va_list file_args;
        va_copy(file_args, args);
        vfprintf(g_log, format, file_args);
        va_end(file_args);
        fflush(g_log);
    }
    va_end(args);
}

static bool ensure_directory(const char* path) {
    if (mkdir(path, 0777) == 0)
        return true;
    if (errno != EEXIST)
        return false;

    struct stat info;
    return stat(path, &info) == 0 && S_ISDIR(info.st_mode);
}

static FILE* open_probe_log(void) {
    if (!ensure_directory("sdmc:/switch") ||
        !ensure_directory(PROBE_DATA_DIR))
        return NULL;
    return fopen(PROBE_LOG_PATH, "a");
}

static GLuint compile_shader(GLenum kind, const char* source) {
    const GLuint shader = glCreateShader(kind);
    if (shader == 0u) {
        log_message("[gles] glCreateShader failed (0x%04X)\n", glGetError());
        return 0u;
    }

    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled == GL_TRUE)
        return shader;

    char message[1024] = {0};
    GLsizei length = 0;
    glGetShaderInfoLog(shader, sizeof message, &length, message);
    log_message("[gles] shader compile failed: %s\n", message);
    glDeleteShader(shader);
    return 0u;
}

static GLuint create_textured_program(void) {
    static const char vertex_source[] =
        "attribute vec3 a_position;\n"
        "attribute vec2 a_texcoord;\n"
        "varying vec2 v_texcoord;\n"
        "void main() {\n"
        "  gl_Position = vec4(a_position, 1.0);\n"
        "  v_texcoord = a_texcoord;\n"
        "}\n";
    static const char fragment_source[] =
        "precision mediump float;\n"
        "varying vec2 v_texcoord;\n"
        "uniform sampler2D u_texture;\n"
        "void main() { gl_FragColor = texture2D(u_texture, v_texcoord); }\n";

    const GLuint vertex = compile_shader(GL_VERTEX_SHADER, vertex_source);
    const GLuint fragment = compile_shader(GL_FRAGMENT_SHADER, fragment_source);
    if (vertex == 0u || fragment == 0u) {
        if (vertex != 0u)
            glDeleteShader(vertex);
        if (fragment != 0u)
            glDeleteShader(fragment);
        return 0u;
    }

    const GLuint program = glCreateProgram();
    if (program == 0u) {
        log_message("[gles] glCreateProgram failed (0x%04X)\n", glGetError());
        glDeleteShader(vertex);
        glDeleteShader(fragment);
        return 0u;
    }

    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glBindAttribLocation(program, 0u, "a_position");
    glBindAttribLocation(program, 1u, "a_texcoord");
    glLinkProgram(program);
    glDeleteShader(vertex);
    glDeleteShader(fragment);

    GLint linked = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (linked == GL_TRUE)
        return program;

    char message[1024] = {0};
    GLsizei length = 0;
    glGetProgramInfoLog(program, sizeof message, &length, message);
    log_message("[gles] program link failed: %s\n", message);
    glDeleteProgram(program);
    return 0u;
}

static GLuint create_test_texture(void) {
    // Four colors with alpha variation make texture sampling and blending
    // visible without relying on any game assets.
    static const GLubyte pixels[] = {
        255u,  32u,  32u, 255u,    32u, 255u,  32u, 160u,
         32u,  96u, 255u, 160u,   255u, 224u,  32u, 255u,
    };

    GLuint texture = 0u;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, pixels);

    const GLenum error = glGetError();
    if (texture == 0u || error != GL_NO_ERROR) {
        log_message("[gles] texture setup failed (0x%04X)\n", error);
        if (texture != 0u)
            glDeleteTextures(1, &texture);
        return 0u;
    }
    return texture;
}

static bool draw_test_frame(GLuint program, GLuint texture, int width,
                            int height, bool* copy_tested,
                            bool* copy_succeeded) {
    static const GLfloat vertices[] = {
        -0.72f, -0.72f,  0.0f, 0.0f, 0.0f,
         0.72f, -0.72f,  0.0f, 1.0f, 0.0f,
        -0.72f,  0.72f,  0.0f, 0.0f, 1.0f,
         0.72f,  0.72f,  0.0f, 1.0f, 1.0f,
    };

    const Uint32 ticks = SDL_GetTicks();
    const float phase = (float)(ticks % 4000u) / 4000.0f;
    const float blue = 0.10f + 0.18f * phase;
    glViewport(0, 0, width, height);
    glClearColor(0.025f, 0.06f, blue, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glUniform1i(glGetUniformLocation(program, "u_texture"), 0);
    glVertexAttribPointer(0u, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat),
                          vertices);
    glVertexAttribPointer(1u, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat),
                          vertices + 3);
    glEnableVertexAttribArray(0u);
    glEnableVertexAttribArray(1u);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        log_message("[gles] draw failed (0x%04X)\n", error);
        return false;
    }

    if (!*copy_tested) {
        GLuint copy_texture = 0u;
        glGenTextures(1, &copy_texture);
        glBindTexture(GL_TEXTURE_2D, copy_texture);
        glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 0, 0, width, height, 0);
        error = glGetError();
        *copy_succeeded = error == GL_NO_ERROR;
        log_message("[gles] framebuffer color-copy %s (0x%04X)\n",
                *copy_succeeded ? "passed" : "failed", error);
        if (copy_texture != 0u)
            glDeleteTextures(1, &copy_texture);
        *copy_tested = true;
    }
    return true;
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    const bool sd_mounted = fsdevGetDeviceFileSystem("sdmc") != NULL;
    g_log = sd_mounted ? open_probe_log() : NULL;
    if (!sd_mounted)
        fprintf(stderr, "[gles] libnx sdmc device is unavailable\n");
    log_message("[gles] SDL2/switch-mesa public-path probe started\n");

    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        log_message("[gles] SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    (void)SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                              SDL_GL_CONTEXT_PROFILE_ES);
    (void)SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    (void)SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    (void)SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    (void)SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);

    SDL_Window* window = SDL_CreateWindow(
        "Wind Waker Recomp - GLES feasibility probe", SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED, 1280, 720,
        SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN);
    if (window == NULL) {
        log_message("[gles] SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_GLContext context = SDL_GL_CreateContext(window);
    if (context == NULL) {
        log_message("[gles] SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    const int swap_interval_result = SDL_GL_SetSwapInterval(1);
    int drawable_width = 0;
    int drawable_height = 0;
    int depth_size = 0;
    SDL_GL_GetDrawableSize(window, &drawable_width, &drawable_height);
    (void)SDL_GL_GetAttribute(SDL_GL_DEPTH_SIZE, &depth_size);
    const GLubyte* gl_version = glGetString(GL_VERSION);
    const GLubyte* gl_renderer = glGetString(GL_RENDERER);
    log_message("[gles] GL_VERSION=%s\n",
                gl_version != NULL ? (const char*)gl_version : "unavailable");
    log_message("[gles] GL_RENDERER=%s\n",
                gl_renderer != NULL ? (const char*)gl_renderer : "unavailable");
    log_message("[gles] drawable=%dx%d depth_bits=%d swap_interval=%d\n",
                drawable_width, drawable_height, depth_size,
                swap_interval_result);

    const GLuint program = create_textured_program();
    const GLuint texture = create_test_texture();
    if (program == 0u || texture == 0u) {
        if (texture != 0u)
            glDeleteTextures(1, &texture);
        if (program != 0u)
            glDeleteProgram(program);
        SDL_GL_DeleteContext(context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    log_message("[gles] rendering colored texture; press + to exit\n");
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);
    bool copy_tested = false;
    bool copy_succeeded = false;
    bool running = true;
    bool runtime_ok = true;
    const Uint32 start_ticks = SDL_GetTicks();
    Uint32 interval_start = start_ticks;
    Uint32 interval_frames = 0u;
    Uint32 total_frames = 0u;
    while (appletMainLoop() && running) {
        padUpdate(&pad);
        if ((padGetButtonsDown(&pad) & HidNpadButton_Plus) != 0u)
            break;

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT)
                running = false;
        }

        if (!draw_test_frame(program, texture, drawable_width, drawable_height,
                             &copy_tested, &copy_succeeded)) {
            runtime_ok = false;
            break;
        }
        SDL_GL_SwapWindow(window);
        interval_frames++;
        total_frames++;

        const Uint32 now = SDL_GetTicks();
        const GLenum swap_error = glGetError();
        if (swap_error != GL_NO_ERROR) {
            log_message("[gles] GL error after swap (0x%04X)\n", swap_error);
            runtime_ok = false;
            break;
        }
        const Uint32 interval_ms = now - interval_start;
        if (interval_ms >= 5000u) {
            const float fps = (float)interval_frames * 1000.0f /
                              (float)interval_ms;
            log_message("[gles] interval_ms=%u frames=%u fps=%.1f\n",
                        interval_ms, interval_frames, fps);
            interval_start = now;
            interval_frames = 0u;
        }
    }

    const Uint32 runtime_ms = SDL_GetTicks() - start_ticks;
    glDeleteTextures(1, &texture);
    glDeleteProgram(program);
    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    log_message("[gles] runtime_ms=%u frames=%u framebuffer_copy=%s\n",
                runtime_ms, total_frames,
                copy_tested && copy_succeeded ? "passed" : "failed");
    if (g_log != NULL) {
        fputs("[gles] probe stopped\n", g_log);
        fclose(g_log);
    }
    return runtime_ok && copy_tested && copy_succeeded ? 0 : 1;
}

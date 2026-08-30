#include "egl_renderer.hpp"
#include <android/log.h>

#define TAG "VMEngine_EGL"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

static const char* VERTEX_SHADER =
    "attribute vec4 a_Position;\n"
    "attribute vec2 a_TexCoord;\n"
    "varying vec2 v_TexCoord;\n"
    "void main() {\n"
    "    gl_Position = a_Position;\n"
    "    v_TexCoord = a_TexCoord;\n"
    "}\n";

static const char* FRAGMENT_SHADER =
    "precision mediump float;\n"
    "varying vec2 v_TexCoord;\n"
    "uniform sampler2D u_Texture;\n"
    "void main() {\n"
    "    gl_FragColor = texture2D(u_Texture, v_TexCoord);\n"
    "}\n";

EGLRenderer::EGLRenderer() {}
EGLRenderer::~EGLRenderer() {
    destroy();
}

bool EGLRenderer::init(ANativeWindow* window) {
    std::lock_guard<std::mutex> lock(egl_mutex);
    destroy();

    if (window == nullptr) return false;

    egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (egl_display == EGL_NO_DISPLAY) return false;

    eglInitialize(egl_display, nullptr, nullptr);

    const EGLint attribs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_BLUE_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_RED_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_NONE
    };

    EGLConfig config;
    EGLint numConfigs;
    eglChooseConfig(egl_display, attribs, &config, 1, &numConfigs);

    egl_surface = eglCreateWindowSurface(egl_display, config, window, nullptr);

    const EGLint contextAttribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };
    egl_context = eglCreateContext(egl_display, config, EGL_NO_CONTEXT, contextAttribs);

    eglMakeCurrent(egl_display, egl_surface, egl_surface, egl_context);

    init_shaders();
    LOGI("EGL / OpenGL ES 2.0 Renderer berhasil diinisialisasi.");
    return true;
}

bool EGLRenderer::init_shaders() {
    auto compile = [](GLenum type, const char* src) -> GLuint {
        GLuint s = glCreateShader(type);
        glShaderSource(s, 1, &src, nullptr);
        glCompileShader(s);
        return s;
    };

    GLuint vs = compile(GL_VERTEX_SHADER, VERTEX_SHADER);
    GLuint fs = compile(GL_FRAGMENT_SHADER, FRAGMENT_SHADER);

    gl_program = glCreateProgram();
    glAttachShader(gl_program, vs);
    glAttachShader(gl_program, fs);
    glLinkProgram(gl_program);

    glGenTextures(1, &gl_texture);
    glBindTexture(GL_TEXTURE_2D, gl_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    return true;
}

void EGLRenderer::render_texture(const void* pixel_data, int width, int height) {
    std::lock_guard<std::mutex> lock(egl_mutex);
    if (egl_display == EGL_NO_DISPLAY || pixel_data == nullptr) return;

    eglMakeCurrent(egl_display, egl_surface, egl_surface, egl_context);

    glViewport(0, 0, width, height);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(gl_program);

    glBindTexture(GL_TEXTURE_2D, gl_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixel_data);

    static const GLfloat vertices[] = {
        -1.0f,  1.0f, 0.0f, 0.0f,
        -1.0f, -1.0f, 0.0f, 1.0f,
         1.0f,  1.0f, 1.0f, 0.0f,
         1.0f, -1.0f, 1.0f, 1.0f
    };

    GLint posAttr = glGetAttribLocation(gl_program, "a_Position");
    GLint texAttr = glGetAttribLocation(gl_program, "a_TexCoord");

    glVertexAttribPointer(posAttr, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), vertices);
    glVertexAttribPointer(texAttr, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), &vertices[2]);

    glEnableVertexAttribArray(posAttr);
    glEnableVertexAttribArray(texAttr);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    eglSwapBuffers(egl_display, egl_surface);
}

void EGLRenderer::destroy() {
    if (egl_display != EGL_NO_DISPLAY) {
        eglMakeCurrent(egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (egl_context != EGL_NO_CONTEXT) eglDestroyContext(egl_display, egl_context);
        if (egl_surface != EGL_NO_SURFACE) eglDestroySurface(egl_display, egl_surface);
        eglTerminate(egl_display);
    }
    egl_display = EGL_NO_DISPLAY;
    egl_surface = EGL_NO_SURFACE;
    egl_context = EGL_NO_CONTEXT;
}

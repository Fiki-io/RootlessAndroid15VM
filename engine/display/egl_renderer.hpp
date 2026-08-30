#pragma once

#include <android/native_window.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <mutex>

class EGLRenderer {
public:
    EGLRenderer();
    ~EGLRenderer();

    bool init(ANativeWindow* window);
    void render_texture(const void* pixel_data, int width, int height);
    void destroy();

private:
    EGLDisplay egl_display = EGL_NO_DISPLAY;
    EGLSurface egl_surface = EGL_NO_SURFACE;
    EGLContext egl_context = EGL_NO_CONTEXT;
    GLuint gl_program = 0;
    GLuint gl_texture = 0;

    std::mutex egl_mutex;

    bool init_shaders();
};

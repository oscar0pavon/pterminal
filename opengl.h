#ifndef OPENGL_H
#define OPENGL_H

#include <GL/gl.h>
#include <stdbool.h>
#include "color.h"
#include <EGL/egl.h>

void load_font_image(GLuint* texture_pointer);

extern int gl_attributes[4];

extern EGLDisplay egl_display;
extern EGLSurface egl_surface;

#endif

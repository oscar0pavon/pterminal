#ifndef OPENGL_H
#define OPENGL_H

#include <GL/gl.h>
#include <stdbool.h>
#include "color.h"
#include <EGL/egl.h>


extern GLuint font_texture_id;

void init_egl();

void set_ortho_projection(float width, float height);
void load_texture(GLuint* texture_pointer, const char* path);
void gl_draw_rect(PColor color,  float x, float y, float width, float height);

void gl_draw_char(uint8_t character, PColor color, float x, float y,
                  float width, float height);

extern bool is_opengl;

extern int gl_attributes[4];

extern EGLDisplay egl_display;
extern EGLSurface egl_surface;

#endif

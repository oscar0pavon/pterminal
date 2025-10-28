#ifndef OPENGL_H
#define OPENGL_H

#include <GL/gl.h>
#include <stdbool.h>

extern GLuint font_texture_id;

void set_ortho_projection(float width, float height);
void load_texture(GLuint* texture_pointer, const char* path);
void gl_draw_char(char character, float x, float y, float width, float height);

extern bool is_opengl;

#endif

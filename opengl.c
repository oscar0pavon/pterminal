#include "opengl.h"
#include <GL/gl.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <lodepng.h>
#include <math.h>
#include "terminal.h"
#include "types.h"
#include "wayland/wayland.h"
#include "window.h"
#include <wayland-egl-core.h>
#include <wayland-egl.h>
#include <EGL/eglext.h>

EGLDisplay egl_config;
EGLContext egl_context;
EGLDisplay egl_display;
EGLSurface egl_surface;
struct wl_egl_window *egl_window;


bool is_opengl = false;

int gl_attributes[4] = {GLX_DEPTH_SIZE, 16, GLX_DOUBLEBUFFER, None};

typedef struct UV{
    float x;
    float y;
}UV;

GLuint font_texture_id;


void init_egl(){

  if(terminal_window.type == XORG)
    egl_display = eglGetDisplay((EGLNativeDisplayType)xw.display);
  else
    egl_display = eglGetDisplay((EGLNativeDisplayType)wayland_terminal.display);

  if(egl_display == EGL_NO_DISPLAY){
    die("Can't create EGL display");
  }
  printf("Created EGL display\n");

  eglInitialize(egl_display, NULL,NULL);

  printf("Initialized EGL display\n");

  eglBindAPI(EGL_OPENGL_API);

  const EGLint attributes[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT, // Request support for desktop GL
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_NONE
    };
  EGLint configuration_numbers;
  eglChooseConfig(egl_display, attributes, &egl_config, 1, &configuration_numbers);

  egl_context = eglCreateContext(egl_display, egl_config, EGL_NO_CONTEXT, NULL);

  if(terminal_window.type == XORG){

    egl_surface = eglCreateWindowSurface(egl_display, egl_config,
                                       (EGLNativeWindowType)xw.win, NULL);

  } else { // WAYLAND

    egl_window =
        wl_egl_window_create(wayland_terminal.wayland_surface,
                             terminal_window.width, terminal_window.height);

    if (!egl_window) {
      die("Can't create EGL Wayland\n");
    }

    egl_surface = eglCreateWindowSurface(egl_display, egl_config,
                                         (EGLNativeWindowType)egl_window, NULL);
    if(!egl_surface)
      die("Can't create EGL surface\n");
  }
  
  eglMakeCurrent(egl_display, egl_surface, egl_surface, egl_context);

}

void gl_draw_rect(PColor color,  float x, float y, float width, float height){
    glColor4f(color.r, color.g, color.b,1.f); 

    glBegin(GL_QUADS);
        glVertex3f(x, y,-1);
        glVertex3f(x + width, y,-1);
        glVertex3f(x + width, y + height,-1);
        glVertex3f(x, y + height,-1);
    glEnd();
 
    //debug grid red border line
    // glColor4f(1, 0, 0,1.f); 
    // glLineWidth(1);
    // glBegin(GL_LINE_LOOP);
    //     glVertex3f(x, y,-1);
    //     glVertex3f(x + width, y,-1);
    //     glVertex3f(x + width, y + height,-1);
    //     glVertex3f(x, y + height,-1);
    // glEnd();
}

void gl_draw_char(uint8_t character, PColor color, float x, float y,
                  float width, float height) {

  float char_x = character % 16;
  float char_y = floor((float)character / 16);

  float char_size_x = 32.f / 512.f;
  float char_size_y = 32.f / 512.f;

  UV uv1;
  UV uv2;
  UV uv3;
  UV uv4;

  // button left
  uv1.x = char_x * char_size_x;
  uv1.y = char_y * char_size_y;

  // button right
  uv2.x = (char_x + 1) * char_size_x;
  uv2.y = char_y * char_size_y;

  // top right
  uv3.x = (char_x + 1) * char_size_x;
  uv3.y = (char_y + 1) * char_size_y;

  // top left
  uv4.x = char_x * char_size_x;
  uv4.y = (char_y + 1) * char_size_y;

  float alpha_value = 0.f;

  glEnable(GL_BLEND);
  glEnable(GL_TEXTURE_2D);

  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glBindTexture(GL_TEXTURE_2D, font_texture_id);

  glColor3f(color.r, color.g, color.b);

  glBegin(GL_QUADS);
  glTexCoord2f(uv1.x, uv1.y);
  glVertex2f(x, y);
  glTexCoord2f(uv2.x, uv2.y);
  glVertex2f(x + width, y);
  glTexCoord2f(uv3.x, uv3.y);
  glVertex2f(x + width, y + height);
  glTexCoord2f(uv4.x, uv4.y);
  glVertex2f(x, y + height);
  glEnd();

  glDisable(GL_TEXTURE_2D);

  glDisable(GL_BLEND);
}
void set_ortho_projection(float width, float height){


    glLoadIdentity();

    //2D projection where 0,0 is top-left and width,height is bottom-right
    float left = 0.0f;
    float right = (float)width; //window's width
    float bottom = (float)height; //window's height
    float top = 0.0f;
    float near = -1.0f; // Near clipping plane
    float far = 1.0f;   // Far clipping plane

    glOrtho(left, right, bottom, top, near, far);

}


void load_texture(GLuint* texture_pointer, const char* path){

    unsigned int width, height, channels;
    unsigned char* image_data = NULL; 


    unsigned int error = lodepng_decode32_file(&image_data,&width,&height,path);

    

    
    if(image_data && !error){

      glGenTextures(1, texture_pointer);

      glBindTexture(GL_TEXTURE_2D, *texture_pointer);

      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);


      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
                   GL_UNSIGNED_BYTE, image_data);


      free(image_data);
    }else{
        printf("Can't load image texture\n");
    }

}

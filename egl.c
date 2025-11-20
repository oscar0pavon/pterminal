#include "egl.h"
#include "terminal.h"
#include "window.h"
#include <EGL/eglplatform.h>
#include <stdio.h>
#include <wayland-egl-core.h>
#include <EGL/egl.h>
#include "wayland/wayland.h"
#include <EGL/eglext.h>
#include <unistd.h>

EGLDisplay egl_config;
EGLContext egl_context;
EGLDisplay egl_display;
EGLSurface egl_surface;
struct wl_egl_window *egl_window;

void handle_egl_error(EGLSurface surface) {

  if (egl_surface == EGL_NO_SURFACE) { // Use EGL_NO_SURFACE for comparison
    EGLint error = eglGetError();
    fprintf(stderr, "EGL Surface creation failed! Error code: 0x%x\n", error);

    // Interpret the error code:
    switch (error) {
    case EGL_BAD_DISPLAY:
      fprintf(stderr, "Reason: Invalid EGLDisplay handle.\n");
      break;
    case EGL_NOT_INITIALIZED:
      fprintf(
          stderr,
          "Reason: EGL display was not initialized with eglInitialize().\n");
      break;
    case EGL_BAD_CONFIG:
      fprintf(stderr,
              "Reason: The EGLConfig is incompatible with window surfaces.\n");
      break;
    case EGL_BAD_ATTRIBUTE:
      fprintf(stderr,
              "Reason: Invalid attributes passed in the last argument.\n");
      break;
    case EGL_BAD_MATCH:
      fprintf(stderr, "Reason: The EGLNativeWindowType (wl_egl_window) is "
                      "invalid or incompatible.\n");
      break;
    case EGL_BAD_ALLOC:
      fprintf(stderr, "Reason: Ran out of memory or hardware resources.\n");
      break;
    
    case EGL_BAD_NATIVE_WINDOW:
      fprintf(
          stderr,
          "Reason: Invalid native window provided (e.g., Wayland surface).\n");
      break;
    default:
      fprintf(stderr, "Reason: Unknown EGL error.\n");
      break;
    }
    // Handle the failure: cleanup and exit
  }
}

void init_egl(){

  if(terminal_window.type == XORG)
    egl_display = eglGetDisplay((EGLNativeDisplayType)xw.display);
  else{

    //egl_display = eglGetDisplay((EGLNativeDisplayType)wayland_terminal.display);
    if(wayland_terminal.display == NULL){
      die("Wayland display NULL\n");
    }
    egl_display = eglGetPlatformDisplay(EGL_PLATFORM_WAYLAND_KHR,
                                        (void *)wayland_terminal.display, NULL);
  }

  if(egl_display == EGL_NO_DISPLAY){
    die("Can't create EGL display\n");
  }


  EGLBoolean success = eglInitialize(egl_display, NULL, NULL);
  if (success == EGL_FALSE) {
    EGLint error = eglGetError();
    fprintf(stderr, "FATAL EGL Error during initialization! Code: 0x%x\n",
            error);
    sleep(1);
    die("EGL not initialized\n");
  }

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
    if(egl_surface == EGL_NO_SURFACE){
      handle_egl_error(egl_surface);
      die("Can't create EGL surface\n");
    }
    
  }
  
  eglMakeCurrent(egl_display, egl_surface, egl_surface, egl_context);

  
  printf("EGL initialized successfully\n");
}


#ifndef WAYLAND_H
#define WAYLAND_H

#include "protocol.h"

#include <stdbool.h>

typedef struct wl_buffer_listener BufferListener;
typedef struct xdg_surface_listener SurfaceListener;
typedef struct wl_registry_listener RegistryListener;
typedef struct xdg_surface DesktopSurface;
typedef struct wl_registry Registry;
typedef struct wl_buffer Buffer;

typedef struct WaylandTerminal{
    struct wl_display *display;
    Registry *registry;
    struct wl_compositor *compositor;
    struct xdg_wm_base *desktop;
    struct wl_surface *wayland_surface;
    struct xdg_surface *desktop_surface;
    struct xdg_toplevel *window;
}WaylandTerminal;

extern WaylandTerminal wayland_terminal;

bool init_wayland(void);

#endif

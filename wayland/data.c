#include "data.h"
#include "wayland.h"
#include <wayland-client-protocol.h>
#include <string.h>
#include "mouse.h"

static void data_offer_handle_offer(void *data, struct wl_data_offer *offer,
                                    const char *mime_type) {
    // Check if the mime_type is something your terminal likes (text/plain)
    if (strcmp(mime_type, "text/plain") == 0 || strcmp(mime_type, "UTF8_STRING") == 0) {
        // Mark this offer as "useful" for pasting
    }
}

static const struct wl_data_offer_listener data_offer_listener = {
    .offer = data_offer_handle_offer,
};

static void data_device_handle_data_offer(void *data, struct wl_data_device *data_device,
                                          struct wl_data_offer *offer) {
    // This is called when a new piece of data is "offered" to the seat.
    // You should add a listener to the offer to see what MimeTypes it supports.
    wl_data_offer_add_listener(offer, &data_offer_listener, data);
}

static void data_device_handle_enter(void *data, struct wl_data_device *data_device,
                                     uint32_t serial, struct wl_surface *surface,
                                     wl_fixed_t x, wl_fixed_t y, struct wl_data_offer *offer) {
    // Used for Drag and Drop. Usually ignored in basic terminal clipboard.
}

static void data_device_handle_leave(void *data, struct wl_data_device *data_device) {
    // Used for Drag and Drop.
}

static void data_device_handle_motion(void *data, struct wl_data_device *data_device,
                                      uint32_t time, wl_fixed_t x, wl_fixed_t y) {
    // Used for Drag and Drop.
}

static void data_device_handle_drop(void *data, struct wl_data_device *data_device) {
    // Used for Drag and Drop.
}

static void data_device_handle_selection(void *data, struct wl_data_device *data_device,
                                         struct wl_data_offer *offer) {
    //struct wayland_terminal *term = data;

    if(wayland_terminal.active_data_offer){
      wl_data_offer_destroy(wayland_terminal.active_data_offer);
    }

    wayland_terminal.active_data_offer = offer;

    // This 'offer' is now the current clipboard content.
    // You typically store this pointer to use when the user hits 'Paste'.
    //term->current_selection = offer;
}

static const struct wl_data_device_listener data_device_listener = {
    .data_offer = data_device_handle_data_offer,
    .enter = data_device_handle_enter,
    .leave = data_device_handle_leave,
    //.motion = pointer_handle_motion,
    .drop = data_device_handle_drop,
    .selection = data_device_handle_selection,
};

void configure_data(){

  wayland_terminal.data_device = wl_data_device_manager_get_data_device(
      wayland_terminal.data_device_manager, 
      wayland_terminal.seat);

  wl_data_device_add_listener(wayland_terminal.data_device, 
      &data_device_listener, 
      &wayland_terminal);

}

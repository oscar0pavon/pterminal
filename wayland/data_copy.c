#include "data_copy.h"

#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <wayland-client-core.h>
#include "wayland.h"
#include "../selection.h"
#include "primary_selection.h"
#include "selection.h"
#include "mouse.h"

void copy_data_to_fd(const char* mime_type, int32_t fd){

 if (strcmp(mime_type, "text/plain;charset=utf-8") == 0 ||
      strcmp(mime_type, "UTF8_STRING") == 0 ||
      strcmp(mime_type, "text/plain") == 0) {

     char* my_selection = getsel();

      write(fd, my_selection, strlen(my_selection));

  }
  close(fd);
  printf("send data to clipboard\n");

}

static void primary_source_handle_send(void *data,
        struct zwp_primary_selection_source_v1 *source,
        const char *mime_type, int32_t fd) {
  copy_data_to_fd(mime_type,fd);
}

static void primary_source_handle_cancelled(void *data,
        struct zwp_primary_selection_source_v1 *source) {
    // Someone else highlighted something or selection cleared.
    zwp_primary_selection_source_v1_destroy(source);
}

static const struct zwp_primary_selection_source_v1_listener primary_source_listener = {
    .send = primary_source_handle_send,
    .cancelled = primary_source_handle_cancelled,
};

static void data_source_handle_send(void *data, struct wl_data_source *source,
                                   const char *mime_type, int32_t fd) {

  copy_data_to_fd(mime_type,fd);
}

static void data_source_handle_cancelled(void *data, struct wl_data_source *source) {
    // Fired when someone else copies something new; clean up your source here
    wl_data_source_destroy(source);
    printf("cancelled clipboard\n");
}

static const struct wl_data_source_listener data_source_listener = {
    .send = data_source_handle_send,
    .cancelled = data_source_handle_cancelled,
};

void perform_copy_primary(){
  struct zwp_primary_selection_source_v1 *source = 
        zwp_primary_selection_device_manager_v1_create_source(
            wayland_terminal.primary_selection_manager);

  zwp_primary_selection_source_v1_add_listener(source, 
                                              &primary_source_listener, 
                                              NULL);

  zwp_primary_selection_source_v1_offer(source, "text/plain;charset=utf-8");

  zwp_primary_selection_source_v1_offer(source, "UTF8_STRING");

  zwp_primary_selection_device_v1_set_selection(primary_selection.device, 
                                                source, 
                                                main_mouse.last_input_serial);


  wl_display_flush(wayland_terminal.display);

  printf("perfomed primary copy\n");
  
}

void perform_copy( uint32_t serial) {

    struct wl_data_source *source = wl_data_device_manager_create_data_source(
        wayland_terminal.data_device_manager);

    wl_data_source_add_listener(source, &data_source_listener, NULL);
    
    wl_data_source_offer(source, "text/plain;charset=utf-8"); // Most important for modern apps
    wl_data_source_offer(source, "UTF8_STRING");               // Legacy X11/XWayland compatibility
    wl_data_source_offer(source, "text/plain");                // Standard fallback
    
    wl_data_device_set_selection(wayland_terminal.data_device, source, serial);
    wl_display_flush(wayland_terminal.display);
}

#include "data_copy.h"

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <wayland-client-core.h>
#include "wayland.h"

const char *my_clipboard_content = "test string for copy";

static void data_source_handle_send(void *data, struct wl_data_source *source,
                                   const char *mime_type, int32_t fd) {

   if (strcmp(mime_type, "text/plain;charset=utf-8") == 0 ||
        strcmp(mime_type, "UTF8_STRING") == 0 ||
        strcmp(mime_type, "text/plain") == 0) {

        write(fd, my_clipboard_content, strlen(my_clipboard_content));

    }
    close(fd);
    printf("send data to clipboard\n");
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

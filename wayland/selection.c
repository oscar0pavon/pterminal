#include "selection.h"
#include "primary_selection.h"
#include "wayland.h"

PrimarySelection primary_selection;

static void primary_offer_handle_offer(void *data,
        struct zwp_primary_selection_offer_v1 *offer,
        const char *mime_type) {
    // Record that this offer supports 'mime_type'
    // (Use the same logic as your standard clipboard code)
}

static const struct zwp_primary_selection_offer_v1_listener primary_offer_listener = {
    .offer = primary_offer_handle_offer,
};

static void primary_selection_device_handle_data_offer(void *data,
        struct zwp_primary_selection_device_v1 *device,
        struct zwp_primary_selection_offer_v1 *offer) {
    // 1. A new offer exists. Add a listener to it to get MIME types.
    zwp_primary_selection_offer_v1_add_listener(offer, &primary_offer_listener, data);
}

static void primary_selection_device_handle_selection(void *data,
        struct zwp_primary_selection_device_v1 *device,
        struct zwp_primary_selection_offer_v1 *offer) {
  if(primary_selection.offer){
    zwp_primary_selection_offer_v1_destroy(primary_selection.offer);
  }
  primary_selection.offer = offer;
}

static const struct zwp_primary_selection_device_v1_listener primary_device_listener = {
    .data_offer = primary_selection_device_handle_data_offer,
    .selection = primary_selection_device_handle_selection,
};

void configure_selection(void){
 primary_selection.device =
   zwp_primary_selection_device_manager_v1_get_device(
       wayland_terminal.primary_selection_manager, 
       wayland_terminal.seat);

 zwp_primary_selection_device_v1_add_listener(primary_selection.device, 
     &primary_device_listener, NULL);
}


#include "copy_paste.h"
#include "primary_selection.h"
#include "wayland.h"
#include <unistd.h>
#include <stdio.h>
#include <wayland-client-protocol.h>
#include "../tty.h"
#include "selection.h"

void paste_from_clipboard(bool is_primary){
  if(!is_primary){
    if(!wayland_terminal.active_data_offer){
      printf("None data to paste\n");
      return;
    }
  }

  int fds[2];
  if (pipe(fds) == -1) return; // Create a Unix pipe

  if(!is_primary){

    if(wayland_terminal.active_data_offer){

      wl_data_offer_receive(wayland_terminal.active_data_offer, 
          "text/plain", fds[1]);
    }

  }else{

    if(primary_selection.offer){
      zwp_primary_selection_offer_v1_receive(primary_selection.offer,
          "text/plain", fds[1]);
    }
  }

  close(fds[1]);

  wl_display_dispatch(wayland_terminal.display);

  char buffer[1024];
  ssize_t data_lenght;
  while ((data_lenght = read(fds[0], buffer, sizeof(buffer) - 1)) > 0) {
      buffer[data_lenght] = '\0';
      write_to_tty(buffer, data_lenght, 0);
      //printf("Received text: %s\n", buffer);
  }
  
  close(fds[0]);
}

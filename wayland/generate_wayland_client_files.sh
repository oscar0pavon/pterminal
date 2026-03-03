#!/bin/sh

wayland-scanner client-header \
  /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml protocol.h
wayland-scanner private-code \
  /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml protocol.c


wayland-scanner client-header \
  /usr/share/wayland-protocols/unstable/primary-selection/primary-selection-unstable-v1.xml \
  primary_selection.h
wayland-scanner private-code \
  /usr/share/wayland-protocols/unstable/primary-selection/primary-selection-unstable-v1.xml \
  primary_selection.c

#!/bin/sh

wayland-scanner client-header /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml protocol.h
wayland-scanner private-code /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml protocol.c

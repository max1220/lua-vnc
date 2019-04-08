#!/bin/bash

# Adjust to your needs. lua5.1 is ABI-Compatible with luajit.
CFLAGS= -O3 -Wall -Wextra -fPIC -I/usr/include/lua5.1 -Ilibvncserver -Ilibvncserver/libvncserver
LIBS= -shared -llua5.1 -lvncserver

.PHONY: clean all
.DEFAULT_GOAL := all

all: vnc.so keysymdef.lua

vnc.so: vnc.c
	$(CC) -o $@ $(CFLAGS) $(LIBS) $<
	strip $@

keysymdef.h:
	wget "https://cgit.freedesktop.org/xorg/proto/x11proto/plain/keysymdef.h"

keysymdef.lua: keysymdef.h
	./defines_to_table.lua keysymdef.h > keysymdef.lua


clean:
	rm -f vnc.so

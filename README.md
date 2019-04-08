lua-vnc
=======





Dependencys
-----------
lua-vnc has no external dependencys. libvncserver has optional dependenciess for extra functionallity: libssl, libz, libpng, libjpeg.
On Ubuntu-like systems you can install them via:

`sudo apt install libssl-dev zlib1g-dev libpng12-dev libjpeg62-turbo-dev libgcrypt20-dev`




Install
-------

```
git submodule update --init
cd libvncserver
./autogen.sh
make
cd ..
make

```

There is no installation, just copy the lua-vnc.so file to where ever you need it.
You can check where lua looks for modules with: `lua -e "print(package.cpath:gsub(';', '\n'):gsub('?', '[filename]'))"`




Usage
-----

TODO



	vnc_server = vnc.new_server(w,h)


	active = vnc_server:is_active()


	vnc_server:process_events()


	key_ev = vnc_server:read_key()


	mouse_ev = vnc_server:read_mouse()


	vnc_server:draw_from_drawbuffer(db, target_x,target_y,origin_x,origin_y,w,h, true)





Examples
---------

See examples/test.lua for a minimal paint-like application over vnc(Draw with leftclick)




Todo
-----

Documentation
Scaling
VNC client
performance testing
Write more todo

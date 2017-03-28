lua-vnc
=======

...




Dependencys
-----------
lua-vnc has no external dependencys. libvncserver has optional dependencys for extra functionallity: libssl, libz, libpng, libjpeg.
On Ubuntu-like systems you can install them via:

`sudo apt install libssl-dev zlib1g-dev libpng12-dev libjpeg62-turbo-dev`




Install
-------

```
git submodule update --init && make
cd libvncserver
./autogen.sh
make

```

There is no installation, just copy the lua-vnc.so file to where ever you need it.
You can check where lua looks for modules with: `lua -e "print(package.cpath:gsub(';', '\n'):gsub('?', '[filename]'))"`




Usage
-----

...




Examples
--------
See examples/ folder.

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>

#include <stdint.h>
#include <fcntl.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <linux/fb.h>

#include "lua.h"
#include "lauxlib.h"

#include "lfb.h"
#include "libvncserver/rfb/rfb.h"

#define VERSION "0.1"

#define BITS_PER_SAMPLE 8
#define SAMPLES_PER_PIXEL 3
#define BYTES_PER_PIXEL 4

#define LUA_T_PUSH_S_N(S, N) lua_pushstring(L, S); lua_pushnumber(L, N); lua_settable(L, -3);
#define LUA_T_PUSH_S_S(S, S2) lua_pushstring(L, S); lua_pushstring(L, S2); lua_settable(L, -3);
#define LUA_T_PUSH_S_CF(S, CF) lua_pushstring(L, S); lua_pushcfunction(L, CF); lua_settable(L, -3);


typedef struct {
    int w, h;
    rfbScreenInfoPtr screen;
} server_t;



static int vnc_server_is_active(lua_State *L) {
    server_t *server = (server_t *)lua_touserdata(L, 1);
    rfbScreenInfoPtr screen = server->screen;
    lua_pushboolean(L, rfbIsActive(screen));
    return 1;
}

static int vnc_server_process_events(lua_State *L) {
    server_t *server = (server_t *)lua_touserdata(L, 1);
    rfbScreenInfoPtr screen = server->screen;
    long usec = server->screen->deferUpdateTime*1000;
    rfbProcessEvents(screen,usec);
    return 0;
}

static int vnc_server_set_pixel(lua_State *L) {
    server_t *server = (server_t *)lua_touserdata(L, 1);
    rfbScreenInfoPtr screen = server->screen;
    int x = lua_tointeger(L, 1);
    int y = lua_tointeger(L, 2);
    int r = lua_tointeger(L, 3);
    int g = lua_tointeger(L, 4);
    int b = lua_tointeger(L, 5);
    
	screen->frameBuffer[(y*server->w+x)*BYTES_PER_PIXEL+0] = r;
    screen->frameBuffer[(y*server->w+x)*BYTES_PER_PIXEL+1] = g;
    screen->frameBuffer[(y*server->w+x)*BYTES_PER_PIXEL+2] = b;

	if (lua_toboolean(L, 5)) {
		rfbMarkRectAsModified(server->screen,x,y,1,1);
	}

    return 0;
}

static int vnc_server_draw_from_drawbuffer(lua_State *L) {
    server_t *server = (server_t *)lua_touserdata(L, 1);
    rfbScreenInfoPtr screen = server->screen;
    drawbuffer_t *db = (drawbuffer_t *)lua_touserdata(L, 2);
    
    int target_x = lua_tointeger(L, 3);
    int target_y = lua_tointeger(L, 4);
    
    int origin_x = lua_tointeger(L, 5);
    int origin_y = lua_tointeger(L, 6);
    
    int w = lua_tointeger(L, 7);
    int h = lua_tointeger(L, 8);
    
    int cx;
    int cy;
    
    pixel_t p;

    for (cy=0; cy < h; cy=cy+1) {
        for (cx=0; cx < w; cx=cx+1) {
            if (origin_x+cx < 0 || origin_x+cx >= db->w || \
            	origin_y+cy < 0 || origin_y+cy >= db->h || \
            	target_x+cx < 0 || target_x + cx >= server->w ||
            	target_y+cy < 0 || target_x + cx >= server->w) {
            	continue;
            }
            p = db->data[(cy+origin_y)*(db->w)+cx+origin_x];
            
            screen->frameBuffer[((target_y+cy)*server->w+target_x+cx)*BYTES_PER_PIXEL+0] = p.r;
    		screen->frameBuffer[((target_y+cy)*server->w+target_x+cx)*BYTES_PER_PIXEL+1] = p.g;
    		screen->frameBuffer[((target_y+cy)*server->w+target_x+cx)*BYTES_PER_PIXEL+2] = p.b;
            
        }
    }

    return 0;
    
    
    
    
    return 0;
}

static int vnc_server_update_rect(lua_State *L) {
    server_t *server = (server_t *)lua_touserdata(L, 1);
    rfbScreenInfoPtr screen = server->screen;
    
    int x = lua_tointeger(L, 1);
    int y = lua_tointeger(L, 2);
    int w = lua_tointeger(L, 3);
    int h = lua_tointeger(L, 4);
    
    rfbMarkRectAsModified(screen,x,y,w,h);
    
    return 0;
}

static int l_new_server(lua_State *L) {
    server_t *server = (server_t *)lua_newuserdata(L, sizeof(*server));
	rfbScreenInfoPtr screen = rfbGetScreen(NULL, NULL, server->w, server->h, BITS_PER_SAMPLE,SAMPLES_PER_PIXEL, BYTES_PER_PIXEL);
	server->screen = screen;

    server->w = lua_tointeger(L, 1);
    server->h = lua_tointeger(L, 2);

	screen->frameBuffer = calloc(server->w * server->h, BYTES_PER_PIXEL);    
    
    rfbInitServer(screen);
    
    
    lua_createtable(L, 0, 7);
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");

    LUA_T_PUSH_S_N("width", server->w)
    LUA_T_PUSH_S_N("height", server->h)
    LUA_T_PUSH_S_CF("is_active", vnc_server_is_active)
    LUA_T_PUSH_S_CF("process_events", vnc_server_process_events)
    LUA_T_PUSH_S_CF("set_pixel", vnc_server_set_pixel)
    LUA_T_PUSH_S_CF("draw_from_drawbuffer", vnc_server_draw_from_drawbuffer)
    LUA_T_PUSH_S_CF("update_rect", vnc_server_update_rect)
    lua_setmetatable(L, -2);
    
    return 1;
}


LUALIB_API int luaopen_vnc(lua_State *L) {
    lua_newtable(L);

	LUA_T_PUSH_S_S("version", VERSION)
    LUA_T_PUSH_S_CF("new_server", l_new_server)
    // LUA_T_PUSH_S_CF("new_client", l_new_client)
    
 	return 1;
}








//

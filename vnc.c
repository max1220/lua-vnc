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

#include "lua-db.h"

#include "libvncserver/rfb/rfb.h"
#include "libvncserver/rfb/rfbclient.h"

#define VERSION "0.1"

#define BITS_PER_SAMPLE 8
#define SAMPLES_PER_PIXEL 3
#define BYTES_PER_PIXEL 4

#define MAX_KEY_EVENT_COUNT 1024
#define MAX_MOUSE_EVENT_COUNT 1024

#define LUA_T_PUSH_S_B(S, N) lua_pushstring(L, S); lua_pushboolean(L, N); lua_settable(L, -3);
#define LUA_T_PUSH_S_N(S, N) lua_pushstring(L, S); lua_pushnumber(L, N); lua_settable(L, -3);
#define LUA_T_PUSH_S_S(S, S2) lua_pushstring(L, S); lua_pushstring(L, S2); lua_settable(L, -3);
#define LUA_T_PUSH_S_CF(S, CF) lua_pushstring(L, S); lua_pushcfunction(L, CF); lua_settable(L, -3);


typedef struct {
    int w, h;
    rfbScreenInfoPtr screen;
} server_t;

typedef struct {
	rfbBool down;
	rfbKeySym key;
	rfbClientPtr client;
} key_event_t;

typedef struct {
	int button_mask;
	int x;
	int y;
} mouse_event_t;


key_event_t key_event_stack[MAX_KEY_EVENT_COUNT];
int key_event_stack_index = 0;

mouse_event_t mouse_event_stack[MAX_MOUSE_EVENT_COUNT];
int mouse_event_stack_index = 0;


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
    int x = lua_tointeger(L, 2);
    int y = lua_tointeger(L, 3);
    int r = lua_tointeger(L, 4);
    int g = lua_tointeger(L, 5);
    int b = lua_tointeger(L, 6);
    
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
    
    // printf("screen w: %d,   screen h: %d,   db w: %d,   db h: %d \n", screen->width, screen->height, db->w, db->h);
    
    int target_x = lua_tointeger(L, 3);
    int target_y = lua_tointeger(L, 4);
    
    int origin_x = lua_tointeger(L, 5);
    int origin_y = lua_tointeger(L, 6);
    
    int w = lua_tointeger(L, 7);
    int h = lua_tointeger(L, 8);
    
    int cx;
    int cy;
    
    int tx;
    int ty;
    
    int ox;
    int oy;
    
    pixel_t p;

    for (cy=0; cy < h; cy=cy+1) {
        for (cx=0; cx < w; cx=cx+1) {
			tx = target_x+cx;
			ty = target_y+cy;
			ox = origin_x+cx;
			oy = origin_y+cy;
			
            if (ox < 0 || ox >= db->w-1 || \
            	oy < 0 || oy >= db->h-1 || \
            	tx < 0 || tx >= server->w-1 ||
            	ty < 0 || ty >= server->h-1) {
            	continue;
            } else {
				p = db->data[oy*db->w+ox];
				if (p.a > 0) {
					screen->frameBuffer[(ty*server->w+tx)*BYTES_PER_PIXEL+0] = p.r;
					screen->frameBuffer[(ty*server->w+tx)*BYTES_PER_PIXEL+1] = p.g;
					screen->frameBuffer[(ty*server->w+tx)*BYTES_PER_PIXEL+2] = p.b;
				}
			}            
        }
    }
    
    if (lua_toboolean(L, 9)) {
		rfbMarkRectAsModified(server->screen,target_x,target_y,w,h);
	}

    return 0;
}

static int vnc_server_update_rect(lua_State *L) {
    server_t *server = (server_t *)lua_touserdata(L, 1);
    rfbScreenInfoPtr screen = server->screen;
    
    int x = lua_tointeger(L, 2);
    int y = lua_tointeger(L, 3);
    int w = lua_tointeger(L, 4);
    int h = lua_tointeger(L, 5);
    
    rfbMarkRectAsModified(screen,x,y,w,h);
    
    return 0;
}


static int vnc_server_read_key(lua_State *L) {
	key_event_t ev;
	
	if (key_event_stack_index > 0) {
		ev = key_event_stack[key_event_stack_index];
	
	    lua_newtable(L);
		LUA_T_PUSH_S_N("down", (int8_t)ev.down);
		LUA_T_PUSH_S_N("key", (uint32_t)ev.key);
		
		key_event_stack_index = key_event_stack_index - 1;
			
		return 1;
	} else {
		return 0;
	}
}

static int vnc_server_read_mouse(lua_State *L) {
	mouse_event_t ev;
	
	if (mouse_event_stack_index > 0) {
		ev = mouse_event_stack[mouse_event_stack_index];
		
	    lua_newtable(L);
		LUA_T_PUSH_S_N("button_mask", ev.button_mask);
		LUA_T_PUSH_S_N("x", ev.x);
		LUA_T_PUSH_S_N("y", ev.y);
			
		mouse_event_stack_index = mouse_event_stack_index - 1;
		
		return 1;
	} else {
		return 0;
	}
}

static int vnc_server_tostring(lua_State *L) {
    server_t *server = (server_t *)lua_touserdata(L, 1);
    
    lua_pushfstring(L, "vncserver: %dx%d", server->w, server->h);
    
    return 0;
}

static void handle_key(rfbBool down, rfbKeySym key, rfbClientPtr cl) {
	int k;
	if (key_event_stack_index-1 == MAX_KEY_EVENT_COUNT) {
		for (k = key_event_stack_index; k > MAX_KEY_EVENT_COUNT; k--){   
			key_event_stack[k] = key_event_stack[k-1];
		}
	} else {
		key_event_stack_index = key_event_stack_index + 1;
	}
	
	key_event_t ev = {
		.down = down,
		.key = key,
		.client = cl
	};
	key_event_stack[key_event_stack_index] = ev;
}

static void handle_mouse(int button_mask, int x, int y,rfbClientPtr cl) {
	
	int k;
	if (mouse_event_stack_index-1 == MAX_MOUSE_EVENT_COUNT) {
		for (k = mouse_event_stack_index; k > MAX_MOUSE_EVENT_COUNT; k--){   
			mouse_event_stack[k] = mouse_event_stack[k-1];
		}
	} else {
		mouse_event_stack_index = mouse_event_stack_index + 1;
	}
	
	mouse_event_t ev = {
		.button_mask = button_mask,
		.x = x,
		.y = y
	};
	mouse_event_stack[mouse_event_stack_index] = ev;
	
	rfbDefaultPtrAddEvent(button_mask,x,y,cl);
}

static int l_new_server(lua_State *L) {
    server_t *server = (server_t *)lua_newuserdata(L, sizeof(*server));
    server->w = lua_tointeger(L, 1);
    server->h = lua_tointeger(L, 2);

	rfbScreenInfoPtr screen = rfbGetScreen(NULL, NULL, server->w, server->h, BITS_PER_SAMPLE,SAMPLES_PER_PIXEL, BYTES_PER_PIXEL);
	server->screen = screen;
	
	screen->autoPort = TRUE;
	
	screen->kbdAddEvent = handle_key;
	screen->ptrAddEvent = handle_mouse;
	
	screen->frameBuffer = calloc(server->w * server->h, BYTES_PER_PIXEL);    
    
    rfbInitServer(screen);
    
    lua_createtable(L, 0, 10);
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");

    LUA_T_PUSH_S_N("width", server->w)
    LUA_T_PUSH_S_N("height", server->h)
    LUA_T_PUSH_S_CF("is_active", vnc_server_is_active)
    LUA_T_PUSH_S_CF("process_events", vnc_server_process_events)
    LUA_T_PUSH_S_CF("set_pixel", vnc_server_set_pixel)
    LUA_T_PUSH_S_CF("draw_from_drawbuffer", vnc_server_draw_from_drawbuffer)
    LUA_T_PUSH_S_CF("update_rect", vnc_server_update_rect)
    LUA_T_PUSH_S_CF("read_key", vnc_server_read_key)
    LUA_T_PUSH_S_CF("read_mouse", vnc_server_read_mouse)
    LUA_T_PUSH_S_CF("__tostring", vnc_server_tostring)

    lua_setmetatable(L, -2);
    
    return 1;
}


/*
static int l_new_client(lua_State *L) {
	client_t *client = (client_t *)lua_newuserdata(L, sizeof(*client));
	rfbClient* client = rfbGetClient(8,3,24)
	client->serverHost = luaL_checkstring(L, 1);
	client->serverPort = luaL_checkstring(L, 2);
	client->programName = luaL_checkstring(L, 3);
	if (!rfbInitClient(client,NULL, NULL))
		return 0;
}
*/

LUALIB_API int luaopen_vnc(lua_State *L) {
    lua_newtable(L);

	LUA_T_PUSH_S_S("version", VERSION)
    LUA_T_PUSH_S_CF("new_server", l_new_server)
    // LUA_T_PUSH_S_CF("new_client", l_new_client)
    
 	return 1;
}








//

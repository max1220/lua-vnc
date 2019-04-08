#!/usr/bin/env luajit
package.cpath = package.cpath .. ";../?.so"
package.path = package.path .. ";../?.lua"
local ldb = require("ldb")
local vnc = require("vnc")
local bit = require("bit")
local keysymdef = require("keysymdef")

local w = 800
local h = 600

local vnc_server = vnc.new_server(w,h)

local db = ldb.new(w,h)

for y=0, h-1 do
	for x=0, w-1 do
		local c = math.floor((x/w)*255)
		-- vnc_server:set_pixel(x,y,c,c,c)
		db:set_pixel(x,y,c,c,c,255)
	end
end

function key_name(keysym)
	for k,v in pairs(keysymdef) do
		if v == keysym then
			return k:sub(4)
		end
	end
	return ev.key
end

local need_update = true
local last_mouse_x, last_mouse_y
while vnc_server:is_active() do
	vnc_server:process_events()
	
	local key_ev = vnc_server:read_key()
	while key_ev do
		local key = key_name(key_ev.key)
		print("key", key, key_ev.down==1)
		key_ev = vnc_server:read_key()
	end
	
	local mouse_ev = vnc_server:read_mouse()
	while mouse_ev do
		print("mouse", mouse_ev.x, mouse_ev.y, mouse_ev.button_mask)
		if mouse_ev.button_mask ~= 0 then
			-- db:set_pixel(mouse_ev.x,mouse_ev.y,255,0,0,255)
			db:set_line(last_mouse_x or mouse_ev.x, last_mouse_y or mouse_ev.y, mouse_ev.x, mouse_ev.y, 255,0,0,255)
			last_mouse_x = mouse_ev.x
			last_mouse_y = mouse_ev.y
		else
			last_mouse_x = nil
			last_mouse_y = nil
		end
		need_update = true
		mouse_ev = vnc_server:read_mouse()
	end

	if need_update then
		print("Updating...")
		vnc_server:draw_from_drawbuffer(db, 0,0,0,0,w,h, true)
		need_update = false
	end
end

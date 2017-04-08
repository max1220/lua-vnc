#!/usr/bin/env luajit
local lfb = require("lfb")
local vnc = require("vnc")
local bit = require("bit")
local keysymdef = require("keysymdef")

local w = 1024
local h = 768

local vnc_server = vnc.new_server(w,h)

local db = lfb.new_drawbuffer(w,h)

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

while vnc_server:is_active() do
	vnc_server:process_events()
	
	local key_ev = vnc_server:read_key()
	if key_ev then
		local key = key_name(key_ev.key)
		print(key, key_ev.down==1)
	end
	
	local mouse_ev = vnc_server:read_mouse()
	if mouse_ev then
		-- print(mouse_ev.x, mouse_ev.y, mouse_ev.button_mask)
		if mouse_ev.button_mask == 0 then
		
		elseif mouse_ev.button_mask, 8)
		
		if mouse_ev.button_mask ~= 0 then
			db:set_pixel(mouse_ev.x,mouse_ev.y,255,0,0,255)
		end
	end

	vnc_server:draw_from_drawbuffer(db, 0,0,0,0,w,h, true)
end

// This code is licensed under the MIT Open Source License.

// Copyright (c) 2015 Ruairidh Carmichael - ruairidhcarmichael@live.co.uk

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#include "../shared.h"

u32 kDown;
u32 kHeld;
u32 kUp;

touchPosition touch;

bool touchIsDown = false;

int lastPosx = 0;
int lastPosy = 0;

char dsNames[32][32] = {
		"a", "b", "select", "start",
		"dright", "dleft", "dup", "ddown",
		"rbutton", "lbutton", "x", "y",
		"", "", "lzbutton", "rzbutton",
		"", "", "", "",
		"touch", "", "", "",
		"cstickright", "cstickleft", "cstickup", "cstickdown",
		"cpadright", "cpadleft", "cpadup", "cpaddown"
};

char keyNames[32][32] = { // TODO: Improve these.
		"a", "b", "KEY_SELECT", "esc",
		"right", "left", "up", "down",
		"KEY_R", "KEY_L", "x", "y",
		"", "", "KEY_ZL", "KEY_ZR",
		"", "", "", "",
		"touch", "", "", "",
		"KEY_CSTICK_RIGHT", "KEY_CSTICK_LEFT", "KEY_CSTICK_UP", "KEY_CSTICK_DOWN",
		"KEY_CPAD_RIGHT", "KEY_CPAD_LEFT", "KEY_CPAD_UP", "KEY_CPAD_DOWN"
};

int inputScan(lua_State *L) { // love.keyboard.scan()

	hidScanInput();

	kDown = hidKeysDown();
	kHeld = hidKeysHeld();
	kUp = hidKeysUp();

	hidTouchRead(&touch);

	if (kDown & BIT(20)) { // BIT(20) == "touch" -- love.mousepressed()

		touchIsDown = true;

		lua_getfield(L, LUA_GLOBALSINDEX, "love");
		lua_getfield(L, -1, "mousepressed");
		lua_remove(L, -2);

		if (!lua_isnil(L, -1)) {

			lua_pushinteger(L, touch.px);
			lua_pushinteger(L, touch.py);
			lua_pushstring(L, "l");

			lua_call(L, 3, 0);

			lastPosx = touch.px;
			lastPosy = touch.py;

		}

	}

	if (kUp & BIT(20)) { // love.mousereleased()

		touchIsDown = false;

		lua_getfield(L, LUA_GLOBALSINDEX, "love");
		lua_getfield(L, -1, "mousereleased");
		lua_remove(L, -2);

		if (!lua_isnil(L, -1)) {

			lua_pushinteger(L, touch.px);
			lua_pushinteger(L, touch.py);
			lua_pushstring(L, "l");

			lua_call(L, 3, 0);

		}

	}

	int i;
	for (i = 0; i < 32; i++) { // love.keypressed()
		if (kDown & BIT(i)) {
			if (strcmp(keyNames[i], "touch") != 0) { // Touch shouldn't be returned in love.keypressed.
				
				lua_getfield(L, LUA_GLOBALSINDEX, "love");
				lua_getfield(L, -1, "keypressed");
				lua_remove(L, -2);

				if (!lua_isnil(L, -1)) {

					lua_pushstring(L, dsNames[i]);
					lua_pushboolean(L, 0);

					lua_call(L, 2, 0);

				}
			}
		} 
	}

	for (i = 0; i < 32; i++) { // love.keyreleased()
		if (kUp & BIT(i)) {
			if (strcmp(keyNames[i], "touch") != 0) { // Touch shouldn't be returned in love.keypressed.
				
				lua_getfield(L, LUA_GLOBALSINDEX, "love");
				lua_getfield(L, -1, "keyreleased");
				lua_remove(L, -2);

				if (!lua_isnil(L, -1)) {

					lua_pushstring(L, dsNames[i]);

					lua_call(L, 1, 0);

				}
			}
		} 
	}

	return 0;

}

int keyboardIsDown(lua_State *L) { // love.keyboard.isDown()

	const char *key = luaL_checkstring(L, 1);

	int boolval = 0; // 0 == false; 1 == true

	int i;
	for (i = 0; i < 32; i++) {
		if (kHeld & BIT(i)) {
			if (strcmp(keyNames[i], "touch") != 0) { // Touch events should probably not be returned in love.keyboard.
				if (strcmp(key, keyNames[i]) == 0 || strcmp(key, dsNames[i]) == 0) {
					boolval = 1;
				}
			}
		} 
	}

	lua_pushboolean(L, boolval);

	return 1;

}

int mouseIsDown(lua_State *L) { // love.mouse.isDown()

	lua_pushboolean(L, touchIsDown);

	return 1;

}

int mouseGetX(lua_State *L) { // love.mouse.getX()

	lua_pushinteger(L, touch.px);

	return 1;

}

int mouseGetY(lua_State *L) { // love.mouse.getY()

	lua_pushinteger(L, touch.py);

	return 1;

}

int mouseGetPosition(lua_State *L) { // love.mouse.getPosition()

	lua_pushinteger(L, touch.px);
	lua_pushinteger(L, touch.py);

	return 2;

}

int initLoveKeyboard(lua_State *L) {

	luaL_Reg reg[] = {
		{ "scan",	inputScan		},
		{ "isDown",	keyboardIsDown	},
		{ 0, 0 },
	};

	luaL_newlib(L, reg);

	return 1;

}

int initLoveMouse(lua_State *L) {

	luaL_Reg reg[] = {
		{ "isDown",			mouseIsDown			},
		{ "getX",			mouseGetX			},
		{ "getY",			mouseGetY			},
		{ "getPosition",	mouseGetPosition	},
		{ 0, 0 },
	};

	luaL_newlib(L, reg);

	return 1;

}
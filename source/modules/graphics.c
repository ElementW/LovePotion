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

struct Color {

	u8 r, g, b, a;

};

typedef enum BlendMode {

	BlendMode_alpha,
	BlendMode_replace,
	BlendMode_screen,
	BlendMode_add,
	BlendMode_subtract,
	BlendMode_multiply

} BlendMode;

typedef enum BlendAlphaMode {

	BlendAlphaMode_alphamultiply,
	BlendAlphaMode_premultiplied

} BlendAlphaMode;

struct Coord {

	double x, y;

};

struct Transform {

	struct Coord translation, shear;
	double rotation;

};

struct GraphicsState {

	struct Color bg, fg;

	BlendMode blendMode;
	BlendAlphaMode blendAlphaMode;

	struct Transform transform;

} currentState;

int currentScreen = GFX_BOTTOM;

love_font *currentFont;

bool isPushed = false;

bool is3D = false;

int currentDepth = 0;

u32 defaultFilter = GPU_TEXTURE_MAG_FILTER(GPU_LINEAR)|GPU_TEXTURE_MIN_FILTER(GPU_LINEAR); // Default Image Filter.
char *defaultMinFilter = "linear";
char *defaultMagFilter = "linear";

u32 getCurrentColor() {

	return RGBA8(currentState.fg.r, currentState.fg.g, currentState.fg.b, currentState.fg.a);
 
}

int translateCoords(int *x, int *y) {

	// Emulates the functionality of lg.translate

	if (isPushed) {

		*x += currentState.transform.translation.x;
		*y += currentState.transform.translation.y;

	}

	// Sets depth of objects

	if (is3D && sf2d_get_current_screen() == GFX_TOP) {

		float slider = CONFIG_3D_SLIDERSTATE;

		if (sf2d_get_current_side() == GFX_LEFT) *x -= (slider * currentDepth);
		if (sf2d_get_current_side() == GFX_RIGHT) *x += (slider * currentDepth);

	}

	return 0;

}

static int graphicsSetBackgroundColor(lua_State *L) { // love.graphics.setBackgroundColor()

	int r = luaL_checkinteger(L, 1);
	int g = luaL_checkinteger(L, 2);
	int b = luaL_checkinteger(L, 3);

	sf2d_set_clear_color(RGBA8(r, g, b, 0xFF));

	return 0;

}

static int graphicsSetColor(lua_State *L) { // love.graphics.setColor()

	if (lua_isnumber(L, -1)) {

		currentState.fg.r = luaL_checkinteger(L, 1);
		currentState.fg.g = luaL_checkinteger(L, 2);
		currentState.fg.b = luaL_checkinteger(L, 3);
		currentState.fg.a = luaL_optnumber(L, 4, currentState.fg.a);

	} else if (lua_istable(L, -1)) {

		luaError(L, "Table support for setColor is not implemented yet. Use unpack(insertTableHere) until it is.");

	}

	return 0;

}

static int graphicsGetColor(lua_State *L) { // love.graphics.getColor()

	lua_pushnumber(L, currentState.fg.r);
	lua_pushnumber(L, currentState.fg.g);
	lua_pushnumber(L, currentState.fg.b);
	lua_pushnumber(L, currentState.fg.a);

	return 4;

}

static int graphicsRectangle(lua_State *L) { // love.graphics.rectangle()

	if (sf2d_get_current_screen() == currentScreen) {

		char *mode = luaL_checkstring(L, 1);

		int x = luaL_checkinteger(L, 2);
		int y = luaL_checkinteger(L, 3);

		translateCoords(&x, &y);

		int w = luaL_checkinteger(L, 4);
		int h = luaL_checkinteger(L, 5);

		if (strcmp(mode, "fill") == 0) {
			sf2d_draw_rectangle(x, y, w, h, getCurrentColor());
		} else if (strcmp(mode, "line") == 0) {
			sf2d_draw_line(x, y, x, y + h, getCurrentColor());
			sf2d_draw_line(x, y, x + w, y, getCurrentColor());

			sf2d_draw_line(x + w, y, x + w, y + h, getCurrentColor());
			sf2d_draw_line(x, y + h, x + w, y + h, getCurrentColor());
		}

	}

	return 0;

}

static int graphicsCircle(lua_State *L) { // love.graphics.circle()

	if (sf2d_get_current_screen() == currentScreen) {

		int step = 15;

		char *mode = luaL_checkstring(L, 1);
		int x = luaL_checkinteger(L, 2);
		int y = luaL_checkinteger(L, 3);
		int r = luaL_checkinteger(L, 4);

		translateCoords(&x, &y);

		sf2d_draw_line(x, y, x, y, RGBA8(0x00, 0x00, 0x00, 0x00)); // Fixes weird circle bug.
		sf2d_draw_fill_circle(x, y, r, getCurrentColor());

	}

	return 0;

}

static int graphicsLine(lua_State *L) { // love.graphics.line() -- Semi-Broken

	if (sf2d_get_current_screen() == currentScreen) {

		int argc = lua_gettop(L);
		int i = 0;

		if ((argc/2)*2 == argc) {
			for( i; i < argc / 2; i++) {

				int t = i * 4;

				int x1 = luaL_checkinteger(L, t + 1);
				int y1 = luaL_checkinteger(L, t + 2);
				int x2 = luaL_checkinteger(L, t + 3);
				int y2 = luaL_checkinteger(L, t + 4);

				translateCoords(&x1, &y1);
				translateCoords(&x2, &y2);

				sf2d_draw_line(x1, y1, x2, y2, getCurrentColor());

			}
		}

	}

	return 0;

}

static int graphicsGetScreen(lua_State *L) { // love.graphics.getScreen()

	if (currentScreen == GFX_TOP) {
		lua_pushstring(L, "top");
	} else if (currentScreen == GFX_BOTTOM) {
		lua_pushstring(L, "bottom");
	}

	return 1;

}

static int graphicsSetScreen(lua_State *L) { // love.graphics.setScreen()

	char *screen = luaL_checkstring(L, 1);

	if (strcmp(screen, "top") == 0) {
		currentScreen = GFX_TOP;
	} else if (strcmp(screen, "bottom") == 0) {
		currentScreen = GFX_BOTTOM;
	}

	return 0;

}

static int graphicsGetSide(lua_State *L) { // love.graphics.getSide()

	if (sf2d_get_current_side() == GFX_LEFT) {
		lua_pushstring(L, "left");
	} else if (sf2d_get_current_side() == GFX_RIGHT) {
		lua_pushstring(L, "right");
	}

	return 1;

}

static int graphicsPresent(lua_State *L) { // love.graphics.present()

	sf2d_swapbuffers();

	return 0;

}

static int graphicsGetWidth(lua_State *L) { // love.graphics.getWidth()

	int topWidth = 400;
	int botWidth = 320;

	int returnWidth;

	// TODO: Take screen sides into account.

	if (currentScreen == GFX_TOP) {

		returnWidth = topWidth;

	} else if (currentScreen == GFX_BOTTOM) {

		returnWidth = botWidth;

	}

	lua_pushnumber(L, returnWidth);

	return 1;

}

static int graphicsGetHeight(lua_State *L) { // love.graphics.getHeight()

	int returnWidth = 240;

	lua_pushnumber(L, returnWidth);

	return 1;

}

static int graphicsDraw(lua_State *L) { // love.graphics.draw()

	if (sf2d_get_current_screen() == currentScreen) {

		love_image *img = luaobj_checkudata(L, 1, LUAOBJ_TYPE_IMAGE);
		love_quad *quad = NULL;

		int x, y;
		int sx, sy;
		float rad;

		if (!lua_isnone(L, 2) && lua_type(L, 2) != LUA_TNUMBER) {

			quad = luaobj_checkudata(L, 2, LUAOBJ_TYPE_QUAD);
			x = luaL_optnumber(L, 3, 0);
			y = luaL_optnumber(L, 4, 0);
			rad = luaL_optnumber(L, 5, 0);
			sx = luaL_optnumber(L, 6, 0);
			sy = luaL_optnumber(L, 7, 0);

		} else {

			x = luaL_optnumber(L, 2, 0);
			y = luaL_optnumber(L, 3, 0);
			rad = luaL_optnumber(L, 4, 0);
			sx = luaL_optnumber(L, 5, 0);
			sy = luaL_optnumber(L, 6, 0);

		}

		translateCoords(&x, &y);

		if (rad == 0) {
			
            if (sx == 0 && sy == 0){
				
			    if (!quad) {

				    if (img) {
					    sf2d_draw_texture_blend(img->texture, x, y, getCurrentColor());
				    }
				
			    } else {
				    sf2d_draw_texture_part_blend(img->texture, x, y, quad->x, quad->y, quad->width, quad->height, getCurrentColor());
			    }
				
			} else {
				
				if (!quad) {

				    if (img) {
					    sf2d_draw_texture_scale_blend(img->texture, x, y, sx, sy, getCurrentColor());
				    }
				
			    } else {
				    sf2d_draw_texture_part_scale_blend(img->texture, x, y, quad->x, quad->y, quad->width, quad->height, sx, sy, getCurrentColor());
			    }
				
			}

		} else {
            
			sf2d_draw_texture_rotate_blend(img->texture, x + img->texture->width / 2, y + img->texture->height / 2, rad, getCurrentColor());

		}

	}

	return 0;

}

static int graphicsSetFont(lua_State *L) { // love.graphics.setFont()

	currentFont = luaobj_checkudata(L, 1, LUAOBJ_TYPE_FONT);

	return 0;

}

static int graphicsPrint(lua_State *L) { // love.graphics.print()

	if (sf2d_get_current_screen() == currentScreen) {

		if (currentFont) {

			char *printText = luaL_checkstring(L, 1);
			int x = luaL_checkinteger(L, 2);
			int y = luaL_checkinteger(L, 3);

			translateCoords(&x, &y);

			sftd_draw_text(currentFont->font, x, y, getCurrentColor(), currentFont->size, printText);

		}

	}

	return 0;

}

static int graphicsPrintFormat(lua_State *L) {

	if (sf2d_get_current_screen() == currentScreen) {

		if (currentFont) {

			char *printText = luaL_checkstring(L, 1);
			int x = luaL_checkinteger(L, 2);
			int y = luaL_checkinteger(L, 3);
			int limit = luaL_checkinteger(L, 4);
			char *align = luaL_optstring(L, 5, "left");

			int width = sftd_get_text_width(currentFont->font, currentFont->size, printText);

			if (strcmp(align, "center") == 0) {

				if (width < limit) {
					x += (limit / 2) - (width / 2);
				}

			} else if (strcmp(align, "right") == 0) {

				if (width < limit) {
					x += limit - width;
				}

			}

			translateCoords(&x, &y);

			if (x > 0) limit += x; // Quick text wrap fix, needs removing once sf2dlib is updated.

			sftd_draw_text_wrap(currentFont->font, x, y, getCurrentColor(), currentFont->size, limit, printText);

		}

	}

	return 0;

}

static int graphicsPush(lua_State *L) { // love.graphics.push()

	if (sf2d_get_current_screen() == currentScreen) {

		isPushed = true;

	}

	return 0;

}

static int graphicsPop(lua_State *L) { // love.graphics.pop()

	if (sf2d_get_current_screen() == currentScreen) {

		currentState.transform.translation.x = 0;
		currentState.transform.translation.y = 0;
		isPushed = false;

	}

	return 0;

}

static int graphicsOrigin(lua_State *L) { // love.graphics.origin()

	if (sf2d_get_current_screen() == currentScreen) {

		currentState.transform.translation.x = 0;
		currentState.transform.translation.y = 0;

	}

	return 0;

}

static int graphicsTranslate(lua_State *L) { // love.graphics.translate()

	if (sf2d_get_current_screen() == currentScreen) {

		int dx = luaL_checkinteger(L, 1);
		int dy = luaL_checkinteger(L, 2);

		currentState.transform.translation.x = currentState.transform.translation.x + dx;
		currentState.transform.translation.y = currentState.transform.translation.y + dy;

	}

	return 0;

}

static int graphicsSet3D(lua_State *L) { // love.graphics.set3D()

	is3D = lua_toboolean(L, 1);
	sf2d_set_3D(is3D);

	return 0;

}

static int graphicsGet3D(lua_State *L) { // love.graphics.get3D()

	lua_pushboolean(L, is3D);

	return 1;

}

static int graphicsSetDepth(lua_State *L) { // love.graphics.setDepth()

	currentDepth = luaL_checkinteger(L, 1);

	return 0;

}

static int graphicsGetDepth(lua_State *L) { // love.graphics.getDepth()

	lua_pushnumber(L, currentDepth);

	return 1;

}

static int graphicsSetLineWidth(lua_State *L) { // love.graphics.setLineWidth()

 // TODO: Do this properly

}

static int graphicsGetLineWidth(lua_State *L) { // love.graphics.getLineWidth()

 // TODO: This too.

}

static int graphicsSetDefaultFilter(lua_State *L) { // love.graphics.setDefaultFilter()

	char *minMode = luaL_checkstring(L, 1);
	char *magMode = luaL_optstring(L, 2, minMode);

	u32 minFilter;
	u32 magFilter;

	if (strcmp(minMode, "linear") != 0 && 
		strcmp(minMode, "nearest") != 0 &&
		strcmp(magMode, "linear") != 0 &&
		strcmp(magMode, "nearest" != 0)) {
			luaError(L, "Invalid Image Filter.");
			return 0;
		}

	if (strcmp(minMode, "linear") == 0) minFilter = GPU_TEXTURE_MIN_FILTER(GPU_LINEAR);
	if (strcmp(magMode, "linear") == 0) magFilter = GPU_TEXTURE_MAG_FILTER(GPU_LINEAR);
	if (strcmp(minMode, "nearest") == 0) minFilter = GPU_TEXTURE_MIN_FILTER(GPU_NEAREST);
	if (strcmp(magMode, "nearest") == 0) magFilter = GPU_TEXTURE_MAG_FILTER(GPU_NEAREST);

	defaultMinFilter = minMode;
	defaultMagFilter = magMode;

	defaultFilter = magFilter | minFilter;

	return 0;

}

static int graphicsGetDefaultFilter(lua_State *L) { // love.graphics.getDefaultFilter()

	lua_pushstring(L, defaultMinFilter);
	lua_pushstring(L, defaultMagFilter);

	return 2;

}

int imageNew(lua_State *L);
int fontNew(lua_State *L);
int quadNew(lua_State *L);

const char *fontDefaultInit(love_font *self, int size);

int initLoveGraphics(lua_State *L) {

	luaL_Reg reg[] = {
		/** Drawing **/
		//{ "arc",				graphicsArc					},
		{ "circle",				graphicsCircle				},
		//{ "clear",				graphicsClear				},
		//{ "discard",			graphicsDiscard				},
		{ "draw",				graphicsDraw				},
		//{ "ellipse",			graphicsEllipse				},
		{ "line",				graphicsLine				},
		//{ "points",				graphicsPoints				},
		//{ "polygon",			graphicsPolygon				},
		{ "present",			graphicsPresent				},
		{ "print",				graphicsPrint				},
		{ "printf",				graphicsPrintFormat			},
		{ "rectangle",			graphicsRectangle			},
		//{ "stencil",			graphicsStencil				},
		
		/** Object Creation **/
		//{ "newCanvas",			canvasNew					},
		{ "newFont",			fontNew						},
		{ "newImage",			imageNew					},
		//{ "newImageFont",		imageFontNew				},
		//{ "newMesh",			meshNew						},
		//{ "newParticleSystem",	particleSystemNew			},
		{ "newQuad",			quadNew						},
		//{ "newScreenshot",		screenshotNew				},
		//{ "newShader",			shaderNew					},
		//{ "newSpriteBatch",		spriteBatchNew				},
		//{ "newText",			textNew						},
		//{ "newVideo",			videoNew					},
		//{ "setNewFont",			graphicsSetNewFont			},

		/** Graphics State **/
		//{ "getBackgroundColor",	graphicsGetBackgroundColor	},
		//{ "getBlendMode",		graphicsGetBlendMode		},
		//{ "getCanvas",			graphicsGetCanvas			},
		//{ "getCanvasFormats",	graphicsGetCanvasFormats	},
		{ "getColor",			graphicsGetColor			},
		//{ "getColorMask",		graphicsGetColorMask		},
		{ "setBackgroundColor",	graphicsSetBackgroundColor	},
		{ "setColor",			graphicsSetColor			},
		
		{ "getScreen",			graphicsGetScreen			},
		{ "setScreen",			graphicsSetScreen			},
		{ "getSide",			graphicsGetSide				},
		{ "getWidth",			graphicsGetWidth			},
		{ "getHeight",			graphicsGetHeight			},
		{ "setFont",			graphicsSetFont				},
		{ "push",				graphicsPush				},
		{ "pop",				graphicsPop					},
		{ "origin",				graphicsOrigin				},
		{ "translate",			graphicsTranslate			},
		{ "set3D",				graphicsSet3D				},
		{ "get3D",				graphicsGet3D				},
		{ "setDepth",			graphicsSetDepth			},
		{ "getDepth",			graphicsGetDepth			},
		// { "setLineWidth",		graphicsSetLineWidth		},
		// { "getLineWidth",		graphicsGetLineWidth		},
		{ "setDefaultFilter",	graphicsSetDefaultFilter	},
		{ "getDefaultFilter",	graphicsGetDefaultFilter	},
		{ 0, 0 },
	};

	luaL_newlib(L, reg);

	return 1;

}
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
#include "../util.h"

bool channelList[24];

int getOpenChannel() {

	for (int i = 0; i <= 23; i++) {
		if (!channelList[i]) {
			channelList[i] = true;
			return i;
		}
	}

	return -1;

}

#define CLASS_TYPE  LUAOBJ_TYPE_SOURCE
#define CLASS_NAME  "Source"

const char *sourceInit(love_source *self, const char *filename) {

	if (fileExists(filename)) {

		for (int i=0; i<12; i++) self->mix[i] = 1.0f;
		self->interp = NDSP_INTERP_LINEAR;

		const char *ext = fileExtension(filename);

		if (strncmp(ext, "wav", 3) == 0) self->type = TYPE_WAV;
		else self->type = TYPE_UNKNOWN;

		if (self->type == TYPE_WAV) {

			FILE *file = fopen(filename, "rb");

			if (file) {

				const char *error = NULL;
				u32 ckSize;
				char buff[8];

				// Master chunk
				fread(buff, 4, 1, file); // ckId
				if (strncmp(buff, "RIFF", 4) != 0) error = "RIFF chunk not found";

				fseek(file, 4, SEEK_CUR); // skip ckSize

				fread(buff, 4, 1, file); // WAVEID
				if (strncmp(buff, "WAVE", 4) != 0) error = "RIFF not in WAVE format";
				// fmt Chunk
				fread(buff, 4, 1, file); // ckId
				if (strncmp(buff, "fmt ", 4) != 0) error = "fmt chunk not found";

				fread(buff, 4, 1, file); // ckSize
				if (*buff != 16) error = "WAV not in PCM format (bad chunk size)"; // should be 16 for PCM format

				fread(buff, 2, 1, file); // wFormatTag
				if (*buff != 0x0001) error = "WAV not in PCM format"; // PCM format

				u16 channels;
				fread(&channels, 2, 1, file); // nChannels
				self->channels = channels;
				
				u32 rate;
				fread(&rate, 4, 1, file); // nSamplesPerSec
				self->rate = rate;

				fseek(file, 4, SEEK_CUR); // skip nAvgBytesPerSec

				u16 byte_per_block; // 1 block = 1*channelCount samples
				fread(&byte_per_block, 2, 1, file); // nBlockAlign

				u16 byte_per_sample;
				fread(&byte_per_sample, 2, 1, file); // wBitsPerSample
				byte_per_sample /= 8; // bits -> bytes

				// There may be some additionals chunks between fmt and data
				fread(&buff, 4, 1, file); // ckId
				while (strncmp(buff, "data", 4) != 0) {
					fread(&ckSize, 4, 1, file); // ckSize

					fseek(file, ckSize, SEEK_CUR); // skip chunk

					int i = fread(&buff, 4, 1, file); // next chunk ckId

					if (i < 4) {
						error = "reached EOF before finding a data chunk";
						break;
					}
				}

				// data Chunk (ckId already read)
				fread(&ckSize, 4, 1, file); // ckSize
				self->size = ckSize;

				self->nsamples = self->size / byte_per_block;

				if (byte_per_sample == 1) self->encoding = NDSP_ENCODING_PCM8;
				else if (byte_per_sample == 2) self->encoding = NDSP_ENCODING_PCM16;
				else return "unknown encoding, needs to be PCM8 or PCM16";

				if (error != NULL) {
					fclose(file);
					return error;
				}

				self->audiochannel = getOpenChannel();
				self->loop = false;

				// Read data
				if (linearSpaceFree() < self->size) return "not enough linear memory available";
				self->data = linearAlloc(self->size);

				fread(self->data, self->size, 1, file);

			} else return "Could not open source, read failure";

			fclose(file);

		} else return "Unknown audio type";

	}

	return "Could not open source, does not exist";

}

int sourceNew(lua_State *L) { // love.audio.newSource()

	const char *filename = luaL_checkstring(L, 1);

	love_source *self = luaobj_newudata(L, sizeof(*self));
	luaobj_setclass(L, CLASS_TYPE, CLASS_NAME);

	const char *error = sourceInit(self, filename);

	if (error) luaU_error(L, error);

	return 1;

}

int sourceGC(lua_State *L) { // Garbage Collection

	if (!soundEnabled) luaU_error(L, "Could not initialize audio");

	love_source *self = luaobj_checkudata(L, 1, CLASS_TYPE);

	ndspChnWaveBufClear(self->audiochannel);

	linearFree(self->data);

	channelList[self->audiochannel] = false;

	return 0;

}

int sourcePlay(lua_State *L) { // source:play()

	if (!soundEnabled) luaU_error(L, "Could not initialize audio");

	love_source *self = luaobj_checkudata(L, 1, CLASS_TYPE);

	if (self->audiochannel == -1) {
		luaU_error(L, "No available audio channel");
		return 0;
	}

	ndspChnWaveBufClear(self->audiochannel);
	ndspChnReset(self->audiochannel);
	ndspChnInitParams(self->audiochannel);
	ndspChnSetMix(self->audiochannel, self->mix);
	ndspChnSetInterp(self->audiochannel, self->interp);
	ndspChnSetRate(self->audiochannel, self->rate);
	ndspChnSetFormat(self->audiochannel, NDSP_CHANNELS(self->channels) | NDSP_ENCODING(self->encoding));

	ndspWaveBuf* waveBuf = calloc(1, sizeof(ndspWaveBuf));

	waveBuf->data_vaddr = self->data;
	waveBuf->nsamples = self->nsamples;
	waveBuf->looping = self->loop;

	DSP_FlushDataCache((u32*)self->data, self->size);

	ndspChnWaveBufAdd(self->audiochannel, waveBuf);

	return 0;

}

int sourceStop(lua_State *L) { // source:stop()

	if (!soundEnabled) luaU_error(L, "Could not initialize audio");

	love_source *self = luaobj_checkudata(L, 1, CLASS_TYPE);

	ndspChnWaveBufClear(self->audiochannel);

	return 0;

}

int sourceIsPlaying(lua_State *L) { // source:isPlaying()

	if (!soundEnabled) luaU_error(L, "Could not initialize audio");

	love_source *self = luaobj_checkudata(L, 1, CLASS_TYPE);

	lua_pushboolean(L, ndspChnIsPlaying(self->audiochannel));

	return 1;

}

int sourceSetLooping(lua_State *L) { // source:setLooping()

	if (!soundEnabled) luaU_error(L, "Could not initialize audio");

	love_source *self = luaobj_checkudata(L, 1, CLASS_TYPE);

	self->loop = lua_toboolean(L, 2);

	return 0;

}

int sourceIsLooping(lua_State *L) { // source:isLooping()

	if (!soundEnabled) luaU_error(L, "Could not initialize audio");

	love_source *self = luaobj_checkudata(L, 1, CLASS_TYPE);

	lua_pushboolean(L, self->loop);

	return 1;

}

int sourceSetVolume(lua_State *L) { // source:setVolume()

	if (!soundEnabled) luaU_error(L, "Could not initialize audio");

	love_source *self = luaobj_checkudata(L, 1, CLASS_TYPE);
	float vol = luaL_checknumber(L, 2);
	if (vol > 1) vol = 1;
	if (vol < 0) vol = 0;

	for (int i=0; i<=3; i++) self->mix[i] = vol;

	ndspChnSetMix(self->audiochannel, self->mix);

	return 0;

}

int sourceGetVolume(lua_State *L) { // source:getVolume()

	if (!soundEnabled) luaU_error(L, "Could not initialize audio");

	love_source *self = luaobj_checkudata(L, 1, CLASS_TYPE);

	lua_pushnumber(L, self->mix[0]);

	return 1;

}

int sourceTell(lua_State *L) { // source:tell()

	if (!soundEnabled) luaU_error(L, "Could not initialize audio");

	love_source *self = luaobj_checkudata(L, 1, CLASS_TYPE);

	if (!ndspChnIsPlaying(self->audiochannel)) {
		lua_pushnumber(L, 0);
	} else {
		lua_pushnumber(L, (double)(ndspChnGetSamplePos(self->audiochannel)) / self->rate);
	}

	return 1;

}

int sourceGetDuration(lua_State *L) { // source:getDuration()

	if (!soundEnabled) luaU_error(L, "Could not initialize audio");

	love_source *self = luaobj_checkudata(L, 1, CLASS_TYPE);

	lua_pushnumber(L, (double)(self->nsamples) / self->rate);

	return 1;

}

int initSourceClass(lua_State *L) {

	luaL_Reg reg[] = {
		{"new",			sourceNew	},
		{"__gc",		sourceGC	},
		{"play",		sourcePlay	},
		{"stop",		sourceStop	},
		{"isPlaying",	sourceIsPlaying},
		{"setLooping",	sourceSetLooping},
		{"isLooping",	sourceIsLooping},
		{"setVolume",	sourceSetVolume},
		{"getVolume",	sourceGetVolume},
		{"tell",		sourceTell},
		{"getDuration", sourceGetDuration},
		{ 0, 0 },
	};

	luaobj_newclass(L, CLASS_NAME, NULL, sourceNew, reg);

	return 1;

}

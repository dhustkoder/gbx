#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <SDL2/SDL.h>
#include <Utix/Assert.h>
#include <Utix/ScopeExit.h>
#include "Gameboy.hpp"

namespace {

static bool InitSDL();
static void QuitSDL();
static void UpdateKey(gbx::KeyState state, SDL_Scancode keycode, gbx::Keys* keys);
static void RenderGraphics(const gbx::GPU& gpu, const gbx::Memory& memory);


}




int main(int argc, char** argv)
{
	if (argc < 2) {
		fprintf(stderr, "usage: %s <rom>\n", argv[0]);
		return EXIT_FAILURE;
	}

	gbx::Gameboy* const gameboy = gbx::create_gameboy();
	
	if (!gameboy)
		return EXIT_FAILURE;

	const auto gameboy_guard = utix::MakeScopeExit([=] {
		gbx::destroy_gameboy(gameboy);
	});

	if (!gameboy->LoadRom(argv[1]))
		return EXIT_FAILURE;


	if (!InitSDL())
		return EXIT_FAILURE;


	const auto sdl_guard = utix::MakeScopeExit([] {
		QuitSDL();
	});

	SDL_Event event;
	clock_t clk = clock();
	size_t itr = 0;
	while (true) {

		if (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT)
				break;

			while (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
				const auto state = event.type == SDL_KEYUP ? gbx::KEYUP : gbx::KEYDOWN;
				UpdateKey(state, event.key.keysym.scancode, &gameboy->keys);
				if (!SDL_PollEvent(&event))
					break;
			}
		}


		gameboy->Run(71072);
		gameboy->cpu.SetClock(0);
		if (gameboy->hwstate.GetFlags(gbx::HWState::NEED_RENDER)) {
			RenderGraphics(gameboy->gpu, gameboy->memory);
			gameboy->hwstate.ClearFlags(gbx::HWState::NEED_RENDER);
			SDL_Delay(1000 / 60);
			++itr;
		}

		if ((clock() - clk) >= CLOCKS_PER_SEC) {
			printf("ITR: %zu\n", itr);
			itr = 0;
			clk = clock();
		}

	}

	return EXIT_SUCCESS;
}









namespace {

// TODO: endiannes checks, this is little endian only

enum Color : Uint32
{
	BLACK = 0x00000000,
	WHITE = 0xFFFFFF00,
	LIGHT_GREY = 0xA0A0A000,
	DARK_GREY = 0x55555500,
};


struct TileMap
{
	uint8_t data[32][32];
};

struct SpriteAttr
{
	uint8_t ypos;
	uint8_t xpos;
	uint8_t id;
	uint8_t flags;
};


struct Tile
{
	uint8_t data[8][2];
};

struct Sprite
{
	uint8_t data[8][2];
};



constexpr const int WIN_WIDTH = 160;
constexpr const int WIN_HEIGHT = 144;

static SDL_Window* window;
static SDL_Texture* texture;
static SDL_Renderer* renderer;
static Uint32* gfx_buffer;

static void DrawBG(const gbx::GPU& gpu, const gbx::Memory& memory);
static void DrawWIN(const gbx::GPU& gpu, const gbx::Memory& memory);
static void DrawOBJ(const gbx::GPU& gpu, const gbx::Memory& memory);
static void DrawTileMap(const Tile* tiles, const TileMap* map, uint8_t pallete, bool unsigned_map);
static void DrawTile(const Tile& tile, uint8_t pallete, uint8_t x, uint8_t y);
static void DrawSprite(const Sprite& sprite, const SpriteAttr& attr, const gbx::GPU& gpu);
static Color SolvePallete(const Tile& tile, uint8_t row, uint8_t bit, uint8_t pallete);
inline Color SolvePallete(const Sprite& sprite, uint8_t row, uint8_t bit, uint8_t pallete);
inline Color CheckPixel(uint8_t x, uint8_t y);
inline void DrawPixel(Color pixel, uint8_t x, uint8_t y);





static void RenderGraphics(const gbx::GPU& gpu, const gbx::Memory& memory)
{
	SDL_RenderClear(renderer);
	
	if (!gpu.BitLCDC(gbx::GPU::LCD_ON_OFF)) {
		SDL_RenderPresent(renderer);
		return;
	}
	
	int pitch;
	
	if (SDL_LockTexture(texture, nullptr, (void**)&gfx_buffer, &pitch) != 0) {
		fprintf(stderr, "failed to lock texture");
		return;
	}

	if (gpu.BitLCDC(gbx::GPU::BG_ON_OFF))
		DrawBG(gpu, memory);

	if (gpu.BitLCDC(gbx::GPU::WIN_ON_OFF))
		DrawWIN(gpu, memory);

	if (gpu.BitLCDC(gbx::GPU::OBJ_ON_OFF))
		DrawOBJ(gpu, memory);


	SDL_UnlockTexture(texture);
	SDL_RenderCopy(renderer, texture, nullptr, nullptr);
	SDL_RenderPresent(renderer);
}



static void DrawBG(const gbx::GPU& gpu, const gbx::Memory& memory)
{
	const bool tile_data_select = gpu.BitLCDC(gbx::GPU::BG_WIN_TILE_DATA_SELECT);
	const bool tile_map_select = gpu.BitLCDC(gbx::GPU::BG_TILE_MAP_SELECT);

	auto tiles = tile_data_select ? reinterpret_cast<const Tile*>(memory.vram)
	                              : reinterpret_cast<const Tile*>(memory.vram + 0x1000);

	auto map = tile_map_select ? reinterpret_cast<const TileMap*>(memory.vram + 0x1C00)
                               : reinterpret_cast<const TileMap*>(memory.vram + 0x1800);

	DrawTileMap(tiles, map, gpu.bgp, tile_data_select);
}



static void DrawWIN(const gbx::GPU& gpu, const gbx::Memory& memory)
{
	const bool tile_data_select = gpu.BitLCDC(gbx::GPU::BG_WIN_TILE_DATA_SELECT);
	const bool tile_map_select = gpu.BitLCDC(gbx::GPU::WIN_TILE_MAP_SELECT);

	auto tiles = tile_data_select ? reinterpret_cast<const Tile*>(memory.vram)
	                              : reinterpret_cast<const Tile*>(memory.vram + 0x1000);

	auto map = tile_map_select ? reinterpret_cast<const TileMap*>(memory.vram + 0x1C00)
	                           : reinterpret_cast<const TileMap*>(memory.vram + 0x1800);

	DrawTileMap(tiles, map, gpu.bgp, tile_data_select);
}



static void DrawOBJ(const gbx::GPU& gpu, const gbx::Memory& memory)
{
	auto sprite_attr = reinterpret_cast<const SpriteAttr*>(memory.oam);
	auto sprites = reinterpret_cast<const Sprite*>(memory.vram);

	for (uint8_t i = 0; i < 40; ++i) {
		const auto id = sprite_attr[i].id;
		DrawSprite(sprites[id], sprite_attr[i], gpu);
	}
}




static void DrawTileMap(const Tile* tiles, const TileMap* map, uint8_t pallete, bool unsigned_map)
{
	if (unsigned_map) {
		for (uint8_t y = 0; y < 18; ++y)
			for (uint8_t x = 0; x < 20; ++x)
				DrawTile(tiles[map->data[y][x]], pallete, x * 8, y * 8);
	}
	else {
		for (uint8_t y = 0; y < 18; ++y)
			for (uint8_t x = 0; x < 20; ++x)
				DrawTile(tiles[(int8_t)map->data[y][x]], pallete, x * 8, y * 8);
	}
}







static void DrawTile(const Tile& tile, uint8_t pallete, uint8_t x, uint8_t y)
{
	for (uint8_t tile_y = 0; tile_y < 8; ++tile_y) {
		for (uint8_t bit = 0; bit < 8; ++bit) {
			const auto pixel = SolvePallete(tile, tile_y, bit, pallete);
			DrawPixel(pixel, x + bit, y + tile_y);
		}
	}
}



static void DrawSprite(const Sprite& sprite, const SpriteAttr& attr, const gbx::GPU& gpu)
{
	const uint8_t pallete = gbx::TestBit(4, attr.flags) ? gpu.obp1 : gpu.obp0;
	
	const bool priority = gbx::TestBit(7, attr.flags) != 0;
	ASSERT_MSG(!gbx::TestBit(6, attr.flags), "NEED YFLIP");
	ASSERT_MSG(!gbx::TestBit(5, attr.flags), "NEED XFLIP");

	const uint8_t xpos = attr.xpos - 8;
	const uint8_t ypos = attr.ypos - 16;
	if (xpos >= 160 && ypos >= 144)
		return;


	for (uint8_t row = 0; row < 8; ++row) {
		const uint8_t abs_ypos = ypos + row;
		if (abs_ypos >= WIN_WIDTH)
			break;

		for (uint8_t bit = 0; bit < 8; ++bit) {
			const uint8_t abs_xpos = xpos + bit;
			if (abs_xpos >= 160)
				break;
			else if (priority && CheckPixel(abs_xpos, abs_ypos) != WHITE)
					continue;

			const auto pixel = SolvePallete(sprite, row, bit, pallete);
			DrawPixel(pixel, abs_xpos, abs_ypos);
		}
	}
}




static Color SolvePallete(const Tile& tile, uint8_t row, uint8_t bit, uint8_t pallete)
{
	const auto get_color = [=](uint8_t pallete_value) {
		switch (pallete_value) {
		case 0x00: return WHITE;
		case 0x01: return LIGHT_GREY;
		case 0x02: return DARK_GREY;
		default: return BLACK;
		};
	};

	const uint8_t downbit = (tile.data[row][0] & (0x80 >> bit)) ? 1 : 0;
	const uint8_t upperbit = (tile.data[row][1] & (0x80 >> bit)) ? 1 : 0;

	const uint8_t value = (upperbit << 1) | downbit;

	switch (value) {
	case 0x00: return get_color(pallete & 0x03);
	case 0x01: return get_color((pallete & 0x0C) >> 2);
	case 0x02: return get_color((pallete & 0x30) >> 4);
	default: return get_color((pallete & 0xC0) >> 6);
	}
}


inline Color SolvePallete(const Sprite& sprite, uint8_t row, uint8_t bit, uint8_t pallete)
{
	return SolvePallete(reinterpret_cast<const Tile&>(sprite), row, bit, pallete);
}


inline Color CheckPixel(uint8_t x, uint8_t y)
{
	return static_cast<Color>(gfx_buffer[(y * WIN_WIDTH) + x]);
}


inline void DrawPixel(Color pixel, uint8_t x, uint8_t y)
{
	gfx_buffer[(y * WIN_WIDTH) + x] = pixel;
}


static void UpdateKey(gbx::KeyState state, SDL_Scancode keycode, gbx::Keys* keys)
{
	switch (keycode) {
	case SDL_SCANCODE_Z: keys->pad.bit.a = state; break;
	case SDL_SCANCODE_X: keys->pad.bit.b = state; break;
	case SDL_SCANCODE_C: keys->pad.bit.select = state; break;
	case SDL_SCANCODE_V: keys->pad.bit.start = state; break;
	case SDL_SCANCODE_RIGHT: keys->pad.bit.right = state; break;
	case SDL_SCANCODE_LEFT: keys->pad.bit.left = state; break;
	case SDL_SCANCODE_UP: keys->pad.bit.up = state; break;
	case SDL_SCANCODE_DOWN: keys->pad.bit.down = state; break;
	default: break;
	}
}















static bool InitSDL()
{
	if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
		fprintf(stderr, "failed to init SDL2: %s\n", SDL_GetError());
		return false;
	}

	window = SDL_CreateWindow("GBX",
		SDL_WINDOWPOS_CENTERED,
		SDL_WINDOWPOS_CENTERED,
		WIN_WIDTH * 2, WIN_HEIGHT * 2, 0);

	if (!window) {
		fprintf(stderr, "failed to create SDL_Window: %s\n", SDL_GetError());
		goto free_sdl;
	}

	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

	if (!renderer) {
		fprintf(stderr, "failed to create SDL_Renderer: %s\n", SDL_GetError());
		goto free_window;
	}


	texture = SDL_CreateTexture(renderer,
		SDL_PIXELFORMAT_RGBA8888,
		SDL_TEXTUREACCESS_STREAMING,
		WIN_WIDTH, WIN_HEIGHT);

	if (!texture) {
		fprintf(stderr, "failed to create SDL_Texture: %s\n", SDL_GetError());
		goto free_renderer;
	}


	return true;

free_renderer:
	SDL_DestroyRenderer(renderer);
free_window:
	SDL_DestroyWindow(window);
free_sdl:
	SDL_Quit();

	return false;
}






static void QuitSDL()
{
	SDL_DestroyTexture(texture);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();
}













}

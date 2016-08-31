#include <string.h>
#include "Gameboy.hpp"


namespace gbx {


enum Color : uint32_t
{
	BLACK = 0x00000000,
	WHITE = 0xFFFFFF00,
	LIGHT_GREY = 0x90909000,
	DARK_GREY = 0x55555500,
};


static const uint32_t colors[4] = {
	WHITE, LIGHT_GREY, 
	DARK_GREY, BLACK
};



static void draw_scanline(GPU* const gpu,
	const uint8_t* const vram,
	uint32_t* const gfx);

static void draw_bg(const uint8_t* data,
	const uint8_t* map,
	const bool unsig_tiles,
	const uint8_t bgp,
	const uint8_t scx,
	const uint8_t scy,
	const uint8_t lydiv,
	const uint8_t lymod,
	uint32_t* const gfx_line);

inline void draw_row(const uint16_t row,
	const uint8_t pixbeg,
	const uint8_t pixend,
	const uint8_t xpos,
	const uint8_t* pallete,
	uint32_t* gfx_line);

inline uint16_t get_row(const uint8_t* const data,
	const uint8_t* const map,
	const bool unsig_tiles,
	const uint8_t map_id);





void Gameboy::UpdateGPU(const uint8_t cycles)
{
	if (!gpu.lcdc.lcd_on) {
		gpu.clock = 0;
		gpu.ly = 0;
		gpu.stat.mode = GPU::Mode::HBLANK;
		return;
	}

	const auto compare_ly = [this] {
		if (gpu.ly != gpu.lyc) {
			if (gpu.stat.coincidence_flag)
				gpu.stat.coincidence_flag = 0;
		} else {
			gpu.stat.coincidence_flag = 1;
			if (gpu.stat.int_on_coincidence)
				hwstate.RequestInt(INT_LCD_STAT);
		}
	};

	const auto set_mode = [this](const GPU::Mode mode) {
		const auto stat = gpu.stat;
		uint8_t int_on = 0;
		switch (mode) {
		case GPU::Mode::HBLANK: int_on = stat.int_on_hblank; break;
		case GPU::Mode::VBLANK: int_on = stat.int_on_vblank; break;
		case GPU::Mode::OAM: int_on = stat.int_on_oam; break;
		default: break;
		}
		if (int_on)
			hwstate.RequestInt(INT_LCD_STAT);
		gpu.stat.mode = mode;
	};

	gpu.clock += cycles;

	

	switch (gpu.stat.mode) {
	case GPU::Mode::HBLANK:
		if (gpu.clock >= 204) {
			draw_scanline(&gpu, memory.vram, memory.gfx);

			if (gpu.ly != 144) {
				set_mode(GPU::Mode::OAM);
			} else {
				hwstate.RequestInt(INT_VBLANK);
				set_mode(GPU::Mode::VBLANK);
			}

			compare_ly();
			gpu.clock -= 204;
		}

		break;

	case GPU::Mode::VBLANK:
		if (gpu.clock >= 456) {
			++gpu.ly;

			if (gpu.ly > 153) {
				gpu.ly = 0;
				set_mode(GPU::Mode::OAM);
			}

			compare_ly();
			gpu.clock -= 456;
		}

		break;

	case GPU::Mode::OAM:
		if (gpu.clock >= 80) {
			gpu.stat.mode = GPU::Mode::TRANSFER;
			gpu.clock -= 80;
		}

		break;

	case GPU::Mode::TRANSFER:
		if (gpu.clock >= 172) {
			set_mode(GPU::Mode::HBLANK);
			gpu.clock -= 172;
		}

		break;
	}
}






static void draw_scanline(GPU* const gpu, 
	const uint8_t* const vram,
	uint32_t* const gfx)
{
	const auto ly = gpu->ly++;
	const auto bgp = gpu->bgp;
	const auto lcdc = gpu->lcdc;
	const bool unsig_tiles = lcdc.tile_data != 0;
	const auto tile_data = unsig_tiles ? vram : &vram[0x1000];

	const uint8_t lydiv = ly / 8;
	const uint8_t lymod = ly % 8;

	auto gfx_line = &gfx[ly * 160];

	if (lcdc.bg_on) {
		const auto scx = gpu->scx;
		const auto scy = gpu->scy;
		const auto map = lcdc.bg_map ? &vram[0x1C00] : &vram[0x1800];
		draw_bg(tile_data, map, unsig_tiles, bgp, scx, scy, lydiv, lymod, gfx_line);
	}

}





static void draw_bg(const uint8_t* data,
	const uint8_t* map,
	const bool unsig_tiles,
	const uint8_t bgp,
	const uint8_t scx,
	const uint8_t scy,
	const uint8_t lydiv,
	const uint8_t lymod,
	uint32_t* const gfx_line)
{
	const uint8_t scydiv = scy / 8;
	const uint8_t scymod = scy % 8;
	const uint8_t scxdiv = scx / 8;
	const uint8_t scxmod = scx % 8;

	map += (((scydiv + lydiv)&31) * 32);
	data += ((scymod + lymod)&7) * 2;

	const auto get_row = [=] (const uint8_t map_id) {
		return gbx::get_row(data, map, unsig_tiles, map_id);
	};

	const uint8_t pallete[4] = {
		static_cast<uint8_t>(bgp&0x03),
		static_cast<uint8_t>((bgp&0x0C)>>2),
		static_cast<uint8_t>((bgp&0x30)>>4), 
		static_cast<uint8_t>((bgp&0xC0)>>6)
	};

	uint8_t x = 0;

	if (scxmod) {
		++x;
		const auto first_row = get_row(scxdiv);
		const auto last_row = get_row(20 + scxdiv);
		draw_row(first_row, scxmod, 8, 0 - scxmod, pallete, gfx_line);
		draw_row(last_row, 0, scxmod, 160 - scxmod, pallete, gfx_line);
	}

	for (; x < 20; ++x) {
		const auto row = get_row(x + scxdiv);
		const uint8_t xpos = (x * 8) - scxmod;
		draw_row(row, 0, 8, xpos, pallete, gfx_line);
	}
}



inline void draw_row(const uint16_t row,
	const uint8_t pixbeg,
	const uint8_t pixend,
	const uint8_t xpos,
	const uint8_t* pallete,
	uint32_t* gfx_line)
{
	for (uint8_t pix = pixbeg; pix < pixend; ++pix) {
		uint8_t col_num = 0;
		if (row & (0x80 >> pix))
			++col_num;
		if (row & (0x8000 >> pix))
			col_num += 2;

		const uint8_t offset = xpos + pix;
		gfx_line[offset] = colors[pallete[col_num]];
	}
}



inline uint16_t get_row(const uint8_t* const data,
	const uint8_t* const map,
	const bool unsig_tiles,
	const uint8_t map_id)
{
	const uint8_t tile_id = map[map_id&31];
	if (unsig_tiles) {
		const uint16_t addr = tile_id * 16;
		return (data[addr + 1] << 8) | data[addr];
	} else {
		const int16_t addr = static_cast<int8_t>(tile_id) * 16;
		return (*(data + addr + 1) << 8) | *(data + addr);
	}
}





}

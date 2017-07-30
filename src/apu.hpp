#ifndef GBX_APU_HPP_
#define GBX_APU_HPP_
#include <string.h>
#include "common.hpp"
#include "cpu.hpp"

namespace gbx {

constexpr const int_fast32_t kApuFrameCntTicks = kCpuFreq / 512;

struct Apu {

	struct Square {
		int16_t freq;
		int16_t freq_cnt;
		int16_t len_cnt;
		int8_t duty_pos;
		uint8_t out;
		bool trigger;
		bool len_enabled;
		bool enabled;

		union {
			uint8_t val;
			struct {
				uint8_t len    : 6;
				uint8_t duty   : 2;
			};
		} reg1;

		union {
			uint8_t val;
			struct {
				uint8_t period  : 3;
				uint8_t env_add : 1;
				uint8_t vol     : 4;
			};
		} reg2;

	};

	struct Square1 : Square {
		int16_t sweep_cnt;
		int16_t freq_shadow;
		bool sweep_enabled;
		union {
			uint8_t val;
			struct {
				uint8_t shift        : 3;
				uint8_t negate       : 1;
				uint8_t sweep_period : 3;
				uint8_t              : 1;
			};
		} reg0;
	} square1;

	Square square2;

	union {
		uint8_t val;
		struct {
			uint8_t s1t1 : 1;
			uint8_t s2t1 : 1;
			uint8_t s3t1 : 1;
			uint8_t s4t1 : 1;
			uint8_t s1t2 : 1;
			uint8_t s2t2 : 1;
			uint8_t s3t2 : 1;
			uint8_t s4t2 : 1;
		};
	} nr51;


	int16_t frame_cnt;
	int8_t frame_step;
	bool power;
};



extern void update_apu(int16_t cycles, Apu* apu);


inline uint16_t apu_eval_sweep_freq(Apu* const apu)
{
	Apu::Square1& s = apu->square1;
	uint16_t freq = s.freq_shadow >> s.reg0.shift;
	if (s.reg0.negate)
		freq = s.freq_shadow - freq;
	else
		freq = s.freq_shadow + freq;

	if (freq > 0x7FF)
		s.enabled = false;

	return freq;
}

inline uint8_t read_apu_register(const Apu& apu, const uint16_t addr)
{
	const auto rsquare_reg4 = [](const Apu::Square& s) -> uint8_t {
		return (s.trigger<<7)|(s.len_enabled<<6)|(s.freq&0x07);
	};

	switch (addr) {
	case 0xFF10: return apu.square1.reg0.val;
	case 0xFF11: return apu.square1.reg1.val;
	case 0xFF12: return apu.square1.reg2.val;
	case 0xFF14: return rsquare_reg4(apu.square1);
	case 0xFF16: return apu.square2.reg1.val;
	case 0xFF17: return apu.square2.reg2.val;
	case 0xFF19: return rsquare_reg4(apu.square2);
	case 0xFF25: return apu.nr51.val;
	case 0xFF26:
		return (apu.power<<7)                 |
		       ((apu.square2.len_cnt > 0)<<1) |
		       (apu.square1.len_cnt > 0);
	}

	return 0;
}

inline void write_apu_register(const uint16_t addr, const uint8_t val, Apu* const apu)
{
	if (!apu->power)
		return;

	const auto wsquare_reg0 = [&](Apu::Square1* const s) {
		s->reg0.val = val;
	};

	const auto wsquare_reg1 = [&](Apu::Square* const s) {
		s->reg1.val = val;
		s->len_cnt = 64 - s->reg1.len;
	};

	const auto wsquare_reg2 = [&](Apu::Square* const s) {
		s->reg2.val = val;
	};

	const auto wsquare_reg3 = [&](Apu::Square* const s) {
		s->freq = (s->freq&0x0700)|val;
	};

	const auto wsquare_reg4 = [&](const int ch) {
		Apu::Square& s = ch == 1 ? apu->square1 : apu->square2;
		s.freq = (s.freq&0x00FF)|((val&0x07)<<8);
		s.trigger = (val&0x80) != 0;
		s.len_enabled = (val&0x40) != 0;
		if (s.trigger) {
			s.enabled = true;
			s.freq_cnt = (2048 - s.freq) * 4;
			s.len_cnt = 64 - s.reg1.len;
			if (ch == 1) {
				Apu::Square1& s1 = apu->square1;
				s1.freq_shadow = s1.freq;
				s1.sweep_cnt = s1.reg0.sweep_period;
				if (s1.sweep_cnt == 0)
					s1.sweep_cnt = 8;
				s1.freq_shadow = s1.freq;
				s1.sweep_enabled = s1.sweep_cnt > 0 || s1.reg0.shift > 0;
				if (s1.reg0.shift > 0)
					apu_eval_sweep_freq(apu);
			}
		}
	};

	switch (addr) {
	case 0xFF10: wsquare_reg0(&apu->square1); break;
	case 0xFF11: wsquare_reg1(&apu->square1); break;
	case 0xFF12: wsquare_reg2(&apu->square1); break;
	case 0xFF13: wsquare_reg3(&apu->square1); break;
	case 0xFF14: wsquare_reg4(1); break;
	case 0xFF16: wsquare_reg1(&apu->square2); break;
	case 0xFF17: wsquare_reg2(&apu->square2); break;
	case 0xFF18: wsquare_reg3(&apu->square2); break;
	case 0xFF19: wsquare_reg4(2); break;
	case 0xFF25: 
		apu->nr51.val = val;
		break;
	case 0xFF26:
		if ((val&0x80) == 0) {
			memset(apu, 0, sizeof(*apu));
			apu->power = false;
			apu->frame_cnt = kApuFrameCntTicks;
		} else {
			apu->power = true;
		}
		break;
	}
}



} // namespace gbx
#endif

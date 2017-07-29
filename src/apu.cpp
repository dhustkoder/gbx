#include <climits>
#include "audio.hpp"
#include "apu.hpp"


namespace gbx {

constexpr const int kApuSamplesSize = 95;
constexpr const int kSoundBufferSize = 1024;

static int8_t apu_samples[kApuSamplesSize];
static int16_t sound_buffer[kSoundBufferSize];
static int samples_index = 0;
static int sound_buffer_index = 0;


static void tick_length(Apu* const apu)
{
	const auto tick_square_len = [](Apu::Square* const s) {
		if (s->len_cnt > 0)
			--s->len_cnt;
	};

	tick_square_len(&apu->square1);
	tick_square_len(&apu->square2);
}

static void tick_frame_counter(Apu* const apu)
{
	if (--apu->frame_cnt <= 0)  {
		apu->frame_cnt = kApuFrameCntTicks;
		switch (apu->frame_step++) {
		case 0:
			tick_length(apu);
			break;
		case 2:
			tick_length(apu);
			//tick_sweep(apu);
			break;
		case 4:
			tick_length(apu);
			break;
		case 6:
			tick_length(apu);
			//tick_sweep(apu);
			break;
		case 7:
			//tick_envelop(apu);
			apu->frame_step = 0;
			break;
		}
	}
}

static void tick_square_freq_cnt(Apu::Square* const s)
{
	static const uint8_t dutytbl[4][8] = {
		{ 0, 0, 0, 0, 0, 0, 0, 1 },
		{ 1, 0, 0, 0, 0, 0, 0, 1 },
		{ 1, 0, 0, 0, 0, 1, 1, 1 },
		{ 0, 1, 1, 1, 1, 1, 1, 0 }
	};

	if (--s->freq_cnt <= 0) {
		s->freq_cnt = (2048 - s->freq) * 4;
		s->duty_pos += 1;
		s->duty_pos &= 0x07;
	}
	
	if (s->len_cnt <= 0 || !dutytbl[s->reg1.duty][s->duty_pos])
		s->out = 0;
	else
		s->out = s->reg2.vol;
}


void update_apu(const int16_t cycles, Apu* const apu)
{
	if (!apu->power)
		return;

	for (int ticks = 0; ticks < cycles; ++ticks) {
		tick_frame_counter(apu);
		tick_square_freq_cnt(&apu->square1);
		tick_square_freq_cnt(&apu->square2);

		apu_samples[samples_index] = apu->square1.out + apu->square2.out;
		if (++samples_index >= kApuSamplesSize) {
			samples_index = 0;
			double avg = 0;
			for (int i = 0; i < kApuSamplesSize; ++i)
				avg += apu_samples[i];
			avg /= 95;
			avg *= 500;
			sound_buffer[sound_buffer_index] = avg;
			if (++sound_buffer_index >= kSoundBufferSize) {
				sound_buffer_index = 0;
				queue_sound_buffer((uint8_t*)sound_buffer, sizeof(sound_buffer));
			}
		}
	}
}


} // namespace gbx


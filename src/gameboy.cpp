#include <string.h>
#include "instructions.hpp"
#include "gameboy.hpp"

namespace gbx {

static void update_timers(int16_t cycles, HWState* hwstate);
static void update_interrupts(Gameboy* gb);


void run_for(const int32_t clock_limit, Gameboy* const gb)
{
	do {
		const int32_t prevclk = gb->cpu.clock;

		if (!gb->hwstate.flags.cpu_halt) {
			const uint8_t opcode = mem_read8(*gb, gb->cpu.pc++);
			main_instructions[opcode](gb);
			gb->cpu.clock += clock_table[opcode];
		} else {
			gb->cpu.clock += 4;
		}

		const auto step_cycles = static_cast<int16_t>(gb->cpu.clock - prevclk);

		update_ppu(step_cycles, gb->memory, &gb->hwstate, &gb->ppu);
		update_apu(step_cycles, &gb->apu);
		update_timers(step_cycles, &gb->hwstate);
		update_interrupts(gb);
		
	} while (gb->cpu.clock < clock_limit);

	gb->cpu.clock -= clock_limit;
}


void update_timers(const int16_t cycles, HWState* const hwstate)
{
	hwstate->div_clock += cycles;
	if (hwstate->div_clock >= 256) {
		++hwstate->div;
		hwstate->div_clock -= 256;
	}

	const uint8_t tac = hwstate->tac;
	if (test_bit(2, tac)) {
		hwstate->tima_clock += cycles;
		while (hwstate->tima_clock >= hwstate->tima_clock_limit) {
			inc_tima(hwstate);
			hwstate->tima_clock -= hwstate->tima_clock_limit;
			hwstate->tima_clock_limit = kTimaClockLimits[tac&3];
		}
	}
}


void update_interrupts(Gameboy* const gb)
{
	const uint8_t pendents = get_pendent_interrupts(gb->hwstate);
	const auto flags = gb->hwstate.flags;

	if (pendents && flags.cpu_halt) {
		gb->hwstate.flags.cpu_halt = false;
		gb->cpu.clock += 4;
	}

	if (flags.ime == 0) {
		return;
	} else if (flags.ime == 1) {
		gb->hwstate.flags.ime = 2;
		return;
	} else if (pendents == 0) {
		return;
	}

	gb->hwstate.flags.ime = 0;
	for (const Interrupt interrupt : kInterrupts) {
		if (pendents & interrupt.mask) {
			clear_interrupt(interrupt, &gb->hwstate);
			stack_push16(gb->cpu.pc, gb);
			gb->cpu.pc = interrupt.addr;
			gb->cpu.clock += 20;
			break;
		}
	}
}





} // namespace gbx


#include <stdio.h>
#include <Utix/Assert.h>
#include "Gameboy.hpp"
#include "Memory.hpp"

namespace gbx {
static uint8_t dummy = 0; // used for undefined reference for invalid addresses
static const uint8_t& solve_hardware_io_ref(const uint16_t address, const Gameboy& gb);
static const uint8_t& solve_address_ref(const uint16_t address, const Gameboy& gb);





int8_t Gameboy::ReadS8(const uint16_t address) const {
	// TODO: this might not be totally portable 
	return static_cast<int8_t>(ReadU8(address));
}




uint8_t Gameboy::ReadU8(const uint16_t address) const {
	return solve_address_ref(address, *this);
}



uint16_t Gameboy::ReadU16(const uint16_t address) const {
	return ConcatBytes(ReadU8(address + 1), ReadU8(address));
}




void Gameboy::WriteU8(const uint16_t address, const uint8_t value) 
{
	if(address > 0x7fff) {
		const_cast<uint8_t&>(solve_address_ref(address, *this)) = value;
	}
	else {
		fprintf(stderr, "write required at ROM %4x\n", address);
	}
}



void Gameboy::WriteU16(const uint16_t address, const uint16_t value) {
	WriteU8(address, GetLowByte(value));
	WriteU8(address + 1, GetHighByte(value));
}





void Gameboy::PushStack8(const uint8_t value) 
{
	const uint16_t sp = cpu.GetSP() - 1;
	WriteU8(sp, value);
	cpu.SetSP(sp);
}


void Gameboy::PushStack16(const uint16_t value) 
{
	const uint16_t sp = cpu.GetSP() - 2;
	WriteU16(sp, value);
	cpu.SetSP(sp);
}



uint8_t Gameboy::PopStack8() 
{
	const uint16_t sp = cpu.GetSP();
	const uint8_t val = ReadU8(sp);
	cpu.SetSP(sp + 1);
	return val;
}




uint16_t Gameboy::PopStack16() 
{
	const uint16_t sp = cpu.GetSP();
	const uint16_t val = ReadU16(sp);
	cpu.SetSP(sp + 2);
	return val;
}








static const uint8_t& solve_address_ref(const uint16_t address, const Gameboy& gb)
{
	if (address >= 0xFF80) {
		if (address < 0xFFFF)
			return gb.memory.zero_page[address - 0xFF80];
		else
			return gb.interrupts.enable;
	}
	else if (address >= 0xFF00) {
		return solve_hardware_io_ref(address, gb);
	}
	else if (address >= 0xFE00) {
		if (address < 0xFEA0)
			return gb.memory.oam[address - 0xFE00];
	}
	else if (address >= 0xC000) {
		if (address < 0xD000)
			return gb.memory.ram[address - 0xC000];
	}
	else if (address >= 0xA000) {
		ASSERT_MSG(false, "cartridge ram required");
	}
	else if (address >= 0x8000) {
		return gb.memory.vram[address - 0x8000];
	}
	else if (address < 0x8000) {
		if(address >= 0x4000)
			return gb.memory.home[address - 0x4000];
		else
			return gb.memory.fixed_home[address];
	}
	
	fprintf(stderr, "address %4x required\n", address);
	return dummy;
}



static const uint8_t& solve_hardware_io_ref(const uint16_t address, const Gameboy& gb)
{
	switch (address) {
	case 0xFF0F: return gb.interrupts.flags;
	default:
		fprintf(stderr, "hardware io %4x required\n", address);
		break;
	};

	return dummy;
}









}

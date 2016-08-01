#include <stdio.h>
#include <string.h>
#include <Utix/ScopeExit.h>
#include "Memory.hpp"
#include "Gameboy.hpp"
#include "Cartridge.hpp"

namespace gbx {



bool Gameboy::LoadRom(const char* file_name) 
{ 
	{
		FILE* const file = fopen(file_name, "r");
		if (!file) {
			perror("Couldn't open file");
			return false;
		}

		const auto file_guard = utix::MakeScopeExit([=] { fclose(file); });
	
		fseek(file, 0, SEEK_END);

		const auto file_size = static_cast<size_t>(ftell(file));
		if (file_size > CARTRIDGE_MAX_SIZE
			|| file_size < CARTRIDGE_MIN_SIZE) {
			fprintf(stderr, "size of \'%s\': %zu bytes is incompatible!", file_name, file_size);
			return false;
		}

		fseek(file, 0, SEEK_SET);

		fread(memory + RAM_MAX_SIZE, sizeof(uint8_t), file_size, file);
		if (ferror(file)) {
			perror("error while reading from file");
			return false;
		}
	} // close file in the end of this scope

	return this->Reset();
}

















CartridgeInfo get_cartridge_info(const uint8_t* memory)
{
	CartridgeInfo cinfo;
	// 0134 - 0142 game's title
	memcpy(cinfo.internal_name, &memory[0x134], 16);
	cinfo.internal_name[16] = 0;

	const uint8_t super_gb_check = memory[0x146];
	if (super_gb_check == 0x03) {
		cinfo.system = System::SUPER_GAMEBOY;
	}
	else {
		const uint8_t color_check = memory[0x143];
		cinfo.system = color_check == 0x80 ? System::GAMEBOY_COLOR : System::GAMEBOY;
	}

	cinfo.type = static_cast<CartridgeType>(memory[0x147]);

	const uint8_t size_code = memory[0x148];
	switch (size_code) {
	case 0x00: cinfo.size = 32_Kib; break;    // 2 banks
	//case 0x01: cinfo.size = 64_Kib; break;  // 4 banks
	//case 0x02: cinfo.size = 128_Kib; break; // 8 banks
	//case 0x03: cinfo.size = 256_Kib; break; // 16 banks
	//case 0x04: cinfo.size = 512_Kib; break; // 32 banks
	//case 0x05: cinfo.size = 1_Mib; break;   // 64 banks
	//case 0x06: cinfo.size = 2_Mib; break;   // 128 banks
	default: cinfo.size = 0; break;
	}

	return cinfo;
}












}

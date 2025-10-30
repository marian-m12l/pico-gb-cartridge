#pragma once

#include <stdint.h>

void persist_ram_to_flash();
uint8_t* selected_rom();
uint8_t init_rom(const uint8_t* romdata, uint32_t size);
void loop_launcher();
void loop_32kb();
void loop_mbc1();
void find_rom_entries();

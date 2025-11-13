#pragma once

#include <stdint.h>

typedef struct {
    uint8_t type;
    uint8_t rom;
    uint8_t ram;
    uint32_t romsize;
    uint32_t ramsize;
    bool has_ram;
    bool has_battery;
    bool has_rumble;
    void (*loop)();
} cart_t;

void persist_ram_to_flash();
uint8_t* selected_rom();
void set_selected_rom(uint8_t* selected);
cart_t init_rom(const uint8_t* romdata, uint32_t size);
void loop_launcher();
void loop_32kb();
void loop_mbc1();
void loop_mbc5();
void find_rom_entries();

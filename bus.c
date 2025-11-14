#include <string.h>
#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "hardware/xip_cache.h"

#include "debug.h"
#include "bus.h"
#include "pins.h"
#include "launcher.h"

#include "shared/romlist.h"


#define CACHE_AS_SRAM_OFFSET 0x02000000

#ifdef NO_LOAD
#define SET_DATA(data_location_in_rom) data = rom[data_location_in_rom]
#endif

#ifdef LOAD_NO_BANKS
#define ROM_MAX_LENGTH (256*1024)
uint8_t sram_rom[ROM_MAX_LENGTH];
#define SET_DATA(data_location_in_rom) data = sram_rom[data_location_in_rom]
#endif

#ifdef LOAD_BANKS_16K
// 32 banks of 16 KiB (1 bank (16KiB) in pinned cache + 31 banks (496KiB) in main RAM)
#define BANK_LENGTH (16*1024)
#define MAX_BANKS_COUNT (32)
uint8_t* xip_bank31 = (uint8_t*) (XIP_BASE+CACHE_AS_SRAM_OFFSET);
uint8_t sram_banks[31][BANK_LENGTH];
uint8_t* banks[MAX_BANKS_COUNT]; // 524288 bytes of rom data across 32 banks
#define SET_DATA(data_location_in_rom) int bank = (data_location_in_rom >> 14) & 0x1f; int addr = data_location_in_rom & 0x3fff; data = banks[bank][addr]
#endif

#ifdef LOAD_BANKS_4K
// 128 banks of 4 KiB (1 bank (4KiB) in USB RAM + 4 banks (16KiB) in pinned cache + 123 banks (492KiB) in main RAM)
#define BANK_LENGTH (4*1024)
#define MAX_BANKS_COUNT (128)
uint8_t* xip_banks = (uint8_t*) (XIP_BASE+CACHE_AS_SRAM_OFFSET);
uint8_t* usb_bank = (uint8_t*) (USBCTRL_DPRAM_BASE);
uint8_t sram_banks[123][BANK_LENGTH];
uint8_t* banks[MAX_BANKS_COUNT]; // 524288 bytes of rom data across 128 banks
#define SET_DATA(data_location_in_rom) int bank = (data_location_in_rom >> 12) & 0x7f; int addr = data_location_in_rom & 0x0fff; data = banks[bank][addr]
#endif

// FIXME Support larger ram (32 KiB) ?
uint8_t ram[8192];

cart_t cart;

roms_t my_roms;

bool selecting_rom = false;
uint8_t* selected_rom_addr;


uint8_t* ram_persistent_flash_addr() {
    return selected_rom_addr + 0x100000 - cart.ramsize;
}

void persist_ram_to_flash() {
    if (selected_rom_addr != 0 && cart.ramsize > 0) {
        uint32_t len = cart.ramsize;
        uint8_t* dest = ram_persistent_flash_addr();

        DEBUGF("Persisting %d bytes of ram (0x%08x) to flash (0x%08x)\n", len, ram, dest);
        uint32_t offset = ((uint32_t) dest) - XIP_BASE;
        
        DEBUGF("Erasing %p...%p\n", offset, offset + len);
        flash_range_erase(offset, len);
        
        DEBUGF("Writing %p...%p from %p\n", offset, offset + len, ram);
        flash_range_program(offset, ram, len);
    }
}

uint8_t* selected_rom() {
    return selected_rom_addr;
}

void set_selected_rom(uint8_t* selected) {
    selected_rom_addr = selected;
}


void pin_cache_lines() {
    DEBUGF("Pinning 16K of cache lines\n");
    // Pin cache lines for an additional 16 KiB of RAM
    xip_cache_pin_range(CACHE_AS_SRAM_OFFSET, 16*1024);
    DEBUGF("Pinned\n");
}

#ifdef LOAD_NO_BANKS
void load_no_banks(const uint8_t* romdata, uint32_t size) {
    if (size > ROM_MAX_LENGTH) {
        DEBUGF("Unsupported ROM size: %d > %d\n", size, ROM_MAX_LENGTH);
        size = ROM_MAX_LENGTH;
    }
    DEBUGF("Loading %d bytes ROM\n", size);
    memcpy(sram_rom, romdata, size);
    // Verify ROM
    if (memcmp(sram_rom, romdata, size) != 0) {
        DEBUGF("ROM mismatch\n");
    }
}
#endif

#ifdef LOAD_BANKS_16K
void load_banks_16k(const uint8_t* romdata, uint32_t size) {
    for (int i=0; i<31; i++) {
        banks[i] = sram_banks[i];
    }
    banks[31] = xip_bank31;
    // Bank 31 will NOT be zero-initialized by the BSS routine
    memset(xip_bank31, 0, BANK_LENGTH);
    // Load ROM into RAM
    int banks_count = size / BANK_LENGTH;
    DEBUGF("ROM size: %d Banks count: %d\n", size, banks_count);
    if (banks_count > MAX_BANKS_COUNT) {
        DEBUGF("Unsupported ROM size: %d banks > %d\n", banks_count, MAX_BANKS_COUNT);
        banks_count = MAX_BANKS_COUNT;
    }
    DEBUGF("Loading %d ROM banks\n", banks_count);
    for (int i=0; i<banks_count; i++) {
        memcpy(banks[i], romdata + i*BANK_LENGTH, BANK_LENGTH);
    }
    // Verify ROM
    for (int i=0; i<banks_count; i++) {
        if (memcmp(banks[i], romdata + i*BANK_LENGTH, BANK_LENGTH) != 0) {
            DEBUGF("ROM bank mismatch: %d\n", i);
        }
    }
    DEBUGF("ROM banks verified\n");
}
#endif

#ifdef LOAD_BANKS_4K
void load_banks_4k(const uint8_t* romdata, uint32_t size) {
    for (int i=0; i<123; i++) {
        banks[i] = sram_banks[i];
    }
    banks[123] = xip_banks;
    banks[124] = xip_banks + BANK_LENGTH;
    banks[125] = xip_banks + 2*BANK_LENGTH;
    banks[126] = xip_banks + 3*BANK_LENGTH;
    banks[127] = usb_bank;
    // Banks 123+ will NOT be zero-initialized by the BSS routine
    memset(xip_banks, 0, 4*BANK_LENGTH);
    memset(usb_bank, 0, BANK_LENGTH);
    // Load ROM into RAM
    int banks_count = size / BANK_LENGTH;
    DEBUGF("ROM size: %d Banks count: %d\n", size, banks_count);
    if (banks_count > MAX_BANKS_COUNT) {
        DEBUGF("Unsupported ROM size: %d banks > %d\n", banks_count, MAX_BANKS_COUNT);
        banks_count = MAX_BANKS_COUNT;
    }
    DEBUGF("Loading %d ROM banks\n", banks_count);
    for (int i=0; i<banks_count; i++) {
        memcpy(banks[i], romdata + i*BANK_LENGTH, BANK_LENGTH);
    }
    // Verify ROM
    for (int i=0; i<banks_count; i++) {
        if (memcmp(banks[i], romdata + i*BANK_LENGTH, BANK_LENGTH) != 0) {
            DEBUGF("ROM bank mismatch: %d\n", i);
        }
    }
    DEBUGF("ROM banks verified\n");
}
#endif

cart_t init_rom(const uint8_t* romdata, uint32_t size) {
    // Pin cache lines if required
#if defined(LOAD_BANKS_16K) || defined(LOAD_BANKS_4K)
    pin_cache_lines();
#endif

    // Copy to RAM if required
#ifdef LOAD_NO_BANKS
    load_no_banks(romdata, size);
#endif

#ifdef LOAD_BANKS_16K
    load_banks_16k(romdata, size);
#endif

#ifdef LOAD_BANKS_4K
    load_banks_4k(romdata, size);
#endif

    DEBUGF("Loaded ROM at 0x%p\n", romdata);

    memset(&cart, 0, sizeof(cart));

    // Read ROM header
    cart.type = romdata[0x147];
    cart.rom = romdata[0x148];
    cart.ram = romdata[0x149];

    // TODO Read ROM size from header ?!
    cart.romsize = size;

    if (cart.type == 0x02 || cart.type == 0x03 || cart.type == 0x1a || cart.type == 0x1b || cart.type == 0x1d || cart.type == 0x1e) {
        cart.has_ram = true;
        if (cart.ram < 2) {
            cart.ramsize = 0;
        } else if (cart.ram == 2) {
            cart.ramsize = 8 * 1024;
        } else if (cart.ram == 3) {
            cart.ramsize = 32 * 1024;
        } else if (cart.ram == 4) {
            cart.ramsize = 128 * 1024;
        } else if (cart.ram == 5) {
            cart.ramsize = 64 * 1024;
        }
    }

    if (cart.type == 0x03 || cart.type == 0x1b || cart.type == 0x1e) {
        cart.has_battery = true;
    }

    if (cart.type == 0x1c || cart.type == 0x1d || cart.type == 0x1e) {
        cart.has_rumble = true;
    }
    
    if (cart.type == 0) { // 32KiB bankless
        cart.loop = &loop_32kb;
        DEBUGF("ROM type: 32KiB bankless\n");
    } else if (cart.type >= 0x01 && cart.type <= 0x03) { // MBC1
        cart.loop = &loop_mbc1;
        DEBUGF("ROM type: MBC1\n");
    } else if (cart.type >= 0x19 && cart.type <= 0x1e) { // MBC5
        cart.loop = &loop_mbc5;
        DEBUGF("ROM type: MBC5\n");
    } else {
        DEBUGF("ROM type: Unsupported\n");
    }

    // Load ram from flash
    if (selected_rom_addr != 0 && cart.ramsize > 0) {
        uint8_t* src = ram_persistent_flash_addr();
        DEBUGF("Loading RAM from 0x%08x\n", src);
        if (cart.ramsize > sizeof(ram)) {
            DEBUGF("Unsupported ram size: %d bytes (max. supported: %d bytes)\n", cart.ramsize, sizeof(ram));
        } else {
            memcpy(ram, src, cart.ramsize);
        }
    }

    return cart;
}

void __not_in_flash_func(loop_launcher)() {
    DEBUGF("loop_launcher: Waiting for GB to boot...\n");

    while((gpio_get_all64() & GB_RD_PIN_MASK) == 0) {
        tight_loop_contents();
    }

    while (true) {
        while((gpio_get_all64() & GB_RD_PIN_MASK) != 0) {
             tight_loop_contents();
        }
        uint32_t address = (gpio_get_all64() & GB_ADDR_PINS_MASK);
        uint8_t data = 0xff;
        if ((address & 0x8000) == 0 && address < cart.romsize) {
            uint32_t data_location_in_rom = address;
            SET_DATA(data_location_in_rom);
        } else if (address >= 0xb000 && address < 0xb400) {
            // Rom entries
            uint16_t offset = address - 0xb000;
            data = *((uint8_t*)(&my_roms) + offset);
        } else if (selecting_rom && address >= 0xb400 && address < 0xb400 + my_roms.count) {
            // Trigger trigger rom load by reading an offset (rom index) in 0xb400
            uint16_t offset = address - 0xb400;
            // Use a sequence of reads (first 0xbfff, then 0xb40n) to avoid false positives
            if (offset >= 0 && offset < my_roms.count) {
                selected_rom_addr = my_roms.entries[offset].address;
                // Break loop, hand it over to main
                break;
            }
        } else if (address == 0xbfff) {
            selecting_rom = true;
        }
        uint64_t data_out = data << GB_DATA_PINS_SHIFT;
        gpio_set_dir_out_masked64(GB_DATA_PINS_MASK);
        gpio_put_masked64(GB_DATA_PINS_MASK, data_out);
        // FIXME when to set back to input ??
        //gpio_set_dir_in_masked64(GB_DATA_PINS_MASK);
        //gpio_clr_mask64(GB_DATA_PINS_MASK);
    }
}

void __not_in_flash_func(loop_32kb)() {
    DEBUGF("loop_32kb: Waiting for GB to boot...\n");

    while((gpio_get_all64() & GB_RD_PIN_MASK) == 0) {
        tight_loop_contents();
    }

    while (true) {
        while((gpio_get_all64() & GB_RD_PIN_MASK) != 0) {
             tight_loop_contents();
        }
        uint32_t address = (gpio_get_all64() & GB_ADDR_PINS_MASK);
        uint8_t data = 0xff;
        if ((address & 0x8000) == 0 && address < cart.romsize) {
            uint32_t data_location_in_rom = address;
            SET_DATA(data_location_in_rom);
        }
        uint64_t data_out = data << GB_DATA_PINS_SHIFT;
        gpio_set_dir_out_masked64(GB_DATA_PINS_MASK);
        gpio_put_masked64(GB_DATA_PINS_MASK, data_out);
        // FIXME when to set back to input ??
        //gpio_set_dir_in_masked64(GB_DATA_PINS_MASK);
        //gpio_clr_mask64(GB_DATA_PINS_MASK);
    }
}

void __not_in_flash_func(loop_mbc1)() {
    uint8_t rombank = 1;
    uint8_t rambank = 0;
    bool ram_enabled = false;

    DEBUGF("loop_mbc1: Waiting for GB to boot...\n");

    while((gpio_get_all64() & GB_RD_PIN_MASK) == 0) {
        tight_loop_contents();
    }

    while (true) {
        while((gpio_get_all64() & GB_CTRL_PINS_MASK) == GB_CTRL_PINS_MASK) {
             tight_loop_contents();
        }
        bool writing = (gpio_get_all64() & GB_WR_PIN_MASK) == 0;
        uint32_t address = (gpio_get_all64() & GB_ADDR_PINS_MASK);
        if (writing) {
            // READ from data pins
            gpio_set_dir_in_masked64(GB_DATA_PINS_MASK);
            gpio_clr_mask64(GB_DATA_PINS_MASK);
            uint8_t data = (gpio_get_all64() & GB_DATA_PINS_MASK) >> GB_DATA_PINS_SHIFT;
            // Registers: 0x0000-0x1fff to set enable/disable ram
            if (address <= 0x1fff) {
                ram_enabled = (data & 0xf) == 0xa;
            }
            // Registers: 0x2000-0x3fff to set rom bank
            else if (address <= 0x3fff) {
                rombank = (data == 0) ? 1 : (data & 0x1f);  // TODO remove unused bits for small roms ?
            }
            // Registers: 0x4000-0x5fff to set ram bank
            else if (address <= 0x5fff) {
                rambank = data & 0x03;
            }
            // Write to ram
            else if (ram_enabled && address >= 0xa000 && address <= 0xbfff) {
                uint32_t data_location_in_ram = (rambank << 13) + (address & 0x1fff);
                if (data_location_in_ram < cart.ramsize) {
                    ram[data_location_in_ram] = data;
                }
            }
            continue;
        }
        uint8_t data = 0xff;
        // Read from rom
        if ((address & 0x8000) == 0 && address < cart.romsize) {
            uint32_t data_location_in_rom;
            if ((address & 0xc000) == 0) {
                // 0x0000-0x3fff: Bank 0
                data_location_in_rom = address;
            } else {
                // 0x4000-0x7fff: Current bank
                data_location_in_rom = (rombank << 14) + (address & 0x3fff);
            }
            SET_DATA(data_location_in_rom);
        }
        // Read from ram
        else if (ram_enabled && address >= 0xa000 && address <= 0xbfff) {
            uint32_t data_location_in_ram = (rambank << 13) + (address & 0x1fff);
            if (data_location_in_ram < cart.ramsize) {
                data = ram[data_location_in_ram];
            }
        }
        uint64_t data_out = data << GB_DATA_PINS_SHIFT;
        gpio_set_dir_out_masked64(GB_DATA_PINS_MASK);
        gpio_put_masked64(GB_DATA_PINS_MASK, data_out);
        // FIXME when to set back to input ??
        //gpio_set_dir_in_masked64(GB_DATA_PINS_MASK);
        //gpio_clr_mask64(GB_DATA_PINS_MASK);
    }
}

void __not_in_flash_func(loop_mbc5)() {
    uint16_t rombank = 1;
    uint8_t rambank = 0;
    bool ram_enabled = false;

    DEBUGF("loop_mbc5: Waiting for GB to boot...\n");

    while((gpio_get_all64() & GB_RD_PIN_MASK) == 0) {
        tight_loop_contents();
    }

    while (true) {
        while((gpio_get_all64() & GB_CTRL_PINS_MASK) == GB_CTRL_PINS_MASK) {
             tight_loop_contents();
        }
        bool writing = (gpio_get_all64() & GB_WR_PIN_MASK) == 0;
        uint32_t address = (gpio_get_all64() & GB_ADDR_PINS_MASK);
        if (writing) {
            // READ from data pins
            gpio_set_dir_in_masked64(GB_DATA_PINS_MASK);
            gpio_clr_mask64(GB_DATA_PINS_MASK);
            uint8_t data = (gpio_get_all64() & GB_DATA_PINS_MASK) >> GB_DATA_PINS_SHIFT;
            // Registers: 0x0000-0x1fff to set enable/disable ram
            if (address <= 0x1fff) {
                ram_enabled = (data & 0xf) == 0xa;
            }
            // Registers: 0x2000-0x2fff to set low 8 bits of rom bank
            else if (address <= 0x2fff) {
                rombank = (rombank & 0x100) | data;
            }
            // Registers: 0x3000-0x3fff to set 9th bit of rom bank
            else if (address <= 0x3fff) {
                rombank = ((data & 0x01) << 8) | (rombank & 0xff);
            }
            // Registers: 0x4000-0x5fff to set ram bank
            else if (address <= 0x5fff) {
                // If cartridge has a rumble motor, bit 3 of ram bank is used to control it and should _not_ affect selected ram bank
                if (cart.has_rumble) {
                    rambank = data & 0x07;
                } else {
                    rambank = data & 0x0f;
                }
            }
            // Write to ram
            else if (ram_enabled && address >= 0xa000 && address <= 0xbfff) {
                uint32_t data_location_in_ram = (rambank << 13) + (address & 0x1fff);
                if (data_location_in_ram < cart.ramsize) {
                    ram[data_location_in_ram] = data;
                }
            }
            continue;
        }
        uint8_t data = 0xff;
        // Read from rom
        if ((address & 0x8000) == 0 && address < cart.romsize) {
            uint32_t data_location_in_rom;
            if ((address & 0xc000) == 0) {
                // 0x0000-0x3fff: Bank 0
                data_location_in_rom = address;
            } else {
                // 0x4000-0x7fff: Current bank
                data_location_in_rom = (rombank << 14) + (address & 0x3fff);
            }
            SET_DATA(data_location_in_rom);
        }
        // Read from ram
        else if (ram_enabled && address >= 0xa000 && address <= 0xbfff) {
            uint32_t data_location_in_ram = (rambank << 13) + (address & 0x1fff);
            if (data_location_in_ram < cart.ramsize) {
                data = ram[data_location_in_ram];
            }
        }
        uint64_t data_out = data << GB_DATA_PINS_SHIFT;
        gpio_set_dir_out_masked64(GB_DATA_PINS_MASK);
        gpio_put_masked64(GB_DATA_PINS_MASK, data_out);
        // FIXME when to set back to input ??
        //gpio_set_dir_in_masked64(GB_DATA_PINS_MASK);
        //gpio_clr_mask64(GB_DATA_PINS_MASK);
    }
}

const char* magic = "pico-gb-rom     ";
uint16_t bsd_checksum(uint8_t* addr, uint32_t size) {
    uint8_t ch;
    uint16_t checksum = 0;

    for (int i=0; i<size; i++) {
        ch = *(addr + 32 + i);
        checksum = (checksum >> 1) + ((checksum & 1) << 15);
        checksum += ch;
        checksum &= 0xffff;
    }
    return checksum;
}
void find_rom_entries() {
    DEBUGF("find_rom_entries\n");
    int romIndex = 0;
    for (int i=1; i<16; i++) {
        uint8_t* addr = (uint8_t*) 0x10000000 + i * 0x100000;
        DEBUGF("addr=0x%08x\n", addr);
        if (memcmp(addr, magic, 16) == 0) {
            // Found magic bytes
            DEBUGF("found magic\n");
            uint32_t size = *((uint32_t*) (addr + 16));
            DEBUGF("size=%d\n", size);
            uint16_t checksum = *((uint16_t*) (addr + 30));
            DEBUGF("checksum=0x%04x\n", checksum);
            if (checksum == bsd_checksum(addr, size)) {
                // Checksum matches
                DEBUGF("checksum matches\n");
                // Read name from rom header
                uint8_t* romdata = addr + 32;
                uint8_t* name = &romdata[0x134];
                if (strcmp(name, "") != 0) {
                    strncpy(my_roms.entries[romIndex].name, name, 15);
                } else {
                    strcpy(my_roms.entries[romIndex].name, "ROM ###");
                }
                my_roms.entries[romIndex].address = addr;
                romIndex++;
            }
        }
    }
    DEBUGF("find_rom_entries: %d\n", romIndex);
    my_roms.count = romIndex;
}
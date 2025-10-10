#include <gb/gb.h>
#include <stdint.h>
#include <stdio.h>

#include "../../shared/romlist.h"

// Rom entries are read on cartridge at 0xb000
// Rom load is triggered by reading at an offset (rom index) from 0xb400

//#define DEBUG 1
#ifdef DEBUG
roms_t my_roms = {
    14,
    {
        { "ROM 1 987654321", (void*) 0x10001000 },
        { "ROM 2          ", (void*) 0x10002000 },
        { "ROM 3          ", (void*) 0x10003000 },
        { "ROM 4          ", (void*) 0x10004000 },
        { "ROM 5          ", (void*) 0x10005000 },
        { "ROM 6          ", (void*) 0x10006000 },
        { "ROM 7          ", (void*) 0x10007000 },
        { "ROM 8          ", (void*) 0x10008000 },
        { "ROM 9          ", (void*) 0x10009000 },
        { "ROM 10         ", (void*) 0x1000a000 },
        { "ROM 11         ", (void*) 0x1000b000 },
        { "ROM 12         ", (void*) 0x1000c000 },
        { "ROM 13         ", (void*) 0x1000d000 },
        { "ROM 14         ", (void*) 0x1000e000 }
    }
};

roms_t* roms = &my_roms;
#else
roms_t* roms = 0xb000;
#endif

char* pretrigger = 0xbfff;
char* trigger = 0xb400;

const uint8_t emptyTile = 0x00;     // Character ' '
const uint8_t cursorTile = 0x1e;    // Character '>'


void main(void) {
    printf("\n  ~ pico-gb-cart ~ \n");

    // Draw ROM entries
    int i;
    for (i=0; i<roms->count; i++) {
        printf("\n    ");
        printf(roms->entries[i].name);
    }

    char cursorPos = 0;
    char keydownpressed = 0;
    char keyuppressed = 0;
    char keyapressed = 0;

    // Draw cursor
    set_vram_byte(get_bkg_xy_addr(2, 3), cursorTile);
    
    while (1) {
        vsync();

        uint8_t joy = joypad();

        // Navigate with cursor
        if (joy & J_DOWN) {
            if (keydownpressed == 0) {
                keydownpressed = 1;
                set_vram_byte(get_bkg_xy_addr(2, cursorPos+3), emptyTile);
                cursorPos++;
                if (cursorPos >= roms->count) {
                    cursorPos = 0;
                }
                set_vram_byte(get_bkg_xy_addr(2, cursorPos+3), cursorTile);
            }
        } else {
            keydownpressed = 0;
        }
        if (joy & J_UP) {
            if (keyuppressed == 0) {
                keyuppressed = 1;
                set_vram_byte(get_bkg_xy_addr(2, cursorPos+3), emptyTile);
                cursorPos--;
                if (cursorPos < 0) {
                    cursorPos = roms->count - 1;
                }
                set_vram_byte(get_bkg_xy_addr(2, cursorPos+3), cursorTile);
            }
        } else {
            keyuppressed = 0;
        }

        // A button loads the selected ROM
        if (joy & J_A) {
            if (keyapressed == 0) {
                keyapressed = 1;
                
                // Reading this address triggers the ROM load and console reset
                volatile char pre = *(pretrigger);
                volatile char status = *(trigger + cursorPos);
                //printf("\nSTATUS=0x%02x POS=%d", status, cursorPos);
                //printf("\nTRIGGER=0x%02x ADDR=0x%04x", trigger, trigger+cursorPos);
            }
        } else {
            keyapressed = 0;
        }
    }
}

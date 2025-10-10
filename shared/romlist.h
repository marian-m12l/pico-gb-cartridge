#pragma once

#ifdef __SDCC
#define PACKED
#define PADDING void* padding;
#else
#define PACKED __attribute__(( __packed__ ))
#define PADDING
#endif

typedef struct PACKED {
    //char len;
    char name[16];
    void* address;
    PADDING
} rom_t;

typedef struct PACKED {
    char count;
    rom_t entries[14];
} roms_t;

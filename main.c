#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/vreg.h"

#include "bus.h"
#include "pins.h"
#include "launcher.h"


int main() {
    // Overclock to 330MHz
    vreg_set_voltage(VREG_VOLTAGE_1_20);
    set_sys_clock_khz(330000, true);

#ifdef ENABLE_UART
    stdio_init_all();
#endif

#ifdef ENABLE_BUS
    // Configure GPIOs
    gpio_set_dir_masked64(GB_ALL_PINS_MASK, 0x0000000000000000);
    gpio_put_masked64(GB_ALL_PINS_MASK, 0x0000000000000000);
    gpio_set_function_masked64(GB_ALL_PINS_MASK, GPIO_FUNC_SIO);

    // Disable hysteresis on /RD to shave off 2 cycles of latency
    gpio_set_input_hysteresis_enabled(GB_RD_PIN, false);

    gpio_set_slew_rate(GB_DATA_PINS_SHIFT, GPIO_SLEW_RATE_FAST);
    gpio_set_slew_rate(GB_DATA_PINS_SHIFT+1, GPIO_SLEW_RATE_FAST);
    gpio_set_slew_rate(GB_DATA_PINS_SHIFT+2, GPIO_SLEW_RATE_FAST);
    gpio_set_slew_rate(GB_DATA_PINS_SHIFT+3, GPIO_SLEW_RATE_FAST);
    gpio_set_slew_rate(GB_DATA_PINS_SHIFT+4, GPIO_SLEW_RATE_FAST);
    gpio_set_slew_rate(GB_DATA_PINS_SHIFT+5, GPIO_SLEW_RATE_FAST);
    gpio_set_slew_rate(GB_DATA_PINS_SHIFT+6, GPIO_SLEW_RATE_FAST);
    gpio_set_slew_rate(GB_DATA_PINS_SHIFT+7, GPIO_SLEW_RATE_FAST);

    // Hold console in reset until rom is loaded and loop is started
    gpio_put(GB_RESET_PIN, 0);
    gpio_set_drive_strength(GB_RESET_PIN, GPIO_DRIVE_STRENGTH_12MA);
    gpio_set_dir(GB_RESET_PIN, true);

    // Look for ROMs in flash memory
    find_rom_entries();

    // Load menu and loop
    init_rom(launcher_rom, launcher_rom_size);

    // Release reset on console
    gpio_set_dir(GB_RESET_PIN, false);

    loop_launcher();

    // Rom was selected, load and run loop
    uint8_t* selected = selected_rom();
    if (selected != 0) {
        // Hold console in reset until rom is loaded and loop is started
        gpio_set_dir(GB_RESET_PIN, true);

        uint32_t size = *((uint32_t*) (selected + 16));
        uint8_t romtype = init_rom(selected + 32, size);
        
        // Release reset on console
        gpio_set_dir(GB_RESET_PIN, false);

        if (romtype == 0) { // 32KiB bankless ROM
            loop_32kb();
        } else if (romtype == 1) {  // TODO Banking ? MBCx ? SRAM ? ...
            //loop_hirom();
        }
    }
#endif

    while(true) {
        tight_loop_contents();
    }
    return 0;
}

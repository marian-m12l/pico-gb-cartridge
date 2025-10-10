# RP2350-based Game Boy cartridge


## Pins

On rp2350 board (e.g. PGA2350):

- Address bus `A0`-`A15`: GPIO 0 (`A0`) to 15 (`A15`)
- Data bus `D0`-`D7`: GPIO 16 (`D0`) to 23 (`D7`)
- Reset pin `/RESET`: GPIO 24
- Chip select pin `/CS`: GPIO 25
- Read pin `/RD`: GPIO 26
- Write pin `/WR`: GPIO 27
- Clock pin `CLK`: GPIO 28
- Audio pin `AUDIO`: GPIO 29

- UART pins `TX` and `RX`: GPIO 44 and 45

## Build instructions

```
mkdir build
cd build
cmake -DPICO_SDK_PATH=/path/to/pico-sdk-2.1.0 -DPICO_BOARD=pimoroni_pga2350 ..
make
```

# Adding ROMs

Add `rom.gb` in slot `1`:
```
./tools/loadrom.sh rom.gb 1"
```

Each slot (from 1 to 14) occupies 1MiB in flash memory.

# Running

```
$ openocd-rpi/installed/bin/openocd -f interface/jlink.cfg -c "transport select swd" -c "adapter speed 6000" -f target/rp2350.cfg
$ arm-none-eabi-gdb -ex 'target remote localhost:3333' -ex 'load' -ex 'monitor reset init' -ex 'continue' pico-gb-cartridge.elf
$ picocom -b 115200 /dev/ttyACM0 -g picocom.log
```

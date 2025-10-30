# RP2350-based Game Boy cartridge

![launcher](https://github.com/user-attachments/assets/f570e777-22eb-4787-b23b-830a3acacbd1)

![ingame](https://github.com/user-attachments/assets/6fe78c4f-f3ac-4561-988c-c138041f281f)

https://github.com/user-attachments/assets/1f53fd3e-0f1c-4872-a420-8dec0f6acc66

![gb_cart_1](https://github.com/user-attachments/assets/d729a48d-4ac1-476d-beb8-cb5565ad9255)


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

- On-board button (active low) to persist sram / reset game or to launcher: GPIO 30

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

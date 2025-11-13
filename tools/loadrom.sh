#!/bin/bash

OPENOCD=openocd

# List slots
if [ "$#" -eq 0 ]; then
    for i in {1..14}; do
        # Magic bytes
        addr=$(printf "0x%X" $((i * 0x100000 + 0x10000000)))
        output=$($OPENOCD -f interface/jlink.cfg -c "transport select swd" -c "adapter speed 6000" -f target/rp2350.cfg -c "init; rp2350.dap.core0 read_memory $addr 8 16" -c "exit;" 2>&1 | tail -1)
        magic=$(echo -n $output | xxd -r -p)
        if [ 'pico-gb-rom     ' = "$magic" ]; then
            # Rom size
            addr=$(printf "0x%X" $((i * 0x100000 + 0x10000000 + 0x10)))
            output=$($OPENOCD -f interface/jlink.cfg -c "transport select swd" -c "adapter speed 6000" -f target/rp2350.cfg -c "init; rp2350.dap.core0 mdw $addr 1" -c "exit;" 2>&1 | tail -1)
            size=$(echo -n $((16#$(echo -n $output | cut -d' ' -f2))))
            size=$(($size / 1024))
            # Name from cartridge header
            addr=$(printf "0x%X" $((i * 0x100000 + 0x10000000 + 0x20 + 0x134)))
            output=$($OPENOCD -f interface/jlink.cfg -c "transport select swd" -c "adapter speed 6000" -f target/rp2350.cfg -c "init; rp2350.dap.core0 read_memory $addr 8 15" -c "exit;" 2>&1 | tail -1)
            name=$(echo -n $output | xxd -r -p | tr -d '\0')
            if [ "$name" = '' ]; then
                name="ROM ###"
            fi
            echo "Slot #$i occupied: $name ($size KiB)"
        else
            echo "Slot #$i free"
        fi
    done

    exit 0
fi


# Clear slot
if [ "$#" -eq 1 ]; then
    index=$1    # Between 1 and 14

    # Destination address
    addr=$(printf "0x%X" $((index * 0x100000 + 0x10000000)))

    echo "erasing rom at address $addr"

    $OPENOCD -f interface/jlink.cfg -c "transport select swd" -c "adapter speed 6000" -f target/rp2350.cfg -c "init; halt; flash erase_address $addr 0x20000; resume; exit"

    exit 0
fi


# Program slot
rom=$1
index=$2    # Between 1 and 14

# Magic bytes
echo -en 'pico-gb-rom     ' > /tmp/rom.bin

# Rom size
size=$(stat -c "%s" "$rom")
echo $size
size=$(printf "0x%X" $((size)))
echo $size
printf "0: %.4x" $((size & 0xffff)) | xxd -r -g0 | dd conv=swab >> /tmp/rom.bin
printf "0: %.4x" $((size >> 16)) | xxd -r -g0 | dd conv=swab >> /tmp/rom.bin

# Padding
echo -en '\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff' >> /tmp/rom.bin

# Checksum
cksum -a bsd --raw "$rom" | dd conv=swab >> /tmp/rom.bin
cat "$rom" >> /tmp/rom.bin

# Destination address
addr=$(printf "0x%X" $((index * 0x100000 + 0x10000000)))

echo "programming rom $1 at address $addr"

$OPENOCD -f interface/jlink.cfg -c "transport select swd" -c "adapter speed 6000" -f target/rp2350.cfg -c "program /tmp/rom.bin verify reset exit $addr"
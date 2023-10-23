../configure --enable-debug --target-list=arm-softmmu
../configure --enable-debug --target-list=arm-softmmu --enable-sanitizers

./qemu-system-arm -M arduino-uno-rev4 -nographic --trace 'nvic_*'
./qemu-system-arm -M arduino-uno-rev4 -nographic -d guest_errors
./qemu-system-arm -M arduino-uno-rev4 -nographic -d guest_errors -bootloader ~/work/stuff/dfu_minima.hex
./qemu-system-arm -M arduino-uno-rev4 -nographic -bootloader ~/work/stuff/bl_minima.hex -kernel ~/work/stuff/serial_write.hex -serial null -serial /dev/ttyS0

arm-none-eabi-objdump --disassembler-options=force-thumb -d /tmp/dfu_minima.o | nvim
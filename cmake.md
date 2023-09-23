../configure --enable-debug --target-list=arm-softmmu
../configure --enable-debug --target-list=arm-softmmu --enable-sanitizers

./qemu-system-arm -M arduino-uno-rev4 -nographic --trace 'nvic_*'
./qemu-system-arm -M arduino-uno-rev4 -nographic -d guest_errors
./qemu-system-arm -M arduino-uno-rev4 -nographic -d guest_errors -bootloader ~/work/stuff/dfu_minima.hex

arm-none-eabi-objdump --disassembler-options=force-thumb -d /tmp/dfu_minima.o | nvim
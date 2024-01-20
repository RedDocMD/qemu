../configure --enable-debug --target-list=arm-softmmu
../configure --enable-debug --target-list=arm-softmmu --enable-sanitizers

./qemu-system-arm -M arduino-uno-rev4 -nographic --trace 'nvic_*'
./qemu-system-arm -M arduino-uno-rev4 -nographic -d guest_errors
./qemu-system-arm -M arduino-uno-rev4 -nographic -d guest_errors -bootloader ~/work/stuff/dfu_minima.hex
./qemu-system-arm -M arduino-uno-rev4 -nographic -bootloader ~/work/stuff/bl_minima.hex -kernel ~/work/stuff/serial_write.hex -serial null -serial /dev/ttyS0
./qemu-system-arm -M arduino-uno-rev4 -nographic -bootloader ~/work/stuff/dfu_minima.hex -kernel ~/work/stuff/serial_read.hex -serial null -serial unix:/tmp/serial.socket,server
./qemu-system-arm -M arduino-uno-rev4 -nographic -bootloader ~/work/stuff/bl_minima.hex -kernel ~/work/stuff/serial_write.hex -serial null -serial stdio -monitor none
./qemu-system-arm -M arduino-uno-rev4 -nographic -bootloader ~/work/stuff/bl_minima.hex -kernel ~/work/stuff/digital_write.hex -chardev socket,id=pin,port=3000,host=localhost,ipv4=on,server=on,websocket=on,wait=off
./qemu-system-arm -M arduino-uno-rev4 -nographic -bootloader ~/work/stuff/bl_minima.hex -kernel ~/work/stuff/mixed_wave.hex -chardev socket,id=pin,port=3400,host=localhost,ipv4=on,server=on,websocket=on,wait=off -chardev socket,id=scope,port=3500,host=localhost,ipv4=on,server=on,wait=off -serial null -serial null -monitor stdio

arm-none-eabi-objdump --disassembler-options=force-thumb -d /tmp/dfu_minima.o | nvim
minicom -D unix:/tmp/serial.socket

./qemu-system-arm -M raspi2b -nographic -monitor stdio -serial null -s

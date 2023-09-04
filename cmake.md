../configure --enable-debug --target-list=arm-softmmu
./qemu-system-arm -M arduino-uno-rev4 -nographic --trace 'nvic_*'

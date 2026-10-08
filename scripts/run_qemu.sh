qemu-system-x86_64 \
    -drive format=raw,file=bin/os.img,index=0,media=disk \
    -m 32M \
    -no-reboot \
    -no-shutdown
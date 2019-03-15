rm *.bin.*
aarch64-linux-gnu-gcc server.c -lpthread -o server.bin.arm
aarch64-linux-gnu-gcc client.c -lpthread -o client.bin.arm
gcc server.c -lpthread -o server.bin.x86
gcc client.c -lpthread -o client.bin.x86

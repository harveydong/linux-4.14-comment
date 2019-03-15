#aarch64-linux-gnu-gcc server.c -lpthread -o server.bin
#aarch64-linux-gnu-gcc client.c -lpthread -o client.bin
gcc server.c -lpthread -o server.bin
gcc client.c -lpthread -o client.bin

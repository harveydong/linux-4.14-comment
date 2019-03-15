ps -aux | grep server.bin | sed -n 's/root *\(\w*\).*/\1/p' | xargs sudo kill -9
ps -aux | grep server.bin

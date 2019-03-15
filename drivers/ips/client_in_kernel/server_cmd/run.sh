echo ====== make ======
./make.sh

echo ====== kill ======
./kill.sh

echo ====== server.bin ======
sudo ./server.bin.x86 &
#use nice to improve response
#nice --adjustment=-20 /mnt/x2a/server.bin.arm &
sleep 2

echo ====== client.bin ======
./client.bin.x86 127.0.0.1

echo ====== check server exit ======
ps -aux | grep server.bin.x86

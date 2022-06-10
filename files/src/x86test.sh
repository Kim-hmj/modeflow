echo -35 > /data/r1_volume_db
sudo killall tail
sudo rm /dev/shm/ryder*
./monitormfl.sh &
cat testfile.txt | ./r1-modeflow

#!/bin/bash

# Script to start artnet_receiver on boot
sudo apt update
sudo apt upgrade

# build dependencies for rpi-gpu-hub75-matrix
sudo apt install build-essential gcc make libgles2-mesa-dev libgbm-dev libegl1-mesa-dev libavformat-dev libswscale-dev

cd ./rpi-gpu-hub75-matrix

# NOTE: you cannot compile for multiple boards. pin configuration is defined at compile time
# compile with supports for hzeller's 3 port board (default):
make DEF="-DHZELLER=1"

sudo make install

gcc artnet_receiver.c  -Wall -O3 -lrpihub75 -o artnet_receiver

./artnet_receiver -x 576 -y 32 -c 9 -p 1 -b 100 -g 1.8 -f 120

#!/bin/bash
# Script to start artnet_receiver on boot
cd ~/rpi-gpu-hub75-matrix

make lib

sudo make install

gcc artnet_receiver.c  -Wall -O3 -lrpihub75 -o artnet_receiver

./artnet_receiver -x 576 -y 32 -c 9 -p 1 -b 100 -g 1.8 -f 120

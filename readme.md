```bash
sudo chmod +x ./start_artnet_receiver.sh
sudo cp start_artnet_receiver.service /etc/systemd/system/start_artnet_receiver.service
sudo systemctl daemon-reload
sudo systemctl enable start_artnet_receiver.service
sudo systemctl start start_artnet_receiver.service
sudo systemctl status start_artnet_receiver.service
```

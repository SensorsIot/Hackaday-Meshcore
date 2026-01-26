# USB via Ethernet for Proxmox

Use Raspberry Pi devices as remote USB hosts for Proxmox VMs via USB/IP.

---

# Architecture

```
[ Raspberry Pi ]  ← USB →  MCU
       │
       │ USB/IP (Wi-Fi/Ethernet)
       ▼
[ Proxmox VM ]  →  /dev/ttyACM0 or /dev/ttyUSB0
```

---

# Raspberry Pi Setup (USB/IP Server)

## 1. Install USB/IP

```bash
sudo apt update
sudo apt install usbip
```

## 2. Enable kernel modules

```bash
echo "usbip_host" | sudo tee -a /etc/modules
echo "vhci_hcd" | sudo tee -a /etc/modules
sudo modprobe usbip_host
sudo modprobe vhci_hcd
```

## 3. Create and enable USB/IP daemon service

```bash
sudo tee /etc/systemd/system/usbipd.service << 'EOF'
[Unit]
Description=USB/IP Host Daemon
After=network.target

[Service]
ExecStart=/usr/sbin/usbipd
Type=simple
Restart=on-failure

[Install]
WantedBy=multi-user.target
EOF

sudo systemctl daemon-reload
sudo systemctl enable --now usbipd
```

## 4. Identify USB device busid

```bash
usbip list -l
```

## 5. Create persistent bind service

```bash
sudo tee /etc/systemd/system/usbip-bind.service << 'EOF'
[Unit]
Description=Bind USB devices for USB/IP
After=usbipd.service
Requires=usbipd.service

[Service]
Type=oneshot
ExecStart=/usr/sbin/usbip bind -b <BUSID>
RemainAfterExit=yes

[Install]
WantedBy=multi-user.target
EOF

sudo systemctl daemon-reload
sudo systemctl enable --now usbip-bind.service
```

## 6. Enable overlay filesystem (optional, protects SD card)

```bash
sudo raspi-config nonint enable_overlayfs
sudo reboot
```

---

# Proxmox VM Setup (USB/IP Client)

## 1. Install standard kernel (required)

Cloud kernels do not include `vhci_hcd`. Install standard kernel:

```bash
sudo apt update
sudo apt install linux-image-amd64
```

Set GRUB to boot standard kernel:

```bash
grep menuentry /boot/grub/grub.cfg
# Update GRUB_DEFAULT in /etc/default/grub
sudo update-grub
sudo reboot
```

## 2. Install USB/IP and load module

```bash
sudo apt install usbip
sudo modprobe vhci_hcd
echo "vhci_hcd" | sudo tee -a /etc/modules
```

## 3. Create auto-attach service

```bash
sudo tee /etc/systemd/system/usbip-attach.service << 'EOF'
[Unit]
Description=Attach USB device via USB/IP
After=network-online.target
Wants=network-online.target

[Service]
Type=oneshot
ExecStart=/bin/bash -c 'sleep 5 && /usr/sbin/usbip attach -r <PI_IP> -b <BUSID>'
ExecStop=/usr/sbin/usbip detach -p 0
RemainAfterExit=yes
Restart=on-failure
RestartSec=10

[Install]
WantedBy=multi-user.target
EOF

sudo systemctl daemon-reload
sudo systemctl enable usbip-attach.service
```

## 4. Create watchdog for Pi reboots

```bash
sudo tee /usr/local/bin/usbip-watchdog.sh << 'EOF'
#!/bin/bash
if ! lsusb | grep -q '<VENDOR_ID>:<PRODUCT_ID>'; then
    /usr/sbin/usbip detach -p 0 2>/dev/null
    /usr/sbin/usbip attach -r <PI_IP> -b <BUSID> 2>/dev/null
fi
EOF
sudo chmod +x /usr/local/bin/usbip-watchdog.sh

sudo tee /etc/systemd/system/usbip-watchdog.service << 'EOF'
[Unit]
Description=USB/IP Watchdog

[Service]
Type=oneshot
ExecStart=/usr/local/bin/usbip-watchdog.sh
EOF

sudo tee /etc/systemd/system/usbip-watchdog.timer << 'EOF'
[Unit]
Description=USB/IP Watchdog Timer

[Timer]
OnBootSec=60
OnUnitActiveSec=30

[Install]
WantedBy=timers.target
EOF

sudo systemctl daemon-reload
sudo systemctl enable --now usbip-watchdog.timer
```

## 5. Manual commands

```bash
# List devices on Pi
usbip list -r <PI_IP>

# Attach
sudo usbip attach -r <PI_IP> -b <BUSID>

# Detach
usbip port
sudo usbip detach -p 0
```

---

# Devcontainer USB Access

```json
{
  "runArgs": [
    "--privileged",
    "--device=/dev/bus/usb:/dev/bus/usb",
    "--device=/dev/ttyACM0:/dev/ttyACM0"
  ]
}
```

---

# Claude Code Persistence Across Container Rebuilds

To persist Claude authentication and `/resume` history across container rebuilds:

## 1. Add mount for Claude data in `devcontainer.json`

```json
{
  "mounts": [
    "source=${localWorkspaceFolder}/.claude-data,target=/home/dev/.claude-data,type=bind"
  ]
}
```

## 2. Update postCreateCommand to set up symlinks

After cloning the Claude config repo, add:

```bash
mkdir -p ~/.claude-data/projects && \
ln -sf ~/.claude-data/.credentials.json ~/.claude/.credentials.json && \
ln -sf ~/.claude-data/history.jsonl ~/.claude/history.jsonl && \
rm -rf ~/.claude/projects && \
ln -sf ~/.claude-data/projects ~/.claude/projects && \
ln -sf ~/.claude-data/.claude.json ~/.claude.json
```

## 3. Add to `.gitignore`

```
.claude-data/
```

This stores credentials, history, and conversation sessions in the workspace folder (persistent storage) with symlinks from `~/.claude/`.


---

# Stability Tips

- Use Ethernet over Wi-Fi
- Disable Pi Wi-Fi power saving: `sudo iwconfig wlan0 power off`
- Use short USB cables
- One Pi per VM for isolation

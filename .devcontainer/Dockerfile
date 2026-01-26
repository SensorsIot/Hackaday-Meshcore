FROM mcr.microsoft.com/devcontainers/base:debian

# Remove unused zsh/oh-my-zsh to save space
RUN rm -rf /home/*/.oh-my-zsh /root/.oh-my-zsh     && apt-get purge -y zsh || true     && apt-get autoremove -y

# Install build tools and dependencies
RUN apt-get update && apt-get install -y     build-essential     cmake     python3     python3-pip     python3-venv     udev     usbutils     minicom     screen     curl     ca-certificates     gnupg     && rm -rf /var/lib/apt/lists/*

# Install Node.js LTS (v20.x)
RUN mkdir -p /etc/apt/keyrings     && curl -fsSL https://deb.nodesource.com/gpgkey/nodesource-repo.gpg.key | gpg --dearmor -o /etc/apt/keyrings/nodesource.gpg     && echo "deb [signed-by=/etc/apt/keyrings/nodesource.gpg] https://deb.nodesource.com/node_20.x nodistro main" > /etc/apt/sources.list.d/nodesource.list     && apt-get update     && apt-get install -y nodejs     && rm -rf /var/lib/apt/lists/*

# Add udev rules for ESP32 USB adapters
RUN echo '# Espressif USB JTAG/serial debug unit' > /etc/udev/rules.d/99-esp32.rules     && echo 'SUBSYSTEMS=="usb", ATTRS{idVendor}=="303a", ATTRS{idProduct}=="1001", MODE="0666"' >> /etc/udev/rules.d/99-esp32.rules     && echo '# CP210x USB to UART Bridge' >> /etc/udev/rules.d/99-esp32.rules     && echo 'SUBSYSTEMS=="usb", ATTRS{idVendor}=="10c4", ATTRS{idProduct}=="ea60", MODE="0666"' >> /etc/udev/rules.d/99-esp32.rules     && echo '# CH340 USB to UART' >> /etc/udev/rules.d/99-esp32.rules     && echo 'SUBSYSTEMS=="usb", ATTRS{idVendor}=="1a86", ATTRS{idProduct}=="7523", MODE="0666"' >> /etc/udev/rules.d/99-esp32.rules     && echo '# FTDI USB to UART' >> /etc/udev/rules.d/99-esp32.rules     && echo 'SUBSYSTEMS=="usb", ATTRS{idVendor}=="0403", ATTRS{idProduct}=="6001", MODE="0666"' >> /etc/udev/rules.d/99-esp32.rules

# Create dev user with UID 1000 (replace vscode)
RUN userdel -r vscode 2>/dev/null || true     && useradd -m -s /bin/bash -u 1000 dev     && usermod -aG sudo,dialout dev     && echo "dev ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers

# Switch to dev user
USER dev
WORKDIR /home/dev

# Install PlatformIO Core
RUN curl -fsSL https://raw.githubusercontent.com/platformio/platformio-core-installer/master/get-platformio.py -o get-platformio.py     && python3 get-platformio.py     && rm get-platformio.py

# Install Claude Code globally
RUN sudo npm install -g @anthropic-ai/claude-code

# Set up bash aliases
RUN echo 'alias superclaude="claude --dangerously-skip-permissions"' >> ~/.bashrc     && echo 'alias superclaude="claude --dangerously-skip-permissions"' >> ~/.bash_profile

# Add PlatformIO to PATH
ENV PATH="/home/dev/.platformio/penv/bin:${PATH}"

# Set working directory
WORKDIR /workspaces/meshcore

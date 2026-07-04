FROM mcr.microsoft.com/devcontainers/base:debian

# Rename base image's vscode user to dev (keeps UID 1000, matches host dev user)
RUN usermod -l dev -d /home/dev -m vscode \
    && groupmod -n dev vscode \
    && sed -i 's/vscode/dev/g' /etc/sudoers.d/vscode || true \
    && mv /etc/sudoers.d/vscode /etc/sudoers.d/dev 2>/dev/null || true

# Strip unused zsh/oh-my-zsh
RUN rm -rf /home/*/.oh-my-zsh /root/.oh-my-zsh \
    && apt-get purge -y zsh || true \
    && apt-get autoremove -y

# Build tools, USB stack, Python
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    python3 \
    python3-pip \
    python3-venv \
    udev \
    usbutils \
    minicom \
    screen \
    curl \
    ca-certificates \
    gnupg \
    && rm -rf /var/lib/apt/lists/*

# Node.js LTS direct install (devcontainer Node feature is broken: expired yarn GPG)
RUN mkdir -p /etc/apt/keyrings \
    && curl -fsSL https://deb.nodesource.com/gpgkey/nodesource-repo.gpg.key | gpg --dearmor -o /etc/apt/keyrings/nodesource.gpg \
    && echo "deb [signed-by=/etc/apt/keyrings/nodesource.gpg] https://deb.nodesource.com/node_20.x nodistro main" > /etc/apt/sources.list.d/nodesource.list \
    && apt-get update \
    && apt-get install -y nodejs \
    && rm -rf /var/lib/apt/lists/*

# ESP32 USB-serial adapter rules
RUN echo '# Espressif USB JTAG/serial debug unit' > /etc/udev/rules.d/99-esp32.rules \
    && echo 'SUBSYSTEMS=="usb", ATTRS{idVendor}=="303a", ATTRS{idProduct}=="1001", MODE="0666"' >> /etc/udev/rules.d/99-esp32.rules \
    && echo '# CP210x USB to UART Bridge' >> /etc/udev/rules.d/99-esp32.rules \
    && echo 'SUBSYSTEMS=="usb", ATTRS{idVendor}=="10c4", ATTRS{idProduct}=="ea60", MODE="0666"' >> /etc/udev/rules.d/99-esp32.rules \
    && echo '# CH340 USB to UART' >> /etc/udev/rules.d/99-esp32.rules \
    && echo 'SUBSYSTEMS=="usb", ATTRS{idVendor}=="1a86", ATTRS{idProduct}=="7523", MODE="0666"' >> /etc/udev/rules.d/99-esp32.rules \
    && echo '# FTDI USB to UART' >> /etc/udev/rules.d/99-esp32.rules \
    && echo 'SUBSYSTEMS=="usb", ATTRS{idVendor}=="0403", ATTRS{idProduct}=="6001", MODE="0666"' >> /etc/udev/rules.d/99-esp32.rules

USER dev
WORKDIR /home/dev

# PlatformIO Core (installs into ~/.platformio)
RUN curl -fsSL https://raw.githubusercontent.com/platformio/platformio-core-installer/master/get-platformio.py -o get-platformio.py \
    && python3 get-platformio.py \
    && rm get-platformio.py

ENV PATH="/home/dev/.platformio/penv/bin:${PATH}"

WORKDIR /workspaces/Hackaday
RUN echo 'cd /workspaces/Hackaday 2>/dev/null' >> /home/dev/.bashrc

# tmux — terminal multiplexer for SSH sessions
USER root
RUN apt-get update && apt-get install -y --no-install-recommends tmux \
    && apt-get clean && rm -rf /var/lib/apt/lists/*
USER dev

# Per-user npm prefix on PATH so claude (installed into ~/.npm-global/bin by
# postCreate) and the superclaude alias resolve. MUST come before the tmux guard
# below: the guard execs tmux, so bashrc lines after it never run on SSH login.
RUN echo 'export PATH="$HOME/.npm-global/bin:$PATH"' >> /home/dev/.bashrc

# Force UTF-8 locale so tmux renders Unicode block glyphs (claude's splash logo)
# correctly. Must also come before the tmux guard.
RUN echo 'export LANG=C.UTF-8' >> /home/dev/.bashrc

# Persistent claude session: interactive SSH logins auto-attach a tmux session
# "main" running claude, so it survives disconnects (reconnect = continue).
RUN printf '%s\n' \
    '' \
    '# --- Persistent claude session (auto-attach tmux on interactive SSH) ---' \
    'if [ -n "$SSH_CONNECTION" ] && [ -z "$TMUX" ] && [ "$TERM_PROGRAM" != "vscode" ] && [ ! -f "$HOME/.no-tmux" ] && command -v tmux >/dev/null 2>&1; then' \
    '    case "$-" in' \
    '        *i*) exec tmux new-session -A -s main -c /workspaces/Hackaday "claude --dangerously-skip-permissions; exec bash" ;;' \
    '    esac' \
    'fi' \
    >> /home/dev/.bashrc

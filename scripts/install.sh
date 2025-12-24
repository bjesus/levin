#!/bin/bash
# Levin installation script

set -e

# Check if running as root
if [ "$EUID" -ne 0 ]; then
  echo "Please run as root (use sudo)"
  exit 1
fi

echo "Installing Levin..."

# Create user and group
if ! id -u Levin &>/dev/null; then
    useradd --system --no-create-home --shell /bin/false Levin
    echo "Created Levin user"
fi

# Create directories
mkdir -p /var/lib/Levin/{data,state,torrents}
mkdir -p /var/log/Levin
mkdir -p /etc/Levin
mkdir -p /var/run

# Set permissions
chown -R Levin:Levin /var/lib/Levin
chown -R Levin:Levin /var/log/Levin
chmod 755 /var/lib/Levin
chmod 755 /var/log/Levin

# Install binary
install -m 755 build/Levind /usr/local/bin/Levind
install -m 755 build/cli/Levin /usr/local/bin/Levin
echo "Installed binaries to /usr/local/bin/"

# Install config
if [ ! -f /etc/Levin/Levin.toml ]; then
    cp config/Levin.toml.example /etc/Levin/Levin.toml
    chown root:Levin /etc/Levin/Levin.toml
    chmod 640 /etc/Levin/Levin.toml
    echo "Installed default config to /etc/Levin/Levin.toml"
    echo "Please edit this file to configure Levin"
fi

# Install systemd service
install -m 644 scripts/Levin.service /etc/systemd/system/Levin.service
systemctl daemon-reload
echo "Installed systemd service"

echo ""
echo "Installation complete!"
echo ""
echo "Next steps:"
echo "  1. Edit /etc/Levin/Levin.toml"
echo "  2. Enable service: sudo systemctl enable Levin"
echo "  3. Start service: sudo systemctl start Levin"
echo "  4. Check status: sudo systemctl status Levin"
echo "  5. View logs: sudo journalctl -u Levin -f"
echo "  6. Control daemon: Levin status|pause|resume|list|stats"

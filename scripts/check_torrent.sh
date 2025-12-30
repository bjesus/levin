#!/bin/bash
# Check if a torrent file has WebSocket trackers for WebTorrent support

if [ $# -eq 0 ]; then
    echo "Usage: $0 <torrent_file>"
    echo
    echo "Checks if a torrent file has WebSocket trackers needed for WebTorrent."
    exit 1
fi

TORRENT_FILE="$1"

if [ ! -f "$TORRENT_FILE" ]; then
    echo "Error: File not found: $TORRENT_FILE"
    exit 1
fi

echo "Checking torrent: $TORRENT_FILE"
echo "============================================================"

# Extract tracker URLs using transmission-show if available
if command -v transmission-show &> /dev/null; then
    echo
    echo "Trackers found:"
    transmission-show "$TORRENT_FILE" | grep -E "^  " | while read -r line; do
        if echo "$line" | grep -q "wss://\|ws://"; then
            echo "  ✓ $line (WebSocket - WebTorrent compatible!)"
        else
            echo "    $line"
        fi
    done
    
    echo
    if transmission-show "$TORRENT_FILE" | grep -q "wss://\|ws://"; then
        echo "✓ This torrent HAS WebSocket tracker(s) - WebTorrent will work!"
    else
        echo "✗ No WebSocket trackers found - WebTorrent will NOT work"
        echo
        echo "Common WebSocket trackers you can add:"
        echo "  - wss://tracker.openwebtorrent.com"
        echo "  - wss://tracker.webtorrent.dev" 
        echo "  - wss://tracker.btorrent.xyz"
        echo
        echo "Use scripts/add_websocket_tracker.sh to add them"
    fi
    
# Fallback to ctorrent if available
elif command -v ctorrent &> /dev/null; then
    ctorrent -x "$TORRENT_FILE" 2>&1 | grep -i "tracker" | while read -r line; do
        if echo "$line" | grep -q "wss://\|ws://"; then
            echo "  ✓ $line (WebSocket)"
        else
            echo "    $line"
        fi
    done
    
# Fallback to aria2c if available  
elif command -v aria2c &> /dev/null; then
    echo
    aria2c --show-files "$TORRENT_FILE" 2>&1 | grep -E "Announce|tracker" | while read -r line; do
        if echo "$line" | grep -q "wss://\|ws://"; then
            echo "  ✓ $line (WebSocket)"
        else
            echo "    $line"
        fi
    done
    
else
    echo
    echo "Error: No torrent inspection tool found."
    echo "Please install one of: transmission-cli, ctorrent, aria2c"
    echo
    echo "  Ubuntu/Debian: sudo apt install transmission-cli"
    echo "  Fedora:        sudo dnf install transmission-cli"
    echo "  Arch:          sudo pacman -S transmission-cli"
    exit 1
fi

echo
echo "STUN Server: stun.l.google.com:19302 (Google - default, no config needed)"

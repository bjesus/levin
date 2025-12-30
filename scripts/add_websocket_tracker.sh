#!/bin/bash
# Add WebSocket tracker(s) to a torrent file for WebTorrent support

set -e

if [ $# -lt 1 ]; then
    echo "Usage: $0 <torrent_file> [websocket_tracker_url]"
    echo
    echo "Adds WebSocket tracker(s) to a torrent file for WebTorrent support."
    echo
    echo "If no tracker URL is provided, adds default WebTorrent trackers:"
    echo "  - wss://tracker.openwebtorrent.com"
    echo "  - wss://tracker.webtorrent.dev"
    echo "  - wss://tracker.btorrent.xyz"
    echo
    echo "Example:"
    echo "  $0 my-torrent.torrent"
    echo "  $0 my-torrent.torrent wss://tracker.openwebtorrent.com"
    exit 1
fi

TORRENT_FILE="$1"
TRACKER_URL="${2:-}"

if [ ! -f "$TORRENT_FILE" ]; then
    echo "Error: File not found: $TORRENT_FILE"
    exit 1
fi

# Check for mktorrent or transmission-edit
if ! command -v transmission-edit &> /dev/null && ! command -v mktorrent &> /dev/null; then
    echo "Error: No torrent editing tool found."
    echo "Please install transmission-cli or mktorrent:"
    echo
    echo "  Ubuntu/Debian: sudo apt install transmission-cli"
    echo "  Fedora:        sudo dnf install transmission-cli"
    echo "  Arch:          sudo pacman -S transmission-cli"
    exit 1
fi

# Create backup
BACKUP_FILE="${TORRENT_FILE}.backup.$(date +%s)"
cp "$TORRENT_FILE" "$BACKUP_FILE"
echo "Created backup: $BACKUP_FILE"

if [ -n "$TRACKER_URL" ]; then
    # Add single tracker
    TRACKERS=("$TRACKER_URL")
else
    # Add default WebTorrent trackers
    TRACKERS=(
        "wss://tracker.openwebtorrent.com"
        "wss://tracker.webtorrent.dev"
        "wss://tracker.btorrent.xyz"
    )
fi

echo
echo "Adding WebSocket trackers to: $TORRENT_FILE"
echo "============================================================"

for tracker in "${TRACKERS[@]}"; do
    echo "Adding: $tracker"
    if command -v transmission-edit &> /dev/null; then
        transmission-edit -a "$tracker" "$TORRENT_FILE"
    else
        # mktorrent doesn't support editing, need to use Python or another tool
        echo "Warning: mktorrent cannot edit existing torrents"
        echo "Please use transmission-edit to add trackers"
        exit 1
    fi
done

echo
echo "✓ WebSocket tracker(s) added successfully!"
echo
echo "Verify with: scripts/check_torrent.sh $TORRENT_FILE"

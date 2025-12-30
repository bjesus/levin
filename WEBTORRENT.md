# WebTorrent Support in Levin

Levin now has **full WebTorrent support** on both desktop and Android! This allows Levin to seed torrents to **both traditional BitTorrent clients and web browsers**.

## What is WebTorrent?

WebTorrent is a protocol that enables BitTorrent over WebRTC, allowing torrents to be downloaded and seeded directly in web browsers without plugins. Levin acts as a "hybrid" peer that can connect to both:
- Traditional BitTorrent clients (uTP/TCP)
- Web browsers using WebTorrent (WebRTC Data Channels)

## Platform Support

### Desktop (Linux/macOS)
✅ **Enabled by default** - Built with libtorrent master + libdatachannel

### Android
✅ **Enabled by default** - libtorrent4j v2.1.0+ includes WebTorrent support

## Requirements

For WebTorrent to work, torrents **must have WebSocket tracker(s)** in addition to regular trackers.

### Common WebSocket Trackers
```
wss://tracker.openwebtorrent.com
wss://tracker.webtorrent.dev
wss://tracker.btorrent.xyz
```

### Checking if a Torrent Has WebSocket Trackers

```bash
# Using the provided script
./scripts/check_torrent.sh path/to/file.torrent

# Or manually with transmission-show
transmission-show file.torrent | grep wss://
```

### Adding WebSocket Trackers to Existing Torrents

```bash
# Add default WebTorrent trackers
./scripts/add_websocket_tracker.sh path/to/file.torrent

# Add specific tracker
./scripts/add_websocket_tracker.sh path/to/file.torrent wss://tracker.openwebtorrent.com
```

Or manually with transmission-edit:
```bash
transmission-edit -a wss://tracker.openwebtorrent.com file.torrent
```

## Configuration

### STUN Server (for NAT traversal)

Levin uses **Google's public STUN server by default**: `stun.l.google.com:19302`

This is configured automatically and requires no setup. To use a different STUN server:

```toml
# ~/.config/levin/levin.toml

[webtorrent]
stun_server = "stun.l.google.com:19302"
```

Alternative STUN servers:
- `stun:stun1.l.google.com:19302` (Google backup)
- `stun:stun.stunprotocol.org:3478` (STUN Protocol)
- `stun:stun.ekiga.net:3478` (Ekiga)

### No Additional Configuration Needed

WebTorrent works automatically when:
1. Libtorrent is built with WebTorrent support (done by default)
2. Torrents have WebSocket trackers

## How It Works

1. **Tracker Connection**: Levin connects to WebSocket trackers (wss://)
2. **WebRTC Signaling**: Exchange connection info via tracker
3. **ICE/STUN**: Use STUN server for NAT traversal
4. **Data Transfer**: Direct peer-to-peer via WebRTC Data Channels

```
┌─────────────┐
│   Browser   │ ←─────────────────┐
│  (WebRTC)   │                   │
└─────────────┘                   │
                                  │ WebRTC Data
┌─────────────┐              ┌────▼────┐
│  Levin      │ ←───────────→│  Levin  │
│  (Hybrid)   │   BitTorrent  │ (Hybrid)│
└─────────────┘              └────┬────┘
       │                          │
       │                          │ BitTorrent
       ▼                          ▼
┌─────────────┐              ┌────────────┐
│  qBittorrent│              │ Transmission│
│   (TCP/uTP) │              │  (TCP/uTP)  │
└─────────────┘              └─────────────┘
```

## Testing WebTorrent

1. **Find a WebTorrent-compatible torrent**:
   - Sintel test torrent: https://webtorrent.io/torrents/sintel.torrent
   - Has `wss://tracker.openwebtorrent.com` tracker

2. **Add to Levin**:
   ```bash
   cp sintel.torrent ~/.config/levin/torrents/
   ```

3. **Test in browser**:
   - Go to https://webtorrent.io/
   - Add the same torrent magnet/file
   - You should see Levin as a peer!

4. **Check logs**:
   ```bash
   tail -f ~/.local/state/levin/levin.log | grep -i "webtorrent\|rtc"
   ```

## Build from Source

WebTorrent is **enabled by default** when building Levin:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

To build **without** WebTorrent (uses system libtorrent):
```bash
cmake -B build -DUSE_SYSTEM_LIBTORRENT=ON
cmake --build build
```

## Troubleshooting

### "No WebSocket trackers found"
Your torrent doesn't have WebSocket trackers. Add them with:
```bash
./scripts/add_websocket_tracker.sh your-torrent.torrent
```

### WebRTC connections failing
- Check that STUN server is reachable: `nc -zv stun.l.google.com 19302`
- Verify torrents have WebSocket trackers: `./scripts/check_torrent.sh file.torrent`
- Check firewall allows UDP (required for WebRTC)

### Can't connect to web browsers
- WebSocket tracker must be `wss://` (secure), not `ws://`
- Browser must support WebRTC (all modern browsers do)
- Try the official test site: https://webtorrent.io/

## Performance

- **WebRTC uses UDP** - more efficient than TCP for many use cases
- **NAT traversal** - Works through most firewalls without port forwarding
- **Browser compatibility** - Expand your peer pool to include web browsers
- **Overhead** - Slightly higher due to WebRTC signaling, but minimal impact

## Anna's Archive Integration

When using the Anna's Archive torrent populator, you can add WebSocket trackers to all downloaded torrents:

```bash
# After populating torrents
for torrent in ~/.config/levin/torrents/*.torrent; do
    ./scripts/add_websocket_tracker.sh "$torrent"
done
```

Or modify `src/annas_archive.cpp` to add WebSocket trackers automatically during download.

## References

- WebTorrent Protocol: https://github.com/webtorrent/webtorrent/blob/master/docs/protocol.md
- libtorrent WebTorrent PR: https://github.com/arvidn/libtorrent/pull/4123
- WebRTC Specification: https://webrtc.org/
- STUN/TURN Protocol: https://www.rfc-editor.org/rfc/rfc5389

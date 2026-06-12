# CompanionOS API Reference

## UDP Packet Protocol (PC Bridge → ESP32)
The ESP32 listens on UDP Port **4210**. Strings are standard UTF-8. Binary payloads use a specific hex prefix.

### Text Commands
| Command | Action |
|---------|--------|
| `HELLO_COMPANION` | Initial discovery beacon. |
| `SPOTIFY:{json}` | Updates playback state, current track, duration, and progress. |
| `GAMING:{json}` | Updates the current active PC game and play duration. |
| `WEATHER:{json}` | Updates temperature, condition, and forecast data. |
| `RUVIEW:{json}` | Pushes processed RuView CSI presence data to the ESP32 UI. |
| `BTN:{name}` | Virtual button press. Accepts `LEFT`, `RIGHT`, `SELECT`, `LEFT_LONG`, etc. |
| `PAGE:{index}` | Forces the ESP32 to jump to a specific page index. |
| `POMO:START` | Starts the Pomodoro timer. |
| `POMO:PAUSE` | Pauses the Pomodoro timer. |

### Binary Commands (Album Art / Game Covers)
Binary streams bypass the JSON parser and write directly to the TFT buffer.
- `ART_START:` - Instructs the ESP32 to clear the `albumArt` buffer.
- `0xFE [idx_high] [idx_low] [rgb565_data...]` - Sends 96 pixels (192 bytes) of image data for exactly one row.
- `ART_COMPLETE:` - Tells the ESP32 to immediately execute a partial redraw of the album art section.

---

## HTTP REST API (Web Remote)
The Python daemon serves a Flask application on port **5000**.

### `POST /api/button`
Simulates a physical button press on the ESP32.
```json
{ "btn": "LEFT_LONG" }
```

### `POST /api/page`
Jumps to a specific UI page.
```json
{ "page": 2 }
```

### `GET /api/ruview/state`
Returns the current CSI occupancy threshold data for the presence map.
```json
{
  "enabled": true,
  "zones": [
    {
      "node_id": 1,
      "label": "Desk",
      "occupied": true,
      "variance": 0.45
    }
  ]
}
```

### `POST /api/drhack`
Triggers specific hardware attacks or diagnostic tools in the Dr.Hack suite.
```json
{ "action": "DEAUTH_START" }
```

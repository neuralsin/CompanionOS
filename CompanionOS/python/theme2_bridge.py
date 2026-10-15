"""
Theme 2 Data Bridge — sends extended Spotify payloads (T2SPOT:)
for the alternate Spotify UI on the ESP device.
Reuses the spotify_service instance and API keys from companion_controller.py.
"""

import json
import time


def theme2_spotify_feed(spotify_service, send_udp_fn, config):
    """Background thread: sends T2SPOT with volume, shuffle, repeat, device.
    
    Args:
        spotify_service: The SpotifyIntegration instance (already authenticated)
        send_udp_fn: The send_udp() function from companion_controller.py
        config: The loaded config dict
    """
    interval = config.get('update_intervals', {}).get('spotify_poll_seconds', 1)
    time.sleep(3)  # Let main loop stabilize first
    
    last_payload = None
    
    while True:
        try:
            if not spotify_service.enabled or not spotify_service.client:
                time.sleep(interval * 5)
                continue
            
            state = spotify_service.client.current_playback()
            if state:
                device = state.get('device', {})
                payload = {
                    'vol': device.get('volume_percent', 0),
                    'shuf': state.get('shuffle_state', False),
                    'rep': {'off': 0, 'context': 1, 'track': 2}.get(
                        state.get('repeat_state', 'off'), 0
                    ),
                    'dev': (device.get('name', '?'))[:23]
                }
                
                # Only send if changed to reduce UDP spam
                payload_str = json.dumps(payload)
                if payload_str != last_payload:
                    send_udp_fn(f"T2SPOT:{payload_str}")
                    last_payload = payload_str
                    
        except Exception as e:
            print(f"T2 Bridge error: {e}")
        
        time.sleep(interval * 2)  # Poll at half the Spotify rate (sufficient for vol/shuffle)

import re
import os

# --- PATCH TOUCH.H ---
touch_path = r'CompanionOS\arduino\CompanionOS_Main\touch.h'
with open(touch_path, 'r', encoding='utf-8') as f:
    touch_src = f.read()

new_touch_logic = """// Swipe gesture tracking
bool isTouching = false;
int touchStartX = 0;
int touchStartY = 0;
int lastTouchX = 0;
int lastTouchY = 0;
unsigned long lastTouchTime = 0;
unsigned long lastRealContactTime = 0;

void handleTouch() {
  checkPhysicalSensors();
  
  if (ts.touched()) {
    TS_Point p = ts.getPoint();
    int x = map(p.x, 200, 3800, SCREEN_W, 0); // Inverted X Axis
    int y = map(p.y, 200, 3800, SCREEN_H, 0); // Inverted Y Axis
    
    if (x >= 0 && x <= SCREEN_W && y >= 0 && y <= SCREEN_H) {
      lastTouchX = x;
      lastTouchY = y;
      lastRealContactTime = millis();
      
      if (!isTouching) {
        isTouching = true;
        touchStartX = x;
        touchStartY = y;
      }
    }
  } else {
    // Touch released
    if (isTouching && (millis() - lastRealContactTime > 50)) {
      isTouching = false;
      int deltaX = lastTouchX - touchStartX;
      int deltaY = lastTouchY - touchStartY;
      
      // If movement is large, it's a swipe
      if (abs(deltaX) > 50 && abs(deltaX) > abs(deltaY)) {
        if (deltaX > 0) changePage(-1); // Swipe Right
        else changePage(1); // Swipe Left
      } 
      // Otherwise, it's a TAP
      else if (abs(deltaX) < 15 && abs(deltaY) < 15) {
        if (millis() - lastTouchTime > 200) {
          
          if (currentState == STATE_SPOTIFY) {
            // Row 1: Playback Controls (CY=85)
            if (lastTouchY > 60 && lastTouchY < 110) {
                if (lastTouchX > 110 && lastTouchX < 132) sendCommand("SHUFFLE:TOGGLE");
                else if (lastTouchX >= 132 && lastTouchX < 148) sendCommand("PREV");
                else if (lastTouchX >= 148 && lastTouchX < 172) sendCommand("PLAY_PAUSE");
                else if (lastTouchX >= 172 && lastTouchX < 188) sendCommand("NEXT");
                else if (lastTouchX >= 188 && lastTouchX < 210) sendCommand("REPEAT:TOGGLE");
            } 
            // Row 2: Heart & Progress Bar (CY=135)
            else if (lastTouchY >= 120 && lastTouchY <= 160) {
                // Heart
                if (lastTouchX > 75 && lastTouchX < 115) sendCommand("LIKE:TOGGLE");
                // Progress Bar Seek
                else if (lastTouchX >= 120 && lastTouchX <= 200) {
                    if (playDuration > 0) {
                        int seekPos = map(lastTouchX, 120, 200, 0, playDuration);
                        char seekCmd[32];
                        sprintf(seekCmd, "SEEK:%d", seekPos);
                        sendCommand(seekCmd);
                        playProgress = seekPos;
                        redrawSpotifyPartial();
                    }
                }
            }
          } 
          else if (currentState == STATE_EYES) {
              setEmotion((Emotion)((currentEmotion + 1) % EMO_COUNT));
          }
          
          lastTouchTime = millis();
        }
      }
    }
  }
}
"""

touch_pattern = re.compile(r'// Swipe gesture tracking.*?(?=#endif)', re.DOTALL)
touch_src = touch_pattern.sub(new_touch_logic, touch_src)
with open(touch_path, 'w', encoding='utf-8') as f:
    f.write(touch_src)


# --- PATCH COMPANION_CONTROLLER.PY ---
py_path = r'CompanionOS\python\companion_controller.py'
with open(py_path, 'r', encoding='utf-8') as f:
    py_src = f.read()

new_python_cmds = """            if command == "HELLO_COMPANION":
                print(f"👋 Companion Device Discovered at {active_esp_ip}!")
            else:
                print(f"← [{addr[0]}] {command}")
                if command in ["PLAY_PAUSE", "NEXT", "PREV"]:
                    spotify_service.control_playback(command)
                elif command.startswith("VOLUME:"):
                    spotify_service.control_playback(command)
                elif command.startswith("SEEK:"):
                    spotify_service.control_playback(command)
                elif command == "SHUFFLE:TOGGLE":
                    try:
                        state = spotify_service.client.current_playback()
                        if state:
                            spotify_service.client.shuffle(not state['shuffle_state'])
                            print(f"🔀 Shuffle {'ON' if not state['shuffle_state'] else 'OFF'}")
                    except Exception as e:
                        print(f"Shuffle error: {e}")
                elif command == "REPEAT:TOGGLE":
                    try:
                        state = spotify_service.client.current_playback()
                        if state:
                            states = ["off", "context", "track"]
                            idx = (states.index(state['repeat_state']) + 1) % 3
                            spotify_service.client.repeat(states[idx])
                            print(f"🔁 Repeat {states[idx].upper()}")
                    except Exception as e:
                        print(f"Repeat error: {e}")
                elif command == "LIKE:TOGGLE":
                    try:
                        track = spotify_service.get_current_track()
                        if track and track.get('id'):
                            # Check if saved
                            is_saved = spotify_service.client.current_user_saved_tracks_contains([track['id']])[0]
                            if is_saved:
                                spotify_service.client.current_user_saved_tracks_delete([track['id']])
                                print(f"💔 Unsaved {track['name']}")
                            else:
                                spotify_service.client.current_user_saved_tracks_add([track['id']])
                                print(f"❤️ Saved {track['name']}")
                    except Exception as e:
                        print(f"Like toggle error: {e}")"""

py_pattern = re.compile(r'            if command == "HELLO_COMPANION":\n.*?(?=        except Exception as e:)', re.DOTALL)
py_src = py_pattern.sub(new_python_cmds + "\n", py_src)

# We also need to fix album art size to 96!
py_src = py_src.replace("art_data = spotify_service.process_album_art(track['album_art_url'])", "art_data = spotify_service.process_album_art(track['album_art_url'], size=96)")

with open(py_path, 'w', encoding='utf-8') as f:
    f.write(py_src)

print("Applied touch.h and companion_controller.py bindings successfully!")

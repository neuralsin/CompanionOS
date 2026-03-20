import cv2
import serial
import numpy as np

# --- Configuration ---
PORT = 'COM20'         # Change to your ESP8266 port
BAUD = 2000000        # Max speed
W, H = 240, 120       # Reduced resolution for speed
VIDEO_FILE = 'video.mp4'

ser = serial.Serial(PORT, BAUD, timeout=1)
cap = cv2.VideoCapture(VIDEO_FILE)

def get_center_frame(frame, target_w, target_h):
    # Resize keeping aspect ratio, then crop center
    h, w = frame.shape[:2]
    scale = max(target_w/w, target_h/h)
    resized = cv2.resize(frame, (int(w*scale), int(h*scale)))
    
    # Center crop
    y, x = resized.shape[:2]
    startx = x//2 - target_w//2
    starty = y//2 - target_h//2
    return resized[starty:starty+target_h, startx:startx+target_w]

try:
    while cap.isOpened():
        ret, frame = cap.read()
        if not ret:
            cap.set(cv2.CAP_PROP_POS_FRAMES, 0)
            continue

        # 1. Process frame
        cropped = get_center_frame(frame, W, H)
        img = cv2.cvtColor(cropped, cv2.COLOR_BGR2RGB).astype(np.uint16)
        
        # 2. Pack to RGB565
        r = (img[:,:,0] >> 3) << 11
        g = (img[:,:,1] >> 2) << 5
        b = (img[:,:,2] >> 3)
        rgb565 = (r | g | b).byteswap().tobytes()
        
        # 3. Send Sync + Data
        ser.write(b'\xAA') 
        ser.write(rgb565)

except KeyboardInterrupt:
    pass
finally:
    cap.release()
    ser.close()
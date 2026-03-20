import os
import time
import serial
import cv2
import numpy as np
import tkinter as tk
from tkinter import filedialog
import sys

def bgr_to_rgb565(frame, width=240, height=320):
    """
    Converts a standard OpenCV BGR image to Big-Endian RGB565 format.
    """
    # 1. Resize frame to fit the TFT dimensions
    frame_resized = cv2.resize(frame, (width, height))
    
    # 2. Convert to 16-bit to prevent overflow during bit shifting
    img = frame_resized.astype(np.uint16)
    
    # 3. Extract channels and mask bits
    # Red:   Keep top 5 bits, shift to bits 11-15
    # Green: Keep top 6 bits, shift to bits 5-10
    # Blue:  Keep top 5 bits, shift to bits 0-4
    r = (img[:, :, 2] & 0xF8) << 8
    g = (img[:, :, 1] & 0xFC) << 3
    b = (img[:, :, 0] & 0xF8) >> 3
    
    # 4. Combine into single 16-bit value
    rgb565 = r | g | b
    
    # 5. Swap bytes (Little-Endian to Big-Endian) 
    # This is required because TFT_eSPI setSwapBytes(false) expects high byte first
    return rgb565.byteswap().tobytes()

def main():
    # Setup Tkinter to hide the main window but show the file dialog
    root = tk.Tk()
    root.withdraw()
    root.attributes('-topmost', True)

    folder_path = filedialog.askdirectory(title="Select Folder with Extracted Frames")

    if not folder_path:
        print("No folder selected. Exiting.")
        return

    # Get sorted list of images
    frame_files = sorted([f for f in os.listdir(folder_path) if f.lower().endswith(('.jpg', '.jpeg', '.png'))])
    
    if not frame_files:
        print("No valid frame images found in the directory.")
        return

    # Serial Configuration
    port = "COM20"  # Change this to your specific COM port
    baudrate = 921600 
    
    try:
        ser = serial.Serial(port, baudrate, timeout=1)
        
        # Reset the ESP8266 to ensure it starts fresh
        print(f"Resetting ESP8266 on {port}...")
        ser.setDTR(False)
        ser.setRTS(False)
        time.sleep(0.1)
        ser.setDTR(True)
        ser.setRTS(True)
        
        # Give the ESP8266 time to reboot and run setup()
        time.sleep(2.0) 
        ser.reset_input_buffer()
        
    except serial.SerialException as e:
        print(f"Error opening serial port: {e}")
        return

    print("Waiting for 'K' (Ready signal) from ESP8266...")
    
    # Wait for the initial 'K' sent from Arduino's setup()
    while True:
        if ser.in_waiting > 0:
            char = ser.read().decode('ascii', errors='ignore')
            if char == 'K':
                print("Received ready signal! Starting stream...")
                break
        time.sleep(0.01)

    WIDTH = 240
    HEIGHT = 320

    try:
        total_frames = len(frame_files)
        for idx, frame_file in enumerate(frame_files):
            frame_path = os.path.join(folder_path, frame_file)
            frame = cv2.imread(frame_path)
            
            if frame is None:
                continue
                
            # Process frame to RGB565 bytes
            frame_bytes = bgr_to_rgb565(frame, WIDTH, HEIGHT)
            
            # Send the entire frame data at once
            ser.write(frame_bytes)
            ser.flush()
            
            # Update console progress
            sys.stdout.write(f"\rStreaming frame {idx + 1}/{total_frames}")
            sys.stdout.flush()
            
            # Wait for 'K' from ESP8266
            # In your Arduino code, Serial.write('K') happens AFTER the Y-loop finishes
            while True:
                if ser.in_waiting > 0:
                    response = ser.read().decode('ascii', errors='ignore')
                    if response == 'K':
                        break
                    
    except KeyboardInterrupt:
        print("\nPlayback stopped by user.")
    finally:
        ser.close()
        print("\nSerial connection closed.")

if __name__ == "__main__":
    main()
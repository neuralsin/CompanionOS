import cv2
import os
import tkinter as tk
from tkinter import filedialog
import sys

def main():
    # Setup tkinter for file dialog
    root = tk.Tk()
    root.withdraw()
    root.attributes('-topmost', True) # Bring dialog to front

    # Select video
    video_path = filedialog.askopenfilename(
        title="Select Video File",
        filetypes=[("Video Files", "*.mp4 *.avi *.mkv *.mov *.wmv")]
    )

    if not video_path:
        print("No video selected. Exiting.")
        return

    # Create folder named after video file
    video_filename = os.path.basename(video_path)
    video_name, _ = os.path.splitext(video_filename)
    
    # Create the output directory in the same path as the video
    output_dir = os.path.join(os.path.dirname(video_path), video_name)
    
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)

    cap = cv2.VideoCapture(video_path)
    if not cap.isOpened():
        print("Error opening video stream or file")
        return

    total_frames = int(cap.get(cv2.CAP_PROP_FRAME_COUNT))
    print(f"Extracting {total_frames} frames to: {output_dir}")

    frame_idx = 0
    while cap.isOpened():
        ret, frame = cap.read()
        if not ret:
            break
        
        # Save frame as JPG
        frame_path = os.path.join(output_dir, f"frame_{frame_idx:05d}.jpg")
        cv2.imwrite(frame_path, frame)
        
        frame_idx += 1
        sys.stdout.write(f"\rProgress: {frame_idx}/{total_frames} frames extracted")
        sys.stdout.flush()

    cap.release()
    print("\nExtraction finished successfully!")

if __name__ == "__main__":
    main()

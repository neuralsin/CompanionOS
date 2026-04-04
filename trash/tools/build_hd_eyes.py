import os
import math
from PIL import Image, ImageDraw

SCREEN_W, SCREEN_H = 320, 240
data_dir = os.path.join(os.path.dirname(__file__), "..", "arduino", "CompanionOS_Main", "data")
os.makedirs(data_dir, exist_ok=True)

# We will generate a 100x100 eye sprite for each theme/emotion
EYE_SIZE = 100
# RGB565 conversion helper
def rgb_to_565(r, g, b):
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)

def create_eye_sprite(theme_id, emotion, base_color, accent_color, style):
    img = Image.new('RGB', (EYE_SIZE, EYE_SIZE), color=(0, 0, 0)) # Black background (transparent mask assumed by engine)
    draw = ImageDraw.Draw(img)
    
    cx, cy = EYE_SIZE//2, EYE_SIZE//2
    
    # Base eye shape
    if style == "round":
        if emotion == "HAPPY":
            draw.pieslice([10, 30, EYE_SIZE-10, EYE_SIZE+40], 180, 360, fill=base_color)
        elif emotion == "SAD":
            draw.pieslice([10, -40, EYE_SIZE-10, EYE_SIZE-30], 0, 180, fill=base_color)
        elif emotion == "ANGRY":
            draw.pieslice([10, 10, EYE_SIZE-10, EYE_SIZE-10], 0, 360, fill=base_color)
            draw.polygon([(0, 0), (EYE_SIZE, 0), (EYE_SIZE, 40), (0, 70)], fill=(0,0,0)) # Angry brow mask
        elif emotion == "HORNY":
            draw.pieslice([10, 10, EYE_SIZE-10, EYE_SIZE-10], 0, 360, fill=base_color)
            draw.polygon([(cx, cy+20), (cx-30, cy-20), (cx-10, cy-40), (cx+10, cy-40), (cx+30, cy-20)], fill=(255, 105, 180)) # Heart
        else: # NEUTRAL/DEFAULT
            draw.ellipse([20, 20, EYE_SIZE-20, EYE_SIZE-20], fill=base_color)
            
    elif style == "square":
        # Boxy robot style
        draw.rectangle([20, 30, EYE_SIZE-20, EYE_SIZE-30], fill=base_color)
        if emotion == "HAPPY":
            draw.rectangle([20, 30, EYE_SIZE-20, cy], fill=(0,0,0))
    else:
        # Default fallback
        draw.ellipse([30, 10, EYE_SIZE-30, EYE_SIZE-10], fill=base_color)

    # Convert to RGB565 and save as .bin
    pixels = []
    for y in range(EYE_SIZE):
        for x in range(EYE_SIZE):
            r, g, b = img.getpixel((x, y))
            c565 = rgb_to_565(r, g, b)
            pixels.append((c565 >> 8) & 0xFF)
            pixels.append(c565 & 0xFF)
            
    filename = f"t{theme_id}_{emotion.lower()}.bin"
    with open(os.path.join(data_dir, filename), 'wb') as f:
        f.write(bytes(pixels))

print("Generating HD eye binary sprites for LittleFS...")

themes = [
    (0, (255, 255, 0), (255, 0, 0), "round"),      # Pikachu
    (1, (0, 200, 255), (0, 100, 200), "round"),    # Chill
    (2, (0, 255, 0), (0, 100, 0), "square"),       # Gaming
    (3, (255, 255, 255), (100, 100, 100), "round"),# Minimal
    (4, (255, 0, 0), (100, 0, 0), "round"),        # Angry
    (5, (100, 100, 255), (50, 50, 100), "round"),  # Sleep
    (6, (255, 0, 255), (100, 0, 100), "round"),    # Mood
    (7, (0, 255, 255), (0, 100, 100), "square"),   # System
    (8, (255, 150, 0), (100, 50, 0), "round"),     # Companion
    (9, (200, 200, 200), (50, 50, 50), "round"),   # Neutral
    (10, (255, 105, 180), (255, 20, 147), "round") # Anime/Horny
]

emotions = ["HAPPY", "SAD", "EXCITED", "LOVE", "SLEEPY", "ANGRY", "SURPRISED", "HORNY", "NEUTRAL"]

for t_id, base, acc, style in themes:
    for emo in emotions:
        create_eye_sprite(t_id, emo, base, acc, style)

print(f"Generated {len(themes) * len(emotions)} eye sprites in data folder!")

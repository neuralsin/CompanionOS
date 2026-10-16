import sys
from PIL import Image, ImageDraw, ImageFont
import os

def hex_to_rgb(hx):
    hx = hx.lstrip('#')
    return tuple(int(hx[i:i+2], 16) for i in (0, 2, 4))

def convert_565_to_rgb(color565):
    # RGB565 to RGB888
    r = (color565 >> 11) & 0x1F
    g = (color565 >> 5) & 0x3F
    b = color565 & 0x1F
    return (int((r * 255) / 31), int((g * 255) / 63), int((b * 255) / 31))

COLOR_BG = (0, 0, 0)
COLOR_TEXT = (255, 255, 255)
COLOR_DIM = convert_565_to_rgb(0x8410)
COLOR_ACCENT = convert_565_to_rgb(0x6B4D)
COLOR_LYRICS_BG = convert_565_to_rgb(0x1082)

img = Image.new('RGB', (320, 240), COLOR_BG)
draw = ImageDraw.Draw(img)

# Try to use a default font, simple bitmapped appearance
try:
    font1 = ImageFont.truetype("arial.ttf", 10)
    font2 = ImageFont.truetype("arial.ttf", 14)
    font_large = ImageFont.truetype("arial.ttf", 20)
except:
    font1 = ImageFont.load_default()
    font2 = ImageFont.load_default()
    font_large = ImageFont.load_default()

# ── LEFT: Album art ──
ALBUM_X = 10
ALBUM_Y = 15
ALBUM_SIZE = 96
draw.rectangle([ALBUM_X, ALBUM_Y, ALBUM_X + ALBUM_SIZE, ALBUM_Y + ALBUM_SIZE], fill=(50, 50, 50))
draw.text((ALBUM_X + 20, ALBUM_Y + 40), "ALBUM ART", fill=(150, 150, 150), font=font1)

# Track Title & Artist
infoY = ALBUM_Y + ALBUM_SIZE + 8
title = "traitor"
artist = "Olivia Rodrigo"

draw.text((ALBUM_X, infoY), title, fill=COLOR_TEXT, font=font2)
draw.text((ALBUM_X, infoY + 18), artist, fill=COLOR_DIM, font=font1)

# Heart
heart_x = ALBUM_X + draw.textlength(artist, font=font1) + 12
heart_y = infoY + 22
draw.polygon([(heart_x-5, heart_y-3), (heart_x-3, heart_y-5), (heart_x, heart_y-2), (heart_x+3, heart_y-5), (heart_x+5, heart_y-3), (heart_x, heart_y+4)], fill=COLOR_TEXT)

# ── CENTER: Controls ──
ctrlY = ALBUM_Y + 30
ctrlX = 165

# Shuffle
draw.text((115, ctrlY - 5), "Sf", fill=COLOR_ACCENT, font=font1)
# Prev
draw.polygon([(140, ctrlY), (145, ctrlY-5), (145, ctrlY+5)], fill=COLOR_TEXT)
draw.rectangle([136, ctrlY-5, 138, ctrlY+5], fill=COLOR_TEXT)
# Play Circle
draw.ellipse([ctrlX - 18, ctrlY - 18, ctrlX + 18, ctrlY + 18], fill=COLOR_TEXT)
# Pause bars
draw.rectangle([ctrlX - 4, ctrlY - 6, ctrlX - 1, ctrlY + 6], fill=COLOR_BG)
draw.rectangle([ctrlX + 1, ctrlY - 6, ctrlX + 4, ctrlY + 6], fill=COLOR_BG)
# Next
draw.polygon([(190, ctrlY), (185, ctrlY-5), (185, ctrlY+5)], fill=COLOR_TEXT)
draw.rectangle([192, ctrlY-5, 194, ctrlY+5], fill=COLOR_TEXT)
# Repeat
draw.text((205, ctrlY - 5), "Rp", fill=COLOR_ACCENT, font=font1)

# ── PROGRESS BAR ──
barX = 118
barY = ctrlY + 30
barW = 90

draw.rectangle([barX, barY, barX + barW, barY + 2], fill=COLOR_DIM)
draw.rectangle([barX, barY, barX + 40, barY + 2], fill=COLOR_TEXT)
draw.ellipse([barX + 37, barY - 2, barX + 43, barY + 4], fill=COLOR_TEXT)

draw.text((barX, barY + 5), "1:24", fill=COLOR_ACCENT, font=font1)
draw.text((barX + barW - 20, barY + 5), "3:49", fill=COLOR_ACCENT, font=font1)

# ── RIGHT: Lyrics Card ──
cardX = 212
cardY = 10
cardW = 105
cardH = 240 - 20

draw.rounded_rectangle([cardX, cardY, cardX + cardW, cardY + cardH], radius=6, fill=COLOR_LYRICS_BG)
draw.text((cardX + 6, cardY + 6), "LYRICS", fill=COLOR_ACCENT, font=font1)

draw.text((cardX + 6, cardY + 30), "You betrayed me", fill=COLOR_TEXT, font=font2)
draw.text((cardX + 6, cardY + 50), "And I know that", fill=COLOR_TEXT, font=font2)

draw.text((cardX + 6, cardY + 80), "you'll never", fill=COLOR_DIM, font=font2)
draw.text((cardX + 6, cardY + 100), "feel sorry", fill=COLOR_DIM, font=font2)

out_path = r"C:\Users\shaan\.gemini\antigravity\brain\cdde13d3-cde4-4816-b19f-a9e22f76007f\spotify_simulation.png"
img.save(out_path)
print(f"Saved simulation to {out_path}")

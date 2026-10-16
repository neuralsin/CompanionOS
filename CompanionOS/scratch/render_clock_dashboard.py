import math
import os
from PIL import Image, ImageDraw, ImageFont

def render_dashboard():
    # 320x240 Resolution High-Fidelity Simulation
    w, h = 320, 240
    img = Image.new('RGB', (w, h), (8, 12, 20)) # CLK_BG
    draw = ImageDraw.Draw(img)

    try:
        font_xs = ImageFont.truetype("arial.ttf", 9)
        font_sm = ImageFont.truetype("arial.ttf", 11)
        font_md = ImageFont.truetype("arial.ttf", 13)
        font_bold = ImageFont.truetype("arialbd.ttf", 14)
        font_lg = ImageFont.truetype("arialbd.ttf", 22)
    except:
        font_xs = ImageFont.load_default()
        font_sm = ImageFont.load_default()
        font_md = ImageFont.load_default()
        font_bold = ImageFont.load_default()
        font_lg = ImageFont.load_default()

    # Colors
    CLK_CARD_BG = (16, 24, 40)
    CLK_BORDER = (33, 44, 68)
    CLK_CYAN = (0, 210, 255)
    CLK_PURPLE = (160, 50, 255)
    CLK_GREEN = (0, 230, 128)
    CLK_TEXT_DIM = (132, 145, 170)
    CLK_WHITE = (255, 255, 255)

    pad = 8
    col1_w = 140
    col2_w = w - col1_w - (pad * 3)

    # ═══════════════════════════════════════════════════════════
    # LEFT COLUMN TOP: Analog & Digital Clock Widget
    # ═══════════════════════════════════════════════════════════
    c1_x, c1_y = pad, pad
    c1_h = 165
    draw.rounded_rectangle([c1_x, c1_y, c1_x + col1_w, c1_y + c1_h], radius=10, fill=CLK_CARD_BG, outline=CLK_BORDER)

    # Analog Clock Center & Ring
    cx = c1_x + col1_w // 2
    cy = c1_y + 55
    r = 40

    draw.ellipse([cx - r - 2, cy - r - 2, cx + r + 2, cy + r + 2], outline=CLK_PURPLE, width=2)
    draw.ellipse([cx - r, cy - r, cx + r, cy + r], outline=CLK_CYAN, width=1)

    # Numbers
    num_r = r - 10
    draw.text((cx - 6, cy - num_r - 5), "12", fill=CLK_WHITE, font=font_sm)
    draw.text((cx - 3, cy + num_r - 4), "6", fill=CLK_WHITE, font=font_sm)
    draw.text((cx + num_r - 2, cy - 5), "3", fill=CLK_WHITE, font=font_sm)
    draw.text((cx - num_r - 5, cy - 5), "9", fill=CLK_WHITE, font=font_sm)

    # Hands (09:42:15 PM)
    h_angle = math.radians((9 % 12 + 42/60.0) * 30 - 90)
    m_angle = math.radians((42 + 15/60.0) * 6 - 90)
    s_angle = math.radians(15 * 6 - 90)

    # Hour Hand
    hx = cx + math.cos(h_angle) * (r * 0.5)
    hy = cy + math.sin(h_angle) * (r * 0.5)
    draw.line([cx, cy, hx, hy], fill=CLK_WHITE, width=3)

    # Minute Hand
    mx = cx + math.cos(m_angle) * (r * 0.75)
    my = cy + math.sin(m_angle) * (r * 0.75)
    draw.line([cx, cy, mx, my], fill=CLK_WHITE, width=2)

    # Second Hand
    sx = cx + math.cos(s_angle) * (r * 0.85)
    sy = cy + math.sin(s_angle) * (r * 0.85)
    draw.line([cx, cy, sx, sy], fill=CLK_CYAN, width=1)

    draw.ellipse([cx - 4, cy - 4, cx + 4, cy + 4], fill=CLK_PURPLE)
    draw.ellipse([cx - 1, cy - 1, cx + 1, cy + 1], fill=CLK_WHITE)

    # Digital Clock
    draw.text((c1_x + 12, c1_y + 105), "09:42", fill=CLK_CYAN, font=font_lg)
    draw.text((c1_x + 95, c1_y + 112), "PM", fill=CLK_PURPLE, font=font_sm)

    # Date String
    draw.text((c1_x + 10, c1_y + 142), "08 AUGUST 2026 | FRI", fill=CLK_TEXT_DIM, font=font_xs)

    # ═══════════════════════════════════════════════════════════
    # LEFT COLUMN BOTTOM: Wi-Fi Card (Wrapped Text)
    # ═══════════════════════════════════════════════════════════
    w_y = c1_y + c1_h + pad
    w_h = h - w_y - pad
    draw.rounded_rectangle([c1_x, w_y, c1_x + col1_w, w_y + w_h], radius=8, fill=CLK_CARD_BG, outline=CLK_BORDER)
    draw.ellipse([c1_x + 8, w_y + 14, c1_x + 22, w_y + 28], fill=CLK_CYAN)
    draw.text((c1_x + 28, w_y + 8), "Wi-Fi", fill=CLK_WHITE, font=font_sm)
    draw.text((c1_x + 28, w_y + 24), "Companion_5G", fill=CLK_CYAN, font=font_xs)

    # ═══════════════════════════════════════════════════════════
    # RIGHT COLUMN: Expanded Direct Spotify & Dynamic Lyrics Window
    # ═══════════════════════════════════════════════════════════
    c2_x, c2_y = c1_x + col1_w + pad, pad
    c2_h = h - (pad * 2)
    draw.rounded_rectangle([c2_x, c2_y, c2_x + col2_w, c2_y + c2_h], radius=10, fill=CLK_CARD_BG, outline=CLK_BORDER)

    # Header
    draw.text((c2_x + 12, c2_y + 10), "NOW PLAYING", fill=CLK_PURPLE, font=font_sm)
    for i, h_val in enumerate([6, 12, 8, 14, 9]):
        draw.rectangle([c2_x + col2_w - 28 + i*4, c2_y + 22 - h_val, c2_x + col2_w - 26 + i*4, c2_y + 22], fill=CLK_CYAN)

    # Album Art Placeholder
    art_x, art_y, art_s = c2_x + 12, c2_y + 30, 50
    draw.rounded_rectangle([art_x, art_y, art_x + art_s, art_y + art_s], radius=6, fill=(35, 45, 75))
    draw.ellipse([art_x + 10, art_y + 10, art_x + 40, art_y + 40], outline=CLK_CYAN, width=2)
    draw.ellipse([art_x + 20, art_y + 20, art_x + 30, art_y + 30], fill=CLK_WHITE)

    # Track Info (Wrapped & Truncated Cleanly)
    draw.text((art_x + art_s + 12, art_y + 2), "Night Drive (Remix)", fill=CLK_WHITE, font=font_bold)
    draw.text((art_x + art_s + 12, art_y + 18), "The Midnight", fill=CLK_TEXT_DIM, font=font_sm)

    # Quality Tag
    draw.rounded_rectangle([art_x + art_s + 12, art_y + 34, art_x + art_s + 70, art_y + 46], radius=4, fill=(25, 35, 55))
    draw.text((art_x + art_s + 18, art_y + 35), "Spotify", fill=CLK_CYAN, font=font_xs)

    # Progress Bar
    prg_y = c2_y + 92
    draw.text((c2_x + 12, prg_y - 2), "01:24", fill=CLK_TEXT_DIM, font=font_xs)
    draw.text((c2_x + col2_w - 36, prg_y - 2), "03:47", fill=CLK_TEXT_DIM, font=font_xs)

    bar_x = c2_x + 45
    bar_w = col2_w - 90
    draw.line([bar_x, prg_y + 3, bar_x + bar_w, prg_y + 3], fill=CLK_BORDER, width=3)
    draw.line([bar_x, prg_y + 3, bar_x + int(bar_w * 0.38), prg_y + 3], fill=CLK_CYAN, width=3)
    draw.ellipse([bar_x + int(bar_w * 0.38) - 3, prg_y, bar_x + int(bar_w * 0.38) + 3, prg_y + 6], fill=CLK_PURPLE)

    # Controls
    ctrl_x = c2_x + col2_w // 2
    ctrl_y = c2_y + 118
    draw.text((ctrl_x - 55, ctrl_y - 6), "x", fill=CLK_CYAN, font=font_sm)
    draw.polygon([(ctrl_x - 30, ctrl_y), (ctrl_x - 22, ctrl_y - 6), (ctrl_x - 22, ctrl_y + 6)], fill=CLK_WHITE)
    draw.rectangle([(ctrl_x - 34, ctrl_y - 6), (ctrl_x - 32, ctrl_y + 6)], fill=CLK_WHITE)

    # Play Circle Button
    draw.ellipse([ctrl_x - 12, ctrl_y - 12, ctrl_x + 12, ctrl_y + 12], fill=CLK_PURPLE, outline=CLK_CYAN, width=2)
    draw.rectangle([ctrl_x - 4, ctrl_y - 5, ctrl_x - 2, ctrl_y + 5], fill=CLK_WHITE)
    draw.rectangle([ctrl_x + 2, ctrl_y - 5, ctrl_x + 4, ctrl_y + 5], fill=CLK_WHITE)

    # Next
    draw.polygon([(ctrl_x + 30, ctrl_y), (ctrl_x + 22, ctrl_y - 6), (ctrl_x + 22, ctrl_y + 6)], fill=CLK_WHITE)
    draw.rectangle([(ctrl_x + 32, ctrl_y - 6), (ctrl_x + 34, ctrl_y + 6)], fill=CLK_WHITE)
    draw.text((ctrl_x + 45, ctrl_y - 6), "o", fill=CLK_CYAN, font=font_sm)

    # ═══════════════════════════════════════════════════════════
    # DYNAMIC LYRICS WRAPPING & SIZING
    # ═══════════════════════════════════════════════════════════
    lyr_y = c2_y + 138
    draw.line([c2_x + 8, lyr_y, c2_x + col2_w - 8, lyr_y], fill=CLK_BORDER, width=1)

    draw.text((c2_x + 12, lyr_y + 6), "LYRICS", fill=CLK_CYAN, font=font_sm)

    # Long lyrics wrapped onto 2 lines with dynamic font sizing
    draw.text((c2_x + 12, lyr_y + 22), "There's a fire in the hills tonight", fill=CLK_TEXT_DIM, font=font_xs)
    draw.text((c2_x + 12, lyr_y + 36), "Driving fast into the endless summer", fill=CLK_WHITE, font=font_sm)
    draw.text((c2_x + 12, lyr_y + 50), "night under the neon city sky...", fill=CLK_WHITE, font=font_sm)
    draw.text((c2_x + 12, lyr_y + 66), "Where shadows fade away", fill=CLK_TEXT_DIM, font=font_xs)

    out_dir = r"C:\Users\shaan\.gemini\antigravity-ide\brain\8a770566-76b7-4db3-a491-d4aa1c6c1524"
    out_path = os.path.join(out_dir, "clock_dashboard_rendered.png")
    img.save(out_path)
    print(f"Renders saved successfully to: {out_path}")

if __name__ == "__main__":
    render_dashboard()

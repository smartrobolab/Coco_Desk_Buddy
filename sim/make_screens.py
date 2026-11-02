#!/usr/bin/env python3
"""Renders Coco's faces and cards (from the real firmware code) to PNG/GIF.

    sim/build.sh && python3 sim/make_screens.py

Outputs shared/images/screens/*.png, *.gif (single frames), the face sheet
shared/images/coco_faces.png (all versions) and the Coco Touch screen sheet
coco_touch/images/coco_touch_screens.png. Needs Pillow.
"""
import os
import subprocess

from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
RENDER = os.path.join(HERE, "build", "render")
OUT = os.path.join(ROOT, "shared", "images", "screens")
SCALE = 4
ON = (40, 225, 235)   # cyan OLED pixel, like the 0.96" in the reference photo
OFF = (6, 12, 16)
FONT = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"

FACES = ["neutral", "serious", "sleepy", "crying", "sad", "cute", "kissou", "mini_love", "anime_love", "fire",
         "angry", "love", "happy", "surprised", "dizzy", "sleeping", "eating", "playing", "music", "singing", "yawn",
         "low_battery"]
CARDS = ["boot", "wifi", "wifi2", "wififail", "clock", "weather-clear", "weather-partly", "weather-cloudy", "weather-rain", "weather-thunder",
         "weather-snow", "weather-fog", "weather-night", "status", "hold", "noweather"]
STILL_MS = {"eating": 2000, "playing": 1100, "music": 300, "singing": 400, "sleeping": 2900, "yawn": 1000,
            "happy": 150, "surprised": 150, "dizzy": 300, "low_battery": 600, "anime_love": 300, "boot": 2000,
            "hold": 1400, "weather-thunder": 200}


def frames(kind, name, fps, dur):
    raw = subprocess.run([RENDER, kind, name, str(fps), str(dur)], check=True, capture_output=True).stdout
    return [raw[i:i + 1024] for i in range(0, len(raw), 1024)]


def to_image(buf, scale=SCALE):
    img = Image.new("RGB", (128 * scale, 64 * scale), OFF)
    px = ImageDraw.Draw(img)
    for page in range(8):
        for x in range(128):
            b = buf[page * 128 + x]
            for bit in range(8):
                if b >> bit & 1:
                    y = page * 8 + bit
                    px.rectangle([x * scale, y * scale, x * scale + scale - 2, y * scale + scale - 2], fill=ON)
    return img


def still(kind, name):
    ms = STILL_MS.get(name, 0)
    fs = frames(kind, name, 25, ms + 40)
    return to_image(fs[-1])


def sheet(items, cols, path, title):
    tiles = [(label, img.resize((256, 128), Image.NEAREST)) for label, img in items]
    pad, lab = 14, 22
    rows = (len(tiles) + cols - 1) // cols
    w = cols * (256 + pad) + pad
    h = rows * (128 + lab + pad) + pad + 46
    out = Image.new("RGB", (w, h), (24, 26, 30))
    d = ImageDraw.Draw(out)
    d.text((pad, 10), title, fill=(240, 240, 240), font=ImageFont.truetype(FONT, 24))
    f = ImageFont.truetype(FONT, 15)
    for i, (label, img) in enumerate(tiles):
        x = pad + (i % cols) * (256 + pad)
        y = 46 + pad + (i // cols) * (128 + lab + pad)
        d.rounded_rectangle([x - 4, y - 4, x + 260, y + 132], 6, fill=(45, 40, 30))  # brass-ish bezel
        out.paste(img, (x, y))
        d.text((x, y + 134), label, fill=(220, 200, 150), font=f)
    out.save(path)


def main():
    os.makedirs(OUT, exist_ok=True)
    face_imgs = []
    for name in FACES:
        img = still("face", name)
        img.save(os.path.join(OUT, f"face_{name}.png"))
        face_imgs.append((name.replace("_", " "), img))
    card_imgs = []
    for name in CARDS:
        img = still("card", name)
        img.save(os.path.join(OUT, f"card_{name}.png"))
        card_imgs.append((name.replace("-", " "), img))
    # a few animated GIFs
    for kind, name, dur in [("face", "neutral", 4000), ("face", "anime_love", 2000), ("face", "eating", 7000),
                            ("card", "weather-rain", 2000), ("card", "weather-clear", 4000), ("card", "boot", 2400)]:
        imgs = [to_image(b, 2) for b in frames(kind, name, 20, dur)]
        imgs[0].save(os.path.join(OUT, f"{kind}_{name}.gif"), save_all=True, append_images=imgs[1:], duration=50,
                     loop=0)
    sheet(face_imgs, 4, os.path.join(ROOT, "shared", "images", "coco_faces.png"),
          "Coco faces - all versions (rendered from the firmware)")
    os.makedirs(os.path.join(ROOT, "coco_touch", "images"), exist_ok=True)
    sheet(card_imgs, 4, os.path.join(ROOT, "coco_touch", "images", "coco_touch_screens.png"),
          "Coco Touch screens: boot, Wi-Fi setup, clock, weather, status")
    print("wrote shared/images/screens, shared/images/coco_faces.png, coco_touch/images/coco_touch_screens.png")


if __name__ == "__main__":
    main()

# COCO - a tiny brass desk buddy

Coco is a palm-sized desk companion built as a **brass wire circuit sculpture**: a 0.96" OLED face, a tiny ESP32 board and a 16340 Li-ion cell held in a cage of 1.2 mm brass wire, wired with 0.9 mm brass wire. Touch its head and it reacts.

![Coco, three versions](shared/images/coco_family_render.png)

*Coco Touch, Coco Voice C3 and Coco Voice S3, rendered with Blender from the 3D models (`tools/render_realistic.py`).*

![Coco, three versions: what is inside](shared/images/coco_overview.png)

## Pick a version

Each version has its own folder with **everything** for it: its own firmware, circuit diagram, case drawing, assembly steps, bending template, 3D models (STL) and a build guide.

| | [**Coco Touch**](coco_touch/README.md) | [**Coco Voice C3**](coco_voice_c3/README.md) | [**Coco Voice S3**](coco_voice_s3/README.md) |
|---|---|---|---|
| Does | faces, clock, weather, status, touch reactions | + talks (XiaoZhi voice AI) | + talks, own wake word |
| Wake word | - | "Hi ESP" or double-tap | **"Hey Coco"** or double-tap |
| Board | ESP32-C3 Super Mini | ESP32-C3 Super Mini | Seeed XIAO ESP32S3 (8 MB + PSRAM) |
| Extra parts | - | INMP441 mic, MAX98357A amp, 20 mm speaker | same as Voice C3 |
| Frame | 52 x 40 x 20 mm | 56 x 44 x 30 mm | 56 x 44 x 30 mm |
| Firmware | Arduino, [coco_touch/firmware](coco_touch/firmware/coco_touch) | XiaoZhi `coco/c3`, [coco_voice_c3/firmware](coco_voice_c3/firmware) | XiaoZhi `coco/s3`, [coco_voice_s3/firmware](coco_voice_s3/firmware) |
| Folder | [coco_touch/](coco_touch) | [coco_voice_c3/](coco_voice_c3) | [coco_voice_s3/](coco_voice_s3) |

All versions share: the small **TTP223** touch pad as Coco's head, a **TP4056** charger, a slide switch, and a **3.3 V LDO** (ME6211 / RT9080) powering the board's 3V3 pin. Wi-Fi is set up **from your phone**: Coco shows a QR code, you pick your network and type the password and city. No code changes needed.

## Faces (all versions)

Every face is drawn from shapes each frame, so the eyes blink, glance around and animate without bitmaps. The face engine comes from the KIBO robot project.

![Faces](shared/images/coco_faces.png)

## Repository layout

```text
Coco_Desk_Buddy/
├── coco_touch/
│   ├── firmware/coco_touch/   Arduino sketch
│   ├── images/                wiring, case, assembly, screens, bending template
│   ├── cad/                   STL models
│   └── README.md
├── coco_voice_c3/
│   ├── firmware/              XiaoZhi board coco/c3, shared/, build.sh, flash.sh
│   ├── images/  cad/  README.md
├── coco_voice_s3/
│   ├── firmware/              XiaoZhi board coco/s3 ("Hey Coco"), partitions/, shared/, build.sh, flash.sh
│   ├── images/  cad/  README.md
├── shared/            brass-tips.md (tools, soldering, LDO, safety), images/ (faces, overview, screens/)
├── sim/               PC renderer for faces/screens + parser tests
└── tools/             make_diagrams.py, make_stl.py, sync_shared.sh
```

## Regenerating the images and models

```bash
sim/build.sh && sim/build/render test && python3 sim/make_screens.py   # screens + tests
python3 tools/make_diagrams.py                                          # wiring, cases, assembly, templates
python3 tools/make_stl.py                                               # STL models
BLENDER=/path/to/blender python3 tools/render_realistic.py              # photo-style renders (Blender 4.2+)
```

The face engine and the voice code exist once per version folder, so each folder is complete on its own. Edit the master copy (face engine: `coco_touch/firmware/coco_touch/`, voice code: `coco_voice_c3/firmware/shared/`), then run `tools/sync_shared.sh` to copy it to the others (`--check` only reports differences).

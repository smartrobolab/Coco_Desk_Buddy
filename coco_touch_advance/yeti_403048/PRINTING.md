# Coco Yeti, 403048 battery (1.12x): printing guide

Everything you need to order the printed parts from a 3D printing service (JLC3DP, PCBWay, Craftcloud, a local
print shop, ...). The files are checked: each one is a single closed solid, already lying flat side down on the
print bed, and all parts fit together with 1 mm of space round every electronic part
(see [cad/verify_report.txt](cad/verify_report.txt)).

## 1. Which files to upload

Upload **one zip** (five files) for your screen size:

| Your screen | Upload |
|---|---|
| 1.3" OLED (SH1106) | [cad/print/coco_yeti_print_13.zip](cad/print/coco_yeti_print_13.zip) |
| 0.96" OLED (SSD1306) | [cad/print/coco_yeti_print_096.zip](cad/print/coco_yeti_print_096.zip) |

| Part | File (in cad/print/) | Qty | Size (mm) | Colour | Weight (PLA) |
|---|---|---|---|---|---|
| Body (the yeti) | coco_yeti_body.stl | 1 | 83.9 x 83.7 x 45.5 | **white** | ~56 g |
| Back cover | coco_yeti_back_cover.stl | 1 | 57.3 x 63.0 x 4.5 | **white** | ~13 g |
| Base | coco_yeti_base_plain.stl | 1 | 100.8 x 100.8 x 3.9 | white (or blue) | ~31 g |
| Screen ring (bezel) | coco_yeti_bezel_13.stl or _096.stl | 1 | 41.9 x 31.9 x 4.5 | **blue** | ~4 g |
| Chassis (inside, hidden) | coco_yeti_chassis_13.stl or _096.stl | 1 | 44 x 50 x 34 | any (black/grey) | ~6 g |

The finished yeti with all electronics weighs about 107 g. Do **not** scale the files: they are already the
right size (mm).

## 2. Material and process

| | Body, back cover, base | Bezel (screen ring) | Chassis |
|---|---|---|---|
| **Recommended** | **FDM, PLA or PLA+, white** | FDM, PLA, blue | FDM, PETG or PLA, any colour |
| Smoother look | SLA resin, white, "tough" or ABS-like resin | SLA resin + blue paint | FDM is fine |
| Avoid | MJF/SLS nylon (grey or black only, would need paint); brittle standard resin for the body (the screw bosses can crack) | | |

Why FDM PLA: cheapest, strong enough, comes in white and blue, takes self-tapping screws well. Matte or silk white
PLA both look good; matte hides the layer lines on the fur better. PETG for the chassis if the yeti will sit in a
warm place (sun, car).

**Colours:** body, back cover and base white; the bezel blue (for example "sky blue" or RAL 5015). The chassis is
hidden inside, any colour.

## 3. Settings to ask for

| | Body | Back cover | Base | Bezel | Chassis |
|---|---|---|---|---|---|
| Layer height | **0.12-0.16 mm** (fur detail) | 0.2 mm | 0.2 mm | 0.12 mm | 0.2 mm |
| Walls / perimeters | 4 | 3 | 4 | 3 | 3 |
| Infill | **40 %** gyroid | 20 % | **100 %** (heavy base) | 100 % | 100 % |
| Supports | **yes**, tree/organic, touching build plate only if possible (arms, horns, hair) | no | no | no | yes, under the plate (2.5 mm) |
| Orientation | as in the file (back down) | as in the file (flat inside down) | as in the file | as in the file | as in the file (screen ribs down) |

The legs are solid in the model; with 4 walls and 40 % infill they print almost solid anyway, which keeps the
weight low and the yeti steady. 100 % infill on the base makes it heavy and the yeti very stable.

## 4. Note to paste into the order

> Please print in the orientation of the STL files (already placed on the bed), do not scale (units mm).
> Body: white PLA, 0.12-0.16 mm layers, 4 walls, 40 % gyroid infill, tree supports for the arms, horns and hair.
> Back cover: white PLA, 0.2 mm, no supports. Base: white PLA, 0.2 mm, 100 % infill, no supports.
> Bezel: blue PLA, 0.12 mm, 100 % infill, no supports. Chassis: PETG or PLA any colour, 0.2 mm, 100 % infill,
> supports under the flat plate. Please remove the supports. Holes are sized for self-tapping screws: no need to
> drill or tap.

## 5. Hardware to buy (not printed)

| Item | Qty | Use |
|---|---|---|
| M2 x 8 mm self-tapping screws (pan head) | 2 | back cover to body |
| M3 x 12 mm flat-head (countersunk) screws, self-tapping or machine | 2 | base to the legs, from below |
| Double-sided foam tape, a little glue (hot glue or CA) | | battery, charger, sensor, motor |

Electronics: see [README.md](README.md) and [ASSEMBLY.md](ASSEMBLY.md).

## 6. When the parts arrive

1. Remove all supports, especially inside the body and from under the chassis plate. Clean the window edges.
2. Press the bezel into the window: it is a firm press fit (from the original yeti design). If it is too tight,
   sand the bezel's sides lightly; if loose, a drop of glue.
3. Try the two M2 screws in the back cover bosses and the two M3 screws in the soles once before assembly, so they
   cut their thread.
4. Look at the two USB-C openings in the back cover: each has a deep 15 x 9 mm recess for the plug head and a thin
   0.6 mm wall round the socket slot. Clean any support or stringing out of them, so the plug head can go all the
   way in. The TP4056 charger's port is the one for charging; the ESP32-C3's port is for programming.
5. Then follow [ASSEMBLY.md](ASSEMBLY.md).

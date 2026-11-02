# Coco Touch

The simplest Coco: a pair of animated eyes on a 0.96" OLED that reacts to touch and shows the time and weather. No speaker, no microphone.

**Frame:** 52 x 40 x 20 mm  ·  **Board:** ESP32-C3 Super Mini  ·  **Firmware:** Arduino ([firmware/coco_touch](firmware/coco_touch))  ·  **Battery life:** about a day, weeks in deep sleep

<p align="center">
  <img src="images/coco_touch_render.png" width="49%" alt="Coco Touch, front">
  <img src="images/coco_touch_render_back.png" width="49%" alt="Coco Touch, back">
</p>

*Rendered with Blender from the 3D model of the brass frame and real-size parts (front and back). Not a photo of a finished build.*

## Everything for this version

| File | What it is |
|---|---|
| [images/coco_touch_render.png](images/coco_touch_render.png), [_render_back.png](images/coco_touch_render_back.png) | photo-style renders (front, back) |
| [images/coco_touch_wiring.png](images/coco_touch_wiring.png) | circuit diagram |
| [images/coco_touch_size.png](images/coco_touch_size.png) | size diagram: outer size, every part's real size and position, bank-card scale |
| [images/coco_touch_case_dimensions.png](images/coco_touch_case_dimensions.png) | case diagram: every frame bar dimensioned + cut list |
| [images/coco_touch_case.png](images/coco_touch_case.png) | brass case: 3D, front, side, top, measurements, cut list |
| [images/coco_touch_assembly.png](images/coco_touch_assembly.png) | 6 build steps |
| [images/coco_touch_bending_template.pdf](images/coco_touch_bending_template.pdf) | 1:1 template, print at 100 % |
| [images/coco_touch_screens.png](images/coco_touch_screens.png) | boot, Wi-Fi setup, clock, weather, status screens |
| [cad/coco_touch_full.stl](cad/coco_touch_full.stl) | 3D model, frame + all parts |
| [cad/coco_touch_frame.stl](cad/coco_touch_frame.stl) | 3D model, brass frame only |
| [firmware/coco_touch/](firmware/coco_touch) | Arduino sketch (this version's firmware) |

## Parts

| Part | Size | Notes |
|---|---|---|
| ESP32-C3 Super Mini | 22.5 x 18 mm | |
| 0.96" OLED, SSD1306, I2C, 4 pins | 27 x 27 x 4.1 mm | check the pin order on your module |
| TTP223 touch module (small) | 15 x 11 mm | Coco's head |
| TP4056 charger with protection | 17 x 12 mm | swap PROG resistor to 3.3 kΩ |
| 3.7 V Li-ion 16340 | Ø16 x 34 mm | |
| 3.3 V LDO, 3-pin SMD (ME6211C33 / RT9080-33) + 2 x 1 µF | SOT-23 / SOT-89 | **not** AMS1117 |
| slide switch | 4 mm knob | |
| 1 MΩ, 330 kΩ, 100 nF | | battery sense |
| 1.2 mm brass wire | ~70 cm | frame, seats, battery rings |
| 0.9 mm brass wire | ~40 cm | wires, OLED posts |

**Why the small TTP223:** it fits on the top bars as Coco's head. The 24 x 24 mm module is wider than the space above the OLED bay. Leave its A/B jumpers open (momentary, active HIGH), so it can wake the C3 from deep sleep on GPIO0.

## Circuit

![Coco Touch wiring](images/coco_touch_wiring.png)

| ESP32-C3 pin | Goes to |
|---|---|
| GPIO0 | TTP223 I/O (HIGH = touched, wakes from deep sleep) |
| GPIO1 | battery sense: BAT_SW → 1 MΩ → GPIO1 → 330 kΩ → GND, 100 nF across 330 kΩ |
| GPIO4 / GPIO5 | OLED SDA / SCL |
| 3V3 | 3.3 V LDO output (LDO input = BAT_SW from the switch). Also feeds OLED VCC and TTP223 VCC |
| 5V | not connected |
| GND | brass frame |

Power path: battery → TP4056 (B+/B-) → OUT+ → slide switch → **BAT_SW** → LDO → 3V3.

## Size

Coco Touch is **52 x 40 x 20 mm** (wire centre lines; add 1.2 mm for the wire itself), smaller than a bank card.

![Coco Touch size diagram](images/coco_touch_size.png)

## Case diagram

Every bar of the brass frame with its position and length, and the cut list (piece, wire, length, quantity).

![Coco Touch case diagram](images/coco_touch_case_dimensions.png)

![Coco Touch case drawing](images/coco_touch_case.png)

## Building the case

![Coco Touch assembly](images/coco_touch_assembly.png)

Read [../shared/brass-tips.md](../shared/brass-tips.md) first (tools, soldering, LDO choice, safety).

1. **Cage.** Bend two 52 x 40 mm rectangles from 1.2 mm wire. Solder a 40 mm divider bar into each, 19.5 mm from the left edge. Tape the rectangles 20 mm apart and solder four 20 mm struts at the corners.
2. **Seats and battery clips.** Add two floor bars and two top bars (32.5 mm, at 6 mm and 14 mm from the front) between the divider and the right side. Wrap 1.2 mm wire around a 16 mm rod for two rings (Ø17 mm). Solder them in the left bay at 7 mm and 33 mm height, with short tabs to the frame.
3. **TP4056 + switch.** TP4056 on the floor bars, USB-C facing out of the right side. Its **B-** and **OUT-** pads go straight onto a floor bar (GND, and the mount). The switch goes on the right side bars, knob out.
4. **ESP32-C3 + LDO.** C3 against the back, USB-C facing right, GND pad onto the back bar. LDO on the back bar below it: its GND tab onto the frame, 1 µF on IN and OUT. Wire OUT+ → switch → LDO IN, then LDO OUT → C3 3V3. Check 3.3 V with the meter. Add the battery divider to GPIO1.
5. **OLED.** Four short 0.9 mm posts through its mounting holes, soldered to the front frame and the divider. Header at the top (or set `OLED_ROTATE_180 1` for header-down). GND → frame, VCC → 3V3, SCL → GPIO5, SDA → GPIO4.
6. **TTP223 + battery.** TTP223 on the top bars: GND to a bar, VCC → 3V3, I/O → GPIO0. Battery into the rings (wrap on), B+/B- with short wires. **Check polarity first.**

## Firmware

1. Arduino IDE: install the **esp32 by Espressif** board package (3.x).
2. Open [firmware/coco_touch/coco_touch.ino](firmware/coco_touch/coco_touch.ino).
3. Tools: board **ESP32C3 Dev Module**, **USB CDC On Boot: Enabled**, **Partition Scheme: Huge APP (3MB No OTA/1MB SPIFFS)**.
4. Switch OFF, plug the C3's USB-C, upload. No extra libraries are needed, and you don't have to edit any code.

### Wi-Fi setup from your phone (no code changes)

On first boot, and whenever you **hold Coco's head for 10 seconds**:

1. Coco shows a QR code. Scan it with your phone camera to join Coco's hotspot (**Coco-XXXX**).
2. The setup page opens by itself. If it doesn't, scan the second QR code or open `192.168.4.1`.
3. Pick your Wi-Fi from the list (or "Other..." to type a hidden name), type the **password** and your **city**, and tap **Save and connect**.
4. Coco restarts and connects. The settings stay saved through power-off and new uploads.

The time zone (including daylight saving) is set automatically from the city. To keep the saved password when you only change the city, leave the password empty. If Coco can't join, it says so on screen: hold its head 10 s to fix it. Double-tap leaves the setup screen without changes.

### Touch

| Touch | Coco does |
|---|---|
| tap | next face (Neutral → Serious → Sleepy → Crying → Sad → Cute → Kissou → Mini Love → Anime Love → Fire → Angry) |
| double-tap | info cards: Clock → Weather → Status (tap = next card, double-tap = back) |
| long press (0.8 s) | Love while you hold it |
| hold 5 s, let go | goodnight, deep sleep (touch to wake) |
| hold 10 s, let go | Wi-Fi setup |
| 6+ quick taps | Angry; 10+ = Crying |

When left alone it eats a cookie, plays with a ball, dances, sings, yawns, and shows the clock and weather. It shows the clock at every full hour. After 5 minutes (1 minute at night) it falls asleep, and after another 30 minutes it powers down until touched. Wi-Fi is on only a few seconds every 30 minutes.

![Coco Touch screens](images/coco_touch_screens.png)

## 3D model

[cad/coco_touch_full.stl](cad/coco_touch_full.stl) opens in any STL viewer (Windows 3D Viewer, PrusaSlicer, Cura, f3d), and GitHub shows it as an interactive 3D view. Units are mm, the front faces -Y. The brass is modelled as round wire; the thin 0.9 mm signal wires are left out.

## Troubleshooting

| Symptom | Check |
|---|---|
| Black screen, Serial says "OLED not found" | SDA/SCL swapped? OLED VCC on 3V3? Some modules are 0x3D: change `OLED_ADDR` in coco_config.h |
| Face upside down | `OLED_ROTATE_180` in coco_config.h |
| Touch does nothing | TTP223 I/O on GPIO0? Its LED should light when touched. Brass touching the pad? |
| Status says "USB power" | divider not on GPIO1, or the 100 nF is missing |
| Resets when Wi-Fi starts | wrong LDO (AMS1117/XC6206), missing capacitors, or low battery |
| Phone doesn't open the setup page | open `192.168.4.1` in the browser while joined to Coco-XXXX; turn off mobile data for a moment |
| "Can't join" after setup | wrong password, or a 5 GHz-only network (the C3 is 2.4 GHz only) |

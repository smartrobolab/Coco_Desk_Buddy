# Building Coco's brass case

The frame is **1.2 mm brass wire** and doubles as the ground (GND) bus. Signal and power lines are **0.9 mm brass wire**. All sizes are wire centre lines. Measure your own modules before you bend: OLED and TP4056 boards vary by a millimetre or two.

## Tools

- Flat-nose and round-nose pliers, flush cutters, a small file
- Soldering iron (330-360 °C), leaded solder 0.6-0.8 mm, flux pen
- Masking tape, a cork board or a piece of wood with pins as a jig
- Multimeter (continuity and volts)
- Fine sandpaper (400-800) or steel wool to clean the brass before soldering

## Wire tips

- Straighten wire by rolling it under a flat board, or pull it a little with two pliers.
- Bend at the marks on the 1:1 template ([coco_bending_template.pdf](images/coco_bending_template.pdf), print at 100 %). Make each 90° bend with flat pliers, right on the line.
- Clean, flux and tin both pieces, then hold them together and touch with the iron for 1-2 s. A good joint is shiny and concave.
- Solder near a finished joint with a wet tissue or a clip on the old joint, so it doesn't melt open.
- Before every module goes in, check the frame for shorts: no 0.9 mm wire may touch the frame unless it is a GND wire.

## Coco Touch (52 x 40 x 20 mm)

1. **Cage.** Bend two 52 x 40 mm rectangles from 1.2 mm wire, each with its joint in the middle of the bottom side. Solder a 40 mm divider bar 19.5 mm from the left edge of each. Tape the two rectangles 20 mm apart and solder four 20 mm struts at the corners.
2. **Seats and battery clips.** Solder two floor bars and two top bars (32.5 mm long, at 6 mm and 14 mm from the front) between the divider and the right side. Wrap 1.2 mm wire around a 16 mm rod for two rings (Ø17 mm). Solder them in the left bay at 7 mm and 33 mm height, with short tabs to the front and left frame.
3. **TP4056 + switch.** Lay the TP4056 on the floor bars with its USB-C facing out of the right side. Solder its **B-** and **OUT-** pads directly onto a floor bar (this is GND and the mount). Solder the slide switch to the right side bars, with the knob sticking out.
4. **ESP32-C3.** Place it against the back, USB-C facing right, and solder its GND pad to the back frame. Run 0.9 mm wire: TP4056 **OUT+ → switch → C3 5V**. Then solder the divider: BAT_SW → 1 MΩ → G1 → 330 kΩ → frame, with the 100 nF across the 330 kΩ.
5. **OLED.** Push four short 0.9 mm posts through the OLED's mounting holes and solder them to the front frame and the divider, so the glass sits just behind the front rectangle. Header at the top (or set `OLED_ROTATE_180 1` if you mount it header-down). Wires: GND → frame, VCC → 3V3, SCL → G5, SDA → G4. Bend them over the top of the OLED to the C3 behind it.
6. **TTP223 + battery.** Solder the TTP223's GND pin to a top bar, VCC → 3V3, I/O → G0. Slide the battery into the rings (wrap on). Solder B+ and B- with short 0.9 mm wires. **Check polarity with the meter before the last joint.**

### First power-on

1. Switch OFF. Plug USB-C into the C3, upload `coco_touch.ino`, and watch the Serial Monitor (115200).
2. Unplug. Switch ON: the eyes open and "COCO" appears.
3. Plug a USB-C charger into the TP4056: its red LED means charging, blue/green means full.

## Coco Voice (56 x 44 x 30 mm)

Same method, with a bigger cage and three more parts:

1. **Cage** 56 x 44 mm rectangles + 44 mm dividers, joined with 30 mm struts.
2. **Seats:** floor bars at 7 / 20 mm, two bars over the battery bay for the TTP223, and **four 0.9 mm grille bars** over the speaker (3-4 mm apart).
3. **Floor:** MAX98357A at the front, TP4056 beside it with USB-C facing the back. GND pads to the frame. The switch goes on the left wall behind the battery.
4. **Back + side:** ESP32-C3 on the back bars (USB-C right). INMP441 on the right wall with its **sound hole facing out**: the hole is on the bottom side of the PCB, opposite the mic can. L/R → GND.
5. **OLED + speaker:** the OLED as in Coco Touch. Glue the speaker under the grille, facing up (a ring of 0.9 mm wire or a dab of hot glue), and wire it to amp OUT+/OUT-. Amp VIN → BAT_SW, SD → G20, DIN → G10, BCLK → G6, LRC → G7. Mic SCK → G6, WS → G7, SD → G3.
6. **TTP223** on top of the battery bay, then the battery.

Keep the I2S wires (G6, G7, G3, G10) short and spaced, and away from the speaker wires.

## If something's wrong

| Symptom | Check |
|---|---|
| Black screen, Serial says "OLED not found" | SDA/SCL swapped? OLED VCC on 3V3? Some modules are 0x3D: change `OLED_ADDR` |
| Face upside down | `OLED_ROTATE_180` (Touch) or `DISPLAY_MIRROR_X/Y` (Voice) |
| Touch does nothing | TTP223 I/O on G0? LED on the TTP223 should light when touched. Brass touching the pad? |
| Battery % stuck at "USB power" | the divider isn't connected to G1, or the 100 nF is missing |
| Resets when Wi-Fi starts | battery low or a thin/long power wire: keep BAT_SW and GND short; add a 100-470 µF capacitor across 5V-GND |
| Voice: no sound | amp VIN on BAT_SW? SD (G20) must be HIGH; speaker wires on OUT+/OUT- |
| Voice: it hears itself | turn the speaker away from the mic hole; lower the volume ("volume 50") |

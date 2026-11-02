# Brass tips (all Coco versions)

The frame is **1.2 mm brass wire** and doubles as the ground (GND) bus. Signal and power lines are **0.9 mm brass wire**. All sizes in the drawings are wire centre lines. Measure your own modules before bending: OLED and TP4056 boards vary by a millimetre or two.

## Tools

- Flat-nose and round-nose pliers, flush cutters, a small file
- Soldering iron (330-360 °C), leaded solder 0.6-0.8 mm, flux pen
- Masking tape and a cork board or piece of wood with pins as a jig
- Multimeter (continuity and volts)
- Fine sandpaper (400-800) or steel wool to clean the brass

## Bending and soldering

- Straighten wire by rolling it under a flat board, or pull it a little between two pliers.
- Print your version's **bending template** at 100 % (check the 50 mm ruler) and bend on top of it. Make each 90° bend with flat pliers, right on the red dot.
- Start each rectangle in the middle of its bottom side, so the joint is hidden.
- Clean, flux and tin both pieces, then hold them together and touch with the iron for 1-2 s. A good joint is shiny and concave.
- When soldering near a finished joint, clip a wet tissue or a crocodile clip on the old joint so it doesn't melt open.
- Before every module goes in, check for shorts: no 0.9 mm wire may touch the frame unless it is a GND wire. Keep ≥ 2 mm air gap, and put heat-shrink or Kapton tape where wires cross.

## The 3.3 V regulator (LDO)

Every version powers the board's **3V3 pin** from an external 3-pin SMD LDO, fed from the switched battery (BAT_SW). The board's 5V pin stays unconnected.

| Good choices (≥ 500 mA, low dropout) | Don't use |
|---|---|
| **ME6211C33** (SOT-23-5 or SOT-89-3), **RT9080-33**, AP2112K-3.3 | **AMS1117-3.3**: needs ~4.4 V in, doesn't work from a Li-ion cell |
| | XC6206 / HT7333: only 200-250 mA, Wi-Fi peaks are ~350 mA |

- Put a **1 µF (or larger) ceramic capacitor** from IN to GND and from OUT to GND, right at the regulator. A 10 µF on OUT helps with Wi-Fi bursts.
- Check the pinout in your part's datasheet: SOT-89 parts differ (GND-IN-OUT or IN-GND-OUT).
- Solder the regulator's GND tab straight onto a frame bar: the frame doubles as its heat sink.
- Check the output with the meter (3.25-3.35 V) **before** connecting it to the board.

## Safety

- **Switch OFF before plugging in the board's own USB-C** to upload code. The board's USB regulator also drives 3V3, and it must not fight the LDO.
- The TP4056 charges at **1 A** by default, which is too much for a ~700 mAh 16340. Replace its PROG resistor (R3, 1.2 kΩ) with **3.3 kΩ** (~350 mA).
- Use the TP4056 **with protection** (DW01 + 8205A chips, OUT+/OUT- pads) and a rechargeable Li-ion 16340, not a CR123A primary cell.
- Keep the battery's plastic wrap intact: the can is the negative terminal, and the frame is bare metal.
- Check battery polarity with the meter before the last joint.

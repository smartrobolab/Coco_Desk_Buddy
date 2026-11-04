# Coco Yeti, 403048 battery (scaled 1.12x): assembly, part by part

Every step shows the new part in colour, sliding in along the red arrow; parts already fitted are grey. Pictures come from the real STL files (`tools/render_steps.py`). Printing: [PRINTING.md](PRINTING.md).

Wiring: [images/coco_yeti_wiring.png](images/coco_yeti_wiring.png). Legs and base: [images/coco_yeti_bottom.png](images/coco_yeti_bottom.png).

## Step 1. Print and get the parts ready

![Step 1](images/assembly/step_01.png)

Printed (see PRINTING.md): body, bezel (1.3" or 0.96"), chassis (1.3" or 0.96"), back cover, base. Remove the supports and test that the bezel presses into the window. Electronics: ESP32-C3 Super Mini, OLED 1.3" SH1106 or 0.96" SSD1306 (I2C, no pin header), TTP223, MPU6050 GY-521, the black 21 x 23 mm vibration motor module, LiPo 403048 (4.0 x 30 x 48 mm), TP4056 mini USB-C charger, slide switch SS12D00, ME6211 3.3 V LDO + 2 x 1 uF, 1 M + 330 k + 100 nF. Screws: 2 x M2 x 8 self-tapping (back cover), 2 x M3 x 12 flat-head (base). Thin wire, double-sided tape, a little glue.

## Step 2. Bezel into the face

![Step 2](images/assembly/step_02.png)

Press the bezel into the window from the front until it sits flush (a firm press fit, as in the original yeti). Use the bezel for your screen size: its window matches the screen.

## Step 3. Touch sensor into the head

![Step 3](images/assembly/step_03.png)

Solder three wires (VCC, GND, I/O) to the TTP223. Lay the yeti face down and slide the sensor into the slot inside the head, pad side towards the top of the head, against the 2 mm wall under the hair. A drop of glue. Leave its A/B jumpers open.

## Step 4. Screen into the face

![Step 4](images/assembly/step_04.png)

Solder four thin wires to the OLED pins. Still face down, drop the OLED in, glass towards the bezel, pins at the head end. It has 1 mm all round; the chassis presses it against the bezel in step 6.

## Step 5. Motor and motion sensor on the chassis

![Step 5](images/assembly/step_05.png)

Glue the 21 x 23 mm vibration module into the rim on the back of the chassis plate (motor side out). Glue the MPU6050 flat onto the fin: board against the fin, chip facing away from it, pins down. Solder their wires first (motor: VCC, GND, IN; MPU6050: VCC, GND, SDA, SCL, INT).

## Step 6. Chassis into the body

![Step 6](images/assembly/step_06.png)

Slide the chassis in from the back until its plate rests on the back of the screen; the four legs point to the back cover. Lead the screen and touch wires through the notch at the head end of the plate.

## Step 7. Battery, charger and switch on the back cover

![Step 7](images/assembly/step_07.png)

Lay the back cover inside up. Stick the 403048 LiPo flat on it with double-sided tape. The TP4056 charger stands on its long edge right of the battery, near the head end: USB-C end down in its own slot in the back cover, so you charge from outside; a drop of glue. The slide switch goes in the same strip, knob through its slot. Solder: battery + / - to TP4056 B+ / B-, OUT+ to the switch, switch to the LDO input.

## Step 8. ESP32-C3 and wiring

![Step 8](images/assembly/step_08.png)

The ESP32-C3 stands on its long edge against the left side of the back: USB-C end down into the back cover's slot (on the left), the other end in the chassis cup. Solder all wires now (see the wiring diagram): OLED and MPU6050 SDA/SCL to GPIO4/GPIO5, touch to GPIO0, MPU6050 INT to GPIO3, vibration IN to GPIO10, battery sense (1 M / 330 k) to GPIO1, LDO 3.3 V to 3V3 and to the VCC of the OLED, MPU6050 and touch. Leave the ESP32's 5V pin unconnected (the charger has its own USB-C).  The vibration module's VCC goes to the battery AFTER the switch (it is a 5 V motor).

## Step 9. Close the back

![Step 9](images/assembly/step_09.png)

Fold the cover with the battery onto the back: the ESP32-C3's USB-C goes into its slot, the chassis legs rest on the cover. Push the wires in so nothing is pinched. Two M2 x 8 self-tapping screws into the bosses; do not overtighten.

## Step 10. Screw it onto the base

![Step 10](images/assembly/step_10.png)

Stand the yeti on the base, each foot over a countersunk hole. From underneath, drive one M3 x 12 flat-head screw through each hole into the pilot hole in the sole (the legs are solid). Snug, not tight. Now yeti and base are one piece and it cannot tip over.

## Step 11. Flash and set up

![Step 11](images/assembly/step_11.png)

Charge: plug a USB-C cable into the charger's port on the back (the TP4056's red LED lights while charging, blue when full). Program: switch on and plug USB-C into the ESP32-C3's port. Flash coco_yeti_c3_13-merged.bin (1.3") or coco_yeti_c3_096 (0.96") from coco_touch_advance/yeti/firmware/release at 0x0. Set up Wi-Fi from your phone (QR code), then Menu > Settings > Motion setup: stand the yeti upright and tap its head, lay it on its back and tap again. Done.

## Legs and base

![Legs and base](images/coco_yeti_bottom.png)

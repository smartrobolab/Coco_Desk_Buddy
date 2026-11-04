# Coco Yeti, 403035 battery: assembly, part by part

Every step shows the new part in colour, sliding in along the red arrow; parts already fitted are grey. Pictures come from the real STL files (`tools/render_steps.py`).

Wiring: [images/coco_yeti_wiring.png](images/coco_yeti_wiring.png). The bottom of the yeti (feet holes, square pocket): [images/coco_yeti_bottom.png](images/coco_yeti_bottom.png).

## Step 1. Print and get the parts ready

![Step 1](images/assembly/step_01.png)

Print: body (white, back down, supports for arms and horns), bezel (blue, front down), chassis (screen ribs down), back cover (white, flat inside down), plain base (flat). Remove the supports and test that the bezel presses into the window. You also need: ESP32-C3 Super Mini, OLED 1.3" or 0.96" (I2C), TTP223, MPU6050 GY-521, the black 21 x 23 mm vibration motor module, LiPo 403035 (4.0 x 30 x 35 mm), TP4056 mini USB-C charger, slide switch, ME6211 3.3 V LDO + 2 x 1 uF, 1 M + 330 k + 100 nF, 2 x M2 x 8 self-tapping screws, thin wire, double-sided tape, glue.

## Step 2. Bezel into the face

![Step 2](images/assembly/step_02.png)

Press the bezel into the window from the front until it sits flush (it is a firm press fit, as in the original yeti). Use the 1.3" bezel (bigger window) for the 1.3" screen, the zip's bezel for the 0.96".

## Step 3. Touch sensor into the head

![Step 3](images/assembly/step_03.png)

Solder three wires (VCC, GND, I/O) to the TTP223. Lay the yeti face down and slide the sensor into the slot inside the head, pad side towards the top of the head, against the 2 mm wall under the hair. A drop of glue. Leave its A/B jumpers open.

## Step 4. Screen into the face

![Step 4](images/assembly/step_04.png)

Solder four thin wires to the OLED pins (no pin header: it would not fit). Still face down, drop the OLED in, glass towards the bezel, pins at the head end. It sits in its pocket with 1 mm all round; the chassis presses it against the bezel in step 6.

## Step 5. Motor and motion sensor on the chassis

![Step 5](images/assembly/step_05.png)

Glue the black 21 x 23 mm vibration motor module into the rim on the back of the chassis plate (motor side out). Glue the MPU6050 flat onto the fin: board against the fin, chip facing away from it, pins down. Solder its wires (VCC, GND, SDA, SCL, INT) first.

## Step 6. Chassis into the body

![Step 6](images/assembly/step_06.png)

Slide the chassis in from the back until its plate rests on the back of the screen. The four legs point to the back cover. Lead the screen and touch wires past the plate's open side at the head end.

## Step 7. Battery, charger and switch on the back cover

![Step 7](images/assembly/step_07.png)

Lay the back cover inside up. Stick the LiPo 403035 (4.0 x 30 x 35 mm) flat on it with double-sided tape, the TP4056 charger on top of the battery, and the slide switch at the feet end with its knob through the slot. Solder: battery + / - to TP4056 B+ / B-, OUT+ to the switch, switch to the LDO input.

## Step 8. ESP32-C3

![Step 8](images/assembly/step_08.png)

The ESP32-C3 stands at the head end of the back: USB-C end down into the back cover's slot, the other end in the chassis cup. Solder all wires now (see the wiring diagram): OLED and MPU6050 SDA/SCL to GPIO4/GPIO5, touch to GPIO0, MPU6050 INT to GPIO3, vibration IN to GPIO10, battery sense (1 M / 330 k) to GPIO1, ESP32 5V to TP4056 IN+, LDO 3.3 V to 3V3. VCC of the vibration module: to the battery AFTER the switch (it is a 5 V motor); IN to GPIO10; GND to GND.

## Step 9. Close the back

![Step 9](images/assembly/step_09.png)

Fold the cover with the battery onto the back: the ESP32-C3's USB-C goes into its slot, the chassis legs rest on the cover. Push the wires in so nothing is pinched. Two M2 x 8 self-tapping screws into the bosses. Do not overtighten.

## Step 10. Onto the base

![Step 10](images/assembly/step_10.png)

Stand the yeti on the plain base, feet over the two round marks. Optional: fix it with the leg holes (8 x 3 mm magnets in the soles and under the base, or pins / screws; see coco_yeti_bottom.png).

## Step 11. Flash and set up

![Step 11](images/assembly/step_11.png)

Switch on and plug USB-C into the back. Flash coco_yeti_c3_13-merged.bin (1.3") or coco_yeti_c3_096 (0.96") at 0x0. Set up Wi-Fi from your phone (QR code), then Menu > Settings > Motion setup: stand the yeti upright and tap its head, lay it on its back and tap again. Done: pet it, shake it, play games.

## The bottom of the yeti

![Bottom](images/coco_yeti_bottom.png)

The feet holes and the square pocket come from the original yeti design. Coco does not need them; see the picture for what they are for.

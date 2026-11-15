# Reference files for the case

Official Waveshare files for the ESP32-S3-RLCD-4.2, kept here so the case can be built and reviewed
offline. Source page: <https://docs.waveshare.net/ESP32-S3-RLCD-4.2/Resources-And-Documents>
(English wiki: <https://docs.waveshare.com/ESP32-S3-RLCD-4.2>). Files copyright Waveshare; check their
terms before redistributing outside this repo.

| File | What |
|---|---|
| `v2/ESP32-S3-RLCD-4.2-20261010.stp` | STEP assembly, V2 board (Creo export, 856 solids, no part names) |
| `v2/ESP32-S3-RLCD-4.2-20261010.dxf`, `v2/ESP32-S3-RLCD-4.2_20261010.pdf` | outline drawing, V2 |
| `v2/ESP32-S3-RLCD-4.2_20261010.zip` | the original archive of the three files above |
| `v1/ESP32-S3-RLCD-4.2-3dFile.rar` | 3D files, V1 board (not unpacked: no `unrar` here) |
| `ESP32-S3-RLCD-4.2-schematic.pdf` | schematic, one page, version not stated |
| `img/ProductSize-V1.webp`, `img/ProductSize-V2.webp` | the dimension drawings from the wiki |

## V1 and V2

Only the thickness differs. From the STEP (V2): the distance from the **front face of the LCD to the back face of
the PCB is 4.35 mm** (V1: 3.75 mm, per the wiki drawing). Total with the acrylic plate: 14.10 (V2) / 13.50 (V1).
To tell them apart on a real board, measure with calipers from the LCD front surface to the back face of the PCB.

## Numbers used for the case (V2, mm)

STEP axes: X = short side (69.1), Y = thickness (front of the LCD at +0.75, back of the PCB at -3.60), Z = long side (92.5).
"Drawing" = printed on the Waveshare drawing; "STEP" = measured from the bounding boxes of solids in the STEP.

| What | Value | From |
|---|---|---|
| PCB outline | 92.5 x 69.1, 1.60 thick | drawing, STEP (92.52 x 69.12) |
| LCD active area | 84.80 x 63.60 | drawing |
| LCD glass (TFT) | about 91.8 x 67.6, 1.8 thick | STEP |
| LCD front to PCB back | 4.35 | STEP, drawing |
| Acrylic plate (to be replaced) | 92.5 x 70.1, 2.75 thick, 7.00 behind the PCB | STEP, drawing |
| Mounting holes | 4 x M2.5, 85.5 x 62.1 apart; spacers 5.5 wide, 8.5 long | drawing, STEP |
| 18650 holder | 21.1 wide, 87.0 long, back of the PCB to its top 15.2 (sticks out 5.45 past the acrylic) | STEP |
| USB-C | on the short edge at +Z, centred on X, 9.6 wide x 4.2 high | STEP |
| 3 buttons | on the +X long edge, 10.0 apart, first one 36.25 from the -Z end; 3.7 x 4.6 body | STEP, drawing |
| microSD holder | 16.1 x 15.0 x 2.5, near the +Z short edge, x 11.1..27.2 | STEP |
| 2x9 header | 6.6 wide x 21.0 long, 8.6 tall from the PCB back (to 12.3 below it), near the centre of the board | STEP |

Not verified yet: which way the microSD slot opens (guessed: toward the +Z short edge, same side as USB-C, since its
body ends 1 mm from that edge), button heights, the speaker connector. Check on the real board before cutting plastic.

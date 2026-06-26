# Hardware Configuration & Wiring Diagram

This page documents the hardware wiring schematic, pin mapping, and connection guidelines for the RP2040 Embedded Game Engine. The system runs on a standard **Raspberry Pi Pico (RP2040)** micro-controller, rendering graphics on an **SSD1306 OLED display** via I2C, with input driven by **four tactile buttons**.

---

## Technical Specifications

| Peripheral | Controller Pin | Pico Pin | Description |
| :--- | :--- | :--- | :--- |
| **SSD1306 OLED SDA** | GP4 | Pin 6 | I2C Data (with internal pull-up) |
| **SSD1306 OLED SCL** | GP5 | Pin 7 | I2C Clock (with internal pull-up) |
| **Button UP** | GP2 | Pin 4 | Active-Low direction control |
| **Button DOWN** | GP3 | Pin 5 | Active-Low direction control |
| **Button LEFT** | GP1 | Pin 2 | Active-Low direction control |
| **Button RIGHT** | GP0 | Pin 1 | Active-Low direction control |
| **VCC** | 3.3V (OUT) | Pin 36 | 3.3V Power Line to OLED |
| **GND** | GND | Pin 3/8/38 | Common Ground Reference |

---

## Schematic Design

The tactile buttons are wired in an **active-low** configuration. One side of each button is tied directly to the designated GPIO pin, while the other side is tied to the common ground (GND). 

The RP2040 internal pull-ups are enabled programmatically (see `common/input/input.c`), removing the need for external hardware pull-up resistors:

```
                  +----------------------------------+
                  |       Raspberry Pi Pico          |
                  |                                  |
   [Button UP] ---+ GP2 (Pin 4)         GP4 (Pin 6)  +--- OLED SDA
 [Button DOWN] ---+ GP3 (Pin 5)         GP5 (Pin 7)  +--- OLED SCL
 [Button LEFT] ---+ GP1 (Pin 2)                      |
[Button RIGHT] ---+ GP0 (Pin 1)         3V3 (Pin 36) +--- OLED VCC
                  |                      GND (Pin 3) +--- OLED GND
                  |                                  |
                  +----------------------------------+
```

---

## Hardware Assembly Guide

1. **Powering Down**: Always disconnect the Pico from USB power before making or altering breadboard connections to prevent accidental short circuits.
2. **I2C Hookup**: Connect SDA and SCL of the OLED display to GP4 and GP5 respectively. Confirm that the display VCC is powered by the Pico's 3.3V supply (Pin 36), and NOT the 5V VBUS pin, to prevent damage to the OLED controller.
3. **Buttons Connection**: Tie one pin of all four buttons to a common GND line. Connect the remaining pin of each button to GP2, GP3, GP1, and GP0 respectively.
4. **Baud Rate Stability**: The I2C interface is configured at **40 kHz** inside `common/display/display_ssd1306.c`. This slower speed provides noise resistance and stable signal propagation over long jumper wires without requiring external pull-ups.

# Glyph-Hop 

> An ultra-fast, flicker-free arcade obstacle-dodging game crafted for the upcoming **S3 Lucid** (ESP32-S3) development board.

![Platform](https://img.shields.io/badge/Platform-ESP32--S3-blue?style=flat-square&logo=espressif)
![Display](https://img.shields.io/badge/Display-ST7735%20160x80-green?style=flat-square)
![Frame Rate](https://img.shields.io/badge/FPS-50%20Locked-orange?style=flat-square)
![Framework](https://img.shields.io/badge/Framework-Arduino%20%2F%20C%2B%2B-red?style=flat-square&logo=arduino)
![License](https://img.shields.io/badge/License-MIT-lightgrey?style=flat-square)

---

##  Overview

**Glyph-Hop** is a high-performance arcade side-scroller developed specifically for the next-generation **S3 Lucid** board. Leveraging the ESP32-S3 dual-core processor and an onboard 160×80 color TFT (ST7735) display, Glyph-Hop brings responsive retro gameplay with custom physics, procedural pipe generation, and zero-flicker delta rendering.

### 🎮 Gameplay Preview

<p align="center">
  <video src="https://github.com/Abhi-00047/Glyph-Hop/raw/main/assets/Game-preview.mp4" controls autoplay loop muted width="600" style="max-width: 100%;">
  </video>
</p>

---

##  Media & Hardware Showcase

### S3 Lucid Hardware
The upcoming **S3 Lucid** board combines the processing power of the ESP32-S3 with a built-in 160×80 Mini-TFT display and a three-button tactile control layout.

<p align="center">
  <img src="https://files.pcbcupid.com/Glyph-Hop/Glyph%20S3%20front%20view.avif" alt="Glyph S3 Front View" width="640"/>
  <br>
  <em>Figure 1: S3 Lucid Front View</em>
</p>

### Game Interface & Screens

| Game Start Interface | Game Over & High Score Card |
| :---: | :---: |
| <img src="https://files.pcbcupid.com/Glyph-Hop/Glyph-game%20interface.avif" alt="Glyph-Hop Game Interface" width="400"/> | <img src="https://files.pcbcupid.com/Glyph-Hop/Game%20over%20!.avif" alt="Game Over Screen" width="400"/> |
| *Title Screen with dual-tone logo, player preview, and prompt* | *Game Over summary with current score, high score, and "NEW!" badge* |

---

##  Key Features

- **Silky 50 FPS Physics Engine:** Hard-locked 20ms frame cycle with smooth gravity, flap impulsing, and accurate bounding-box collision detection.
- **Zero-Flicker Delta Slice Rendering:** Erases only trailing pixel columns and paints new leading edge slices across SPI instead of clearing entire frames or drawing solid rectangles.
- **Isolated HUD Architecture:** The top HUD bar (Score & High Score) remains static while score digits are selectively refreshed only when an obstacle is cleared.
- **Dynamic Arcade Graphics:**
  - 8×8 sprite with animated wings and beak.
  - Multi-toned shaded pipes with gold end-caps.
  - Alternating two-tone scrolling ground terrain.
  - Background starfield backdrop.
- **Built-in Power-On Self-Test (POST):** Executes automated firmware unit tests for physics and collision detection over the serial monitor at boot.
- **Full State Machine:** `START` ➔ `PLAYING` ⇄ `PAUSED` ➔ `GAMEOVER`.

---

##  Controls

The **S3 Lucid** board features three dedicated tactile pushbuttons wired with internal pull-ups (`INPUT_PULLUP`, active `LOW`):

| Button | Label / Color | GPIO Pin | Function |
| :--- | :--- | :---: | :--- |
| **SW5** | **Center (Blue)** | `GPIO 38` | **Start Game** / **Flap (Jump)** |
| **SW6** | **Top (Yellow)** | `GPIO 40` | **Pause** / **Resume Game** |
| **SW4** | **Bottom (Red)** | `GPIO 39` | **Restart** (after Game Over) |

---

##  Hardware Pinout & Architecture

### ST7735 Mini 160×80 SPI Display

| Signal Name | ESP32-S3 GPIO | Description |
| :--- | :---: | :--- |
| **TFT_CS** | `21` | SPI Chip Select |
| **TFT_DC** | `12` | Data / Command Control |
| **TFT_RST** | `42` | Hardware Reset |
| **TFT_MOSI** | `36` | Master Out Slave In (Data) |
| **TFT_SCLK** | `35` | SPI Serial Clock |
| **TFT_BLK** | `41` | Display Backlight (PWM / AnalogWrite) |

---

##  Getting Started

### 1. Prerequisites & Dependencies

Make sure you have the **Arduino IDE** (2.x recommended) or **PlatformIO** installed with the ESP32 board support package.

Install the following libraries via the Arduino Library Manager (`Ctrl + Shift + I`):
1. **Adafruit GFX Library** (`>= 1.11.x`)
2. **Adafruit ST7735 and ST7789 Library** (`>= 1.10.x`)
3. **SPI** (Included with ESP32 Arduino Core)

### 2. Board Configuration (Arduino IDE)

Configure your target settings under **Tools**:
- **Board:** `ESP32S3 Dev Module` *(or the dedicated Lucid S3 board profile once officially published)*
- **USB CDC On Boot:** `Enabled` (for Serial debug output)
- **CPU Frequency:** `240MHz (WiFi)`
- **Flash Mode:** `QIO 80MHz`
- **Flash Size:** `4MB` (or larger)
- **Partition Scheme:** `Default 4MB with SPIFFS` (or `Huge APP`)
- **Upload Speed:** `921600`

### 3. Flashing the Firmware

1. Clone or download this repository:
   ```bash
   git clone https://github.com/<your-username>/Glyph-Hop.git
   ```
2. Open `Glyph-Hop/Glyph-Hop.ino` in Arduino IDE.
3. Connect your **S3 Lucid** board to your computer via USB-C.
4. Select the detected COM port under **Tools > Port**.
5. Click **Upload** (`Ctrl + U`).
6. Open the Serial Monitor at `115200 baud` to view the boot-up self-test results!

---

## 🔬 Technical Deep-Dive: Delta Rendering

SPI TFT displays on microcontrollers often suffer from visible screen tearing or flicker when clearing frames (`tft.fillScreen(...)`) or updating large sprite bounding boxes.

Glyph-Hop solves this using single-pixel column slice rendering:
```cpp
// Erase only trailing edge as pipe moves left by PIPE_SPEED
for (int sx = newX + PIPE_WIDTH + 1; sx <= oldX + PIPE_WIDTH; sx++) {
  clearPlayfieldSlice(sx);
}
// Paint only newly revealed leading edge slices
for (int sx = newX - 1; sx <= oldX - 1; sx++) {
  drawPipeSlice(sx, sx - newX, gapY);
}
```
This reduces SPI transaction traffic per frame by over **85%**, maintaining a stable 50 FPS without requiring an external double-buffer framebuffer in PSRAM.

---

##  Repository Structure

```text
Glyph-Hop/
├── assets/
│   └── Game-preview.mp4   # 2-second in-game preview video
├── Glyph-Hop.ino          # Main game sketch, physics loop, display driver, and POST
└── README.md              # Project documentation, pinout guide, and visual showcase
```

---

## 📄 License

This project is open-source and released under the [MIT License](LICENSE). Feel free to modify, extend, and share!

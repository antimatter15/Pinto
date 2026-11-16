# Pinto

Pinto is a tiny Bluetooth camera. It pairs a **Himax HM01B0** ultra-low-power image sensor with a **Nordic nRF52840**. The nRF52840 captures a frame, compresses it to JPEG on-chip and sends it over BLE to a phone or a web page.

The project began in spring 2025 on a Seeed **XIAO nRF52840 Sense** with jumper wires. It then went to a XIAO plug-in adapter PCB, and finally to **Pinto2**, a custom 30 mm round board built around a bare nRF52840.

This repo collects every firmware experiment, the host tools, the iOS companion app, the PCB fabrication files, the CAD and the datasheets. Every experiment folder name starts with the date it was made (`YYYY-MM-DD_description`).

---

## Repository layout

```
firmware/     Arduino sketches, one folder per experiment (folder name == sketch name)
ios/          "nRF52 Cam" SwiftUI companion app (BLE + Shortcuts + OpenAI image restyle)
hardware/     PCB fabrication outputs (Gerbers, PADS export, netlist, BOM, DXF, JLC previews)
mechanical/   Enclosure / mount STLs, sensor sketch DXF, HM01B0 module STEP, SWD pogo jig
datasheets/   Component datasheets, grouped by part type
tools/        Desktop tools: JPEG/debayer experiments, bootloader flashing scripts
reference/    Third-party reference code that informed the design
captures/     Images captured by the camera
photos/       Photos of the hardware
```

### firmware/

All sketches build with the Arduino IDE and the **Seeed nRF52 (Adafruit Bluefruit) core**, board "Seeed XIAO nRF52840 Sense". When you rename a folder, rename the `.ino` file inside it to match.

| Folder | What it is |
|---|---|
| `2025-05-25_bitbang_160x120` | First light. Sets up the HM01B0 over I²C (address `0x24`) and bit-bangs the 1-bit serial output with `digitalRead` on PCLK. QQVGA 160×120. |
| `2025-05-26_mclk_oscillator` | Generates the sensor's master clock (MCLK) from the nRF using TIMER → PPI → GPIOTE toggle. Also uses GPIOTE+PPI to make an inverted VSYNC for SPI chip-select. |
| `2025-05-26_spis_test` | Bare SPIS (SPI slave) DMA test. |
| `2025-05-26_spis_capture` | **The core trick:** the sensor's serial pixel stream is read by the nRF's **SPI slave** peripheral (PCLK → SCK, D0 → MOSI, inverted VSYNC → CS), so a whole frame lands in RAM by DMA with no CPU involvement. |
| `2025-05-26_spis_capture_rewire` | Same idea with the wiring rearranged. Full-resolution 324×200 test. |
| `2025-05-27_spis_capture` | Cleanup and tuning (323×200). |
| `2025-05-28_twim_spis` | Swaps Arduino `Wire` for direct TWIM register access. Moves CS to HSYNC. |
| `2025-05-28_invsync` | Captures a full frame with VSYNC-gated CS. A TIMER2 interrupt advances `SPIS.RXD.PTR` to get past the 8-bit SPIS DMA length limit. |
| `2025-05-28_arraylist` / `2025-05-28_invsync_arraylist` | Uses the SPIS EasyDMA *array list* feature to chain row buffers. |
| `2025-05-28_6bit_packing` | **Line-by-line capture:** HSYNC drives CS, one row arrives per SPIS transaction, and the IRQ fixes the 2–3 bit misalignment (`find_line_alignment`). Each row is quantized to 6 bpp and packed 4 pixels → 3 bytes, so a 328×328 frame fits in RAM (~80 KB). |
| `2025-05-29_jpeg_encoder` | Standalone test of `tiny_jpeg_packed.h` on a stored frame (`data.h`). See `README_HUFFMAN_OPTIMIZATION.md`: Huffman tables are precomputed offline by `generate_huffman_tables.py`. |
| `2025-05-30_ble_jpeg` | Capture → JPEG → stream over BLE UART. `index.html` is a Web Bluetooth receiver. |
| `2025-05-30_web_bluetooth` | Improved BLE streaming and Web Bluetooth controller page (`index.html`; `old_index.html` is the previous version). |
| `2025-05-30_ble_throughput` | BLE UART throughput test with a matching web receiver. |
| `2025-06-03_bleuart_example` | Stock Adafruit `bleuart` example, low TX power. Used as a reference. |
| `2025-06-05_xiao_adapter_pcb` | Firmware for the **HM01B0-XIAO adapter PCB** (new pin map, hardware MCLK oscillator and hardware-inverted HSYNC, camera power switch on D6). |
| `2025-06-06_xiao_adapter_basic` | Minimal adapter-board capture (330×330, no packing). Useful for bring-up. |
| `2025-06-06_ble_lowpower` | Low power: DC/DC on, QSPI flash put in deep power-down (`0xB9`), camera power-gated between shots, `sd_app_evt_wait()` idle. |
| `2025-06-07_ble_imu_tap` | Adds the XIAO Sense's LSM6DS3TR-C: a **tap** on the camera triggers a capture. |
| `2025-07-25_pinto_neopixel` | Pinto2 bring-up: drives the 5× SK6812-EC20 LED ring by bit-banging P0.30 (`LED_CE` P0.26). |
| `2025-07-25_pinto_camera` | **Current Pinto2 firmware.** Camera, JPEG, BLE and low power, with the W25Q64JW flash defined and put to sleep. |

### ios/2025-06-06_nRF52-Cam

A SwiftUI app (Xcode 16, iOS) that connects to the camera over the Nordic UART Service. It sends `P` to take a picture, rebuilds the JPEG from the notifications, and keeps a gallery.

- `BluetoothManager.swift`: CoreBluetooth central, NUS UUIDs, JPEG framing by SOI/EOI markers.
- `TakePictureIntent.swift`, `ShortcutsProvider.swift`: App Intents, so Siri or Shortcuts can trigger the camera.
- `OpenAIService.swift`, `GhibliQueue.swift`: optionally send each photo to the OpenAI image edits endpoint for a "Studio Ghibli" restyle. Jobs are queued if the app goes to the background.

**API key setup:** copy `Secrets.swift.example` to `nRF52 Cam/Secrets.swift` and put your key in it. `Secrets.swift` is gitignored.

### hardware/

| Folder | Contents |
|---|---|
| `2025-05-06_pinto-v1` | Gerbers for the first board named "Pinto": a 30 mm round, 2-layer EasyEDA design. Only the fab outputs survive. |
| `2025-05-27_hm01b0-xiao-adapter` | Gerbers for a plug-in board that carries the HM01B0 FPC connector and mounts on a XIAO nRF52840 Sense. Three exports from that night: the base version, plus `EXTOSC` rev1 and rev2, which add an external MCLK oscillator and an inverter for HSYNC. The June firmware targets this board. |
| `2025-07-08_pinto2` | **Pinto2**, the standalone board. `PADS_Pinto2_2025-07-08.zip` is the full EasyEDA Pro → PADS export (schematic + PCB). `assembly-preview/` holds JLCPCB SMT render images from 2025-07-09. `dxf/` holds board/layer DXFs exported 2025-07-24 for enclosure design. `2025-07-25_Netlist_Schematic1.tel` and `2025-07-25_BOM.csv` are the latest netlist and BOM; they include an 8 MHz MCLK oscillator (U15) that may postdate the 07-08 PADS export. |

### mechanical/

- `2025-04-18_hm01b0-sketch.dxf`: Onshape sketch of the HM01B0 module outline.
- `HM01B0-MNA-00FT870_module.step`: vendor 3D model of the sensor module.
- `2025-05-04_hm01b0-part1.stl`: first sensor mount.
- `2025-07-25_swd-pogo-jig.stl`: 3D-printed pogo-pin holder for SWD-flashing Pinto2 (see `photos/`).
- `2025-08-01_hm01b0-thin-part5.stl`, `2025-08-01_hm01b0-thin-part6.stl`: latest thin enclosure parts.

### datasheets/

| Folder | Files |
|---|---|
| `camera/` | HM01B0-MWA datasheet (register reference), HM01B0-ANA-00FT870 and HM01B0-MNA-00FT870 module datasheets (Pinto2 uses the ANA module). |
| `mcu/` | nRF52840 Product Specification v1.11 (aQFN73 ball map in §7.1.1). |
| `imu/` | LSM6DS3TR-C (XIAO Sense IMU). Pinto2 uses the similar LSM6DSR; that datasheet isn't included. |
| `led/` | SK6812-EC20 (2020 package used on Pinto2) and generic SK6812. |
| `power/` | TP4057 Li-ion charger. |
| `rf/` | ADWH002 2.4 GHz chip antenna (LCSC C22467691). |
| `xiao/` | Seeed XIAO nRF52840 schematic and pinout spreadsheet. |

### tools/

- `2025-05-28_tinyjpeg_debayer/TinyJPEG`: a copy of [serge-rgb/TinyJPEG](https://github.com/serge-rgb/TinyJPEG) (public domain), plus experiments run on raw sensor dumps (`source.txt`, `sample.*`): debayering, quality sweeps and image comparisons.
- `2025-05-28_tinyjpeg_debayer/debayer`: the 6-bit, mosaic and row-alignment (`align.c`) experiments that led to `tiny_jpeg_packed.h` (an encoder that reads 6-bit packed pixels straight from the frame buffer), with output JPEGs at several quality settings.
  Build any tool with `cc -O2 -o name name.c -lm`. The binaries are gitignored.
- `2025-07-25_bootloader`: flashes the Seeed XIAO nRF52840 Sense UF2 bootloader (with SoftDevice S140 7.3.0) onto a bare Pinto2 over SWD, using an ST-Link and OpenOCD. `wipe.sh` mass-erases first, then programs; `upload_bootloader.sh` only programs. After that the board shows up as a XIAO nRF52840 Sense, and the Arduino core and UF2 drag-and-drop work.

### reference/

- `2025-05-25_waveshare-PICO-Cam-A-HM01B0.zip`: Waveshare's RP2040 + HM01B0 sample code, used as a reference for the sensor's init sequence.

---

## History

| Date | Milestone |
|---|---|
| 2025-04-18 | HM01B0 module datasheets and STEP model collected. First sketch of the module outline. |
| 2025-05-04 | First printed mount for the sensor. |
| 2025-05-06 | **Pinto v1** round PCB sent to fab (Gerbers only). |
| 2025-05-25 | Firmware work starts on a XIAO nRF52840 Sense with the sensor hand-wired. A bit-banged 160×120 capture works. |
| 2025-05-26 | MCLK generated with TIMER/PPI/GPIOTE. Frames captured by DMA through the **SPI slave** peripheral. |
| 2025-05-27 | Full-res 324×~200 frames. **HM01B0-XIAO adapter PCB** designed (internal-MCLK and external-oscillator variants). |
| 2025-05-28 | Switched to HSYNC-as-CS line capture, with 6-bit packing to fit 328×328 in RAM. Desktop debayer/JPEG experiments. |
| 2025-05-29 | `tiny_jpeg_packed.h` with precomputed Huffman tables runs on the nRF52840. |
| 2025-05-30 | JPEGs stream over BLE UART to a Web Bluetooth page (`captures/2025-05-30_first-ble-jpeg.jpg`). |
| 2025-06-05 | Adapter PCB brought up (new pin map). |
| 2025-06-06 | Low-power work. **iOS app** started (Shortcuts, Ghibli restyle). |
| 2025-06-07 | IMU tap-to-capture. App icon ("totocam"). |
| 2025-07-08 | **Pinto2** designed: a standalone 30 mm round board with a bare nRF52840, Li-ion charging, LED ring, IMU, PDM mic and QSPI flash. JLC SMT order 2025-07-09. |
| 2025-07-24 | DXF exports for enclosure design. |
| 2025-07-25 | Pinto2 bring-up: XIAO bootloader flashed over SWD with a printed pogo jig. LED ring and camera firmware ported. |
| 2025-07-26 → 08-01 | Thin enclosure iterations. |

---

## How it works

### Getting pixels into the nRF52840

The nRF52840 has no camera interface. The HM01B0 can output pixels as a **1-bit serial stream** (register `0x3059 = 0x22`): one data line (D0) clocked by PCLK, framed by VSYNC and HSYNC (`0x1012 = 0x07`), with a gated clock (`0x3060 = 0x30`). Electrically this looks just like an SPI master writing to us, so:

```
HM01B0 PCLK  ──► nRF SPIS SCK
HM01B0 D0    ──► nRF SPIS MOSI
HM01B0 HSYNC ──► inverter ──► nRF SPIS CSN   (active-low chip select = "line valid")
```

Each image row becomes one SPIS transaction, received into `rowBuffer` by EasyDMA. On the `END` event the SPIS1 IRQ:

1. re-arms the DMA for the next row (`RELEASE`);
2. works out the bit alignment. CS has about 1 µs of nondeterministic latency, so the row starts 2 or 3 bits late, and `find_line_alignment_full_6b` tests both;
3. shifts, quantizes to 6 bits and packs 4 pixels into 3 bytes in `frameBuffer`.

At 328×328 that is 80.7 KB per frame instead of 107 KB.

The early sketches made the inverter in software (GPIOTE event on VSYNC → PPI → GPIOTE task toggling a spare pin that was jumpered to CSN). The adapter board and Pinto2 use a **SN74LVC2G14** dual Schmitt inverter instead, for VSYNC and HSYNC.

**MCLK:** on the breadboard, TIMER0 + PPI + GPIOTE toggled P1.11 to make an 8 MHz clock. The EXTOSC adapter and Pinto2 use a dedicated **8 MHz oscillator** (U15).

### Capture sequence (`firmware/2025-07-25_pinto_camera`)

1. Pull `CAMERA_ENABLE` low. This switches on an AO3401 P-FET, which powers the ME6211 2.8 V LDO for the sensor.
2. Init TWIM and write the register table.
3. Stream continuously for ~2.5 s so auto-exposure settles, then go to standby.
4. Arm SPIS and request one frame (`0x3020 = 1`, `0x0100 = 0x03`).
5. Wait until 324 rows have arrived, then stop SPIS and TWIM and power the sensor off.
6. JPEG-encode the packed buffer at quality 70 with `tje_encode_packed_with_func`. The output callback streams bytes directly into BLE.
7. Idle in `sd_app_evt_wait()`. DC/DC is enabled and the QSPI flash is in deep power-down.

### BLE protocol

- Advertises as **`XIAO_nRF52840_CAM`** with the **Nordic UART Service**:
  - Service `6E400001-B5A3-F393-E0A9-E50E24DCCA9E`
  - RX (write) `6E400002-…`: write `P` (or `p`) to take a picture
  - TX (notify) `6E400003-…`: JPEG bytes
- Chunk size is `MTU − 3`. Connection interval 7.5–15 ms (PHY, interval and MTU are printed to serial on connect).
- There is no length header. The receiver collects everything from the `FF D8` (SOI) marker to `FF D9` (EOI).
- Receivers: the Web Bluetooth pages in `firmware/2025-05-30_*` (Chrome) and the iOS app.

### JPEG encoder

`tiny_jpeg_packed.h` is a fork of TinyJPEG. Its changes:
- grayscale only;
- reads 6-bit packed pixels and expands them on the fly, so no full 8-bit frame is needed;
- a write callback for streaming output;
- Huffman tables precomputed into `precomputed_huffman_tables.h`, which saves ~3 KB of stack and fixes hangs on the nRF.

---

## Pinouts

### XIAO nRF52840 breadboard wiring (May 26–30 sketches)

| Signal | nRF pin | XIAO pin | Notes |
|---|---|---|---|
| Cam SDA | P0.04 | D4 | I²C, HM01B0 addr `0x24` |
| Cam SCL | P0.05 | D5 | |
| PCLK → SPIS SCK | P1.13 | D8 | |
| D0 → SPIS MOSI | P1.14 | D9 | |
| SPIS CSN | P1.15 | D10 | jumpered to the inverter output below |
| MCLK out (8 MHz) | P1.11 | D6 | TIMER0/PPI/GPIOTE |
| Sync in | P0.28 (D2) → P0.29 (D3) | D2/D3 | VSYNC at first, HSYNC from May 28 (the code keeps the name `PIN_VSYNC`) |
| Inverted sync out | P1.12 | D7 | GPIOTE software inverter |

The very first bit-bang sketch (`2025-05-25`) used PCLK = D8, D0 = D9, VSYNC = D2 and HREF = D3 as plain GPIOs.

### HM01B0-XIAO adapter PCB (June sketches)

| Signal | nRF pin | XIAO pin |
|---|---|---|
| Cam SDA | P1.14 | D9 |
| Cam SCL | P1.15 | D10 |
| PCLK → SPIS SCK | P0.04 | D4 |
| D0 → SPIS MOSI | P0.05 | D5 |
| HSYNC (inverted) → SPIS CSN | P0.29 | D3 |
| Camera power enable (active low) | P1.11 | D6 |
| IMU tap interrupt (`2025-06-07`) | Arduino pin 1 | D1 |

### Pinto2 (bare nRF52840-QIAA aQFN73)

Taken from `hardware/2025-07-08_pinto2/2025-07-25_Netlist_Schematic1.tel`, with balls mapped to GPIOs using the nRF52840 PS §7.1.1. The GPIO choices copy the XIAO nRF52840 Sense wherever possible, so the XIAO bootloader and Arduino variant work unchanged.

| Function | Net | Ball | GPIO | XIAO equiv. |
|---|---|---|---|---|
| Camera SDA | CAM_SDA | B15 | P1.14 | D9 |
| Camera SCL | CAM_SCL | A14 | P1.15 | D10 |
| Camera PCLK → SPIS SCK | P0.04_… | J1 | P0.04 | D4 |
| Camera D0 → SPIS MOSI | CAM_D0 | K2 | P0.05 | D5 |
| HSYNC (inverted, U6) → SPIS CSN | HSYNC_INV | A10 | P0.29 | D3 |
| VSYNC (inverted, U6) | P0.28_… | B11 | P0.28 | D2 |
| Camera power (Q2, active low) | CAM_CE | B19 | P1.11 | D6 |
| LED ring data (5× SK6812-EC20) | LED_DIN | B9 | P0.30 | — |
| LED ring power (Q3, active low) | LED_CE | G1 | P0.26 | — |
| IMU power (LSM6DSR) | 6D_PWR | P2 | P1.08 | IMU PWR |
| IMU INT1 | 6D_INT1 | T2 | P0.11 | IMU INT |
| IMU I²C SCL | INTERNAL_I2C_SCL | H2 | P0.27 | int. SCL |
| IMU I²C SDA | INTERNAL_I2C_SDA | — | (P0.07 on XIAO) | int. SDA |
| PDM mic data | P0.16_PDM_DATA | AC11 | P0.16 | PDM DATA |
| PDM mic clock | P1.00_PDM_CLK | AD22 | P1.00 | PDM CLK |
| Mic power | P1.10_MIC_PWR | A20 | P1.10 | MIC PWR |
| QSPI flash (W25Q64JW) SCK / CSN | | AC17 / AC21 | P0.21 / P0.25 | same |
| QSPI SIO0–3 | | AD16 / AD20 / AD18 / AC19 | P0.20 / P0.24 / P0.22 / P0.23 | same |
| Battery sense (divider R11/R12) | P0.31_AIN7_BAT | A8 | P0.31 | VBAT |
| Battery-sense enable | P0.14_READ_BAT | AC9 | P0.14 | same |
| Charge current select | P0.13_HICHG | AD8 | P0.13 | same |
| Charger status (TP4057 CHRG) | CHRG | B17 | P1.12 | — |
| Charger standby/done (TP4057 STDBY) | P1.13_SCK_D8 | A16 | P1.13 | D8 |
| Spare test nets | P0.02 (A12), P0.06 (L1) | | | A0 / blue LED |
| USB D+ / D− (33 Ω) | | AD6 / AD4 | — | |
| SWDIO / SWDCLK | test points | AC24 / AA24 | — | |
| RESET | test point | AC13 | P0.18 | |

**Note:** in the 2025-07-25 netlist, `INTERNAL_I2C_SDA` connects only to R3 and the IMU, not to any nRF ball. Check the schematic before you rely on the IMU on this revision.

Other Pinto2 parts: USB-C (GT-USB-7014G, 5.1 kΩ CC pull-downs), 1N5819 + AO3401 power path, TP4057 charger, a SOT-23 3.3 V LDO (U13), ME6211 2.8 V camera LDO, 32 MHz and 32.768 kHz crystals, ADWH002 chip antenna with an LC match (L1/L2), and B+/B− battery pads.

**Test points:** `3V3`, `B+`, `B-`, `GND`, `RESET`, `SWDCLK`, `SWDIO`. The pogo jig in `mechanical/` lands on these.

### HM01B0 FPC connector (Molex 505550-2420, 24-pin) on Pinto2

| Pin | Signal |
|---|---|
| 3, 21 | AVDD28 (2.8 V) |
| 4 | MCLK (8 MHz oscillator) |
| 8 | PCLK |
| 9 | SDA |
| 10 | D0 |
| 11 | SCL |
| 13 | HSYNC |
| 15 | VSYNC |
| 2, 5, 6, 19, 23 | GND |

---

## Building & flashing

1. Install the **Seeed nRF52 Boards** Arduino core and select *Seeed XIAO nRF52840 Sense*.
2. For a bare Pinto2, flash the bootloader first with `tools/2025-07-25_bootloader/wipe.sh`. This needs an ST-Link and OpenOCD.
3. Open `firmware/2025-07-25_pinto_camera/2025-07-25_pinto_camera.ino` and upload over USB.
4. To take a picture, connect with the iOS app or open `firmware/2025-05-30_web_bluetooth/index.html` in Chrome, then press capture (sends `P`).

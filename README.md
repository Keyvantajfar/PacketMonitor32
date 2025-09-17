# PacketMonitor32
**by @spacehuhn, ported to ESP-IDF by K1**

**ESP32 Packet Monitor + SD card**

![PacketMonitor32 Board](https://raw.githubusercontent.com/spacehuhn/PacketMonitor32/master/images/1.jpg)

A small ESP32 project that listens to Wi‑Fi traffic on a selected channel and (optionally) logs captured packets to an SD card in PCAP format. This is an enhanced ESP32 port of the original [ESP8266 PacketMonitor](https://github.com/spacehuhn/PacketMonitor).

---

## Quick highlights

* Monitor Wi‑Fi traffic (per channel 1–14)
* SD card support: save captures as `.pcap` files (open with Wireshark)
* Improved performance and average RSSI display thanks to ESP32

---

## What’s new (compared to ESP8266)

* SD card support to capture traffic
* Better performance (ESP32)
* Shows average RSSI per device

---

## Where to buy

* **Tindie:** [https://goo.gl/kZmVug](https://goo.gl/kZmVug)
* **AliExpress:** [https://goo.gl/hCCKMJ](https://goo.gl/hCCKMJ)

---

## Video demo

[![PacketMonitor32 Video](https://img.youtube.com/vi/7WYakpagPXk/0.jpg)](https://www.youtube.com/watch?v=7WYakpagPXk)

---

## Interface

![Interface explanation](https://raw.githubusercontent.com/spacehuhn/PacketMonitor32/master/images/2.jpg)

**Controls**

* Short press button: cycle monitored Wi‑Fi channel (1–14)
* Hold button \~2 seconds: enable / disable microSD card ("SD" appears on screen when active)

---

## Capturing traffic (SD card)

* Use a fast microSD card (higher speed improves capture reliability).
* **Format:** FAT32 is required. Reformat the card if you’re unsure.
* The device will not delete existing files, but **using an empty card is strongly recommended** to avoid accidental data loss.
* On every boot the board creates a new `.pcap` file in the SD root. Open these with [Wireshark](https://www.wireshark.org/).

**Safely remove SD card:** Hold the button for \~2 seconds until the `SD` indicator disappears; the board will stop writing and it is safe to remove the card.

**Notes / caveats**

* If the board crashes on write, try reformatting the card or use a different card; some low‑quality cards can cause instability.
* The capture is lossy when packet arrival exceeds what the SD card / CPU can write — some packets will be dropped under heavy load.

---

## Prerequisites

* A working ESP‑IDF environment (tested on ESP‑IDF v4.x and later). See the official docs: [https://github.com/espressif/esp-idf](https://github.com/espressif/esp-idf)
* Python and required tooling installed as per ESP‑IDF installation instructions
* MicroSD card (FAT32)
* USB cable and a serial/flash port on your ESP32 board

---

## Build & flash (clean, recommended sequence)

This section shows a minimal, robust sequence to build and flash the firmware using `idf.py`.

> **Important:** Run a full clean first to avoid stale build artifacts (recommended when switching branches or after big changes).

```bash
# 1. Open a terminal in the project root
# 2. Clean build artifacts (recommended)
idf.py fullclean

# 3. Configure (optional)
idf.py menuconfig     # adjust serial port, partition table, or other settings if needed

# 4. Build
idf.py build

# 5. Flash to the device (replace PORT with your serial port, e.g. /dev/ttyUSB0 or COM3)
idf.py -p PORT flash monitor
# The `monitor` target opens a serial monitor after flashing.
```

**Alternative:** If you prefer esptool, you can use it, but `idf.py flash` wraps the right build artifacts and partition table automatically.

---

## Troubleshooting

* **SD‑related reboot loop:** try a different card or reformat as FAT32. Some cards cause crashes when the code writes to corrupted or very slow media.
* **Upload problems:** try lowering the flash frequency and speed in `menuconfig` (e.g., 40MHz, 115200 baud) or use a different USB cable/port.
* **If build fails:** ensure ESP‑IDF environment is sourced (`. $HOME/esp/esp-idf/export.sh`) and Python deps are installed.

---

## Development notes

* The code writes one `.pcap` file per boot in the SD root. Files are named based on timestamp to avoid overwriting.
* If you want to include/exclude additional hardware or pins, see the config section at the top of the source (comments indicate where to change pins and display settings).

---

## License

This project is licensed under the **MIT License** — see the [LICENSE](LICENSE) file for details.

---

*Port by K1*

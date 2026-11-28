# Prima3 Pomphuis — pump house & irrigation controller

A pump house controller on a **WEMOS LOLIN S3 Mini** (ESP32-S3): it fills a water
tank from a borehole, runs up to **9 irrigation zones** through **7 solenoid
valves**, drives a tank filter pump, and keeps track of two households that share
one borehole. Everything is a web page on your own WiFi — no cloud, no app.

**Install / update page:** https://777bdvgp.github.io/Prima3-Pomphuis/

**Contents:** [Features](#features) · [Hardware](#hardware) · [Pin map](#pin-map) ·
[Installing the firmware](#installing-the-firmware) · [First start: WiFi](#first-start-wifi) ·
[Using the rain gauge](#using-the-rain-gauge) · [Updating](#updating) · [Building the firmware yourself](#building-the-firmware-yourself) ·
[Licence](#licence)

> **Who this is for:** someone with hands-on experience in electronics and
> irrigation plumbing. It is not a kit manual. The guide grows over time —
> parts marked **(to be completed)** get more detail and photos later.

## Features

- **HOME** — live status: tank level, float switches, filter pressure, borehole
  pump, which household is pumping, active irrigation zone, manual start / stop.
- **Irrigation** — 9 zones, each with its own valve configuration, start date /
  time, duration, every-N-days frequency, water source (tank or borehole),
  per-weekday suspend, manual or scheduled mode. A 7-day suspend calendar
  switches off all irrigation on chosen days.
- **Rain gauge** (optional) — asks a [Rain Gauge](https://777bdvgp.github.io/RainGauge/)
  on the same network every hour whether to irrigate today; zones with
  **Respect Rain Gauge Recommendation = Yes** are skipped when it says no.
  See [Using the rain gauge](#using-the-rain-gauge).
- **Tank** — fill by float switch or level sensor, any time or only inside a
  time window, start / stop levels, borehole timeout and retry, filter pump
  delay after filling, filter backwash warning from a pressure sensor.
- **Valves** — names for the 7 valves and 9 configurations (which valves are open
  for which zone / tank fill / default). A valve test page (system disabled only).
- **NTFY** — free push alerts on your phone ([ntfy.sh](https://ntfy.sh)): filter
  backwash, borehole pumping, per household.
- **Reports** — general, irrigation, water used and filter logs (last 200 lines each).
  The irrigation log has a line when a zone **starts**, when it **ends** (with the
  duration) and when a run is **skipped**, with the reason (rain gauge, suspended).
- **Remote pump API** — `/pumpstart`, `/pumpstop`, `/pumpstatus` (60 s heartbeat)
  so another controller can start the borehole pump.
- **WiFi** — saved networks list (add / delete, passwords never shown), joins the
  strongest network in range and the **strongest access point** when several
  share one WiFi name (mesh / multi-AP). Setup hotspot **Prima3-Setup** for a new
  board or when no network can be reached.
- **ABOUT** — **firmware update from GitHub** (CHECK / INSTALL) or from an uploaded `.bin`.
- **Safety** — software watchdog, gateway ping (reboots after 5 min without the
  router), reboot when WiFi is lost for 60 s, nightly reboot at 00:13, heap guard.

## Hardware

- WEMOS LOLIN S3 Mini (ESP32-S3, 4 MB flash, 2 MB PSRAM). The PCB was designed for
  the S2 Mini; the S3 Mini is a pin-compatible drop-in.
- PCB: Eagle files in [`PCB/`](PCB) (`WEMOS_ESP32_2S_V4_for_S3.sch` / `.brd`).
  Gerbers **(to be completed)**.
- Relay / driver outputs for 7 solenoid valves, the borehole pump (active high),
  filter pump relay and status LEDs; inputs for float switches, buttons, the two
  "household pumping" detectors and the tank level / filter pressure sensors.
- Bill of materials, wiring and photos **(to be completed)**.

## Pin map

| Output | GPIO | | Input | GPIO |
|---|---|---|---|---|
| Solenoid valve 03 | 4 | | Tank filter pressure (ADC) | 1 |
| Solenoid valve 08 | 6 | | Tank level sensor (ADC) | 3 |
| Solenoid valve 10 | 12 | | System enable / disable button | 35 |
| Solenoid valve 07 | 7 | | Household 2 pumping | 34 |
| Solenoid valve 02 | 13 | | Household 1 pumping | 36 |
| Solenoid valve 09 | 8 | | Tank full float switch | 38 |
| Solenoid valve 17 | 11 | | Tank low float switch | 44 |
| Borehole pump (HIGH = run) | 2 | | Manual borehole start / stop | 37 |
| Filter pump relay | 21 | | Filter pump on / off button | 43 |
| Manual pump LED | 10 | | BOOT button (WiFi setup) | 0 |
| Household 2 pumping LED | 9 | | | |
| Household 1 pumping LED | 14 | | | |
| Tank float high LED | 16 | | | |
| Tank float low LED | 18 | | | |
| System disabled LED | 17 | | | |
| Extra output 1 / 2 | 33 / 5 | | | |

GPIO 43 / 44 are the S3's UART0 pins — used as inputs here, so there is no serial
log on UART0 (the USB port carries `Serial`).

## Installing the firmware

No PlatformIO needed: open **https://777bdvgp.github.io/Prima3-Pomphuis/** in
Chrome or Edge on a PC, plug the S3 Mini in with a USB **data** cable and press
**CONNECT**. Choose **Erase** for a new board, then **Install**. If no port shows
up, hold **BOOT (0)**, tap **RST**, release **BOOT** and try again.

## First start: WiFi

1. A new controller opens its own hotspot **Prima3-Setup** (the LED blinks red fast).
2. Connect a phone to it — the setup page opens by itself (otherwise browse to `http://192.168.4.1/`).
3. Pick your network, type the password and give it a **fixed IP** (other devices
   such as a remote pump controller reach it by IP), press **SAVE & RESTART**.
4. Put the phone back on your WiFi and open the fixed IP (or `http://primapomphuis.local/`).

More networks: **SETTINGS → WiFi Networks**. Hold **BOOT** while powering up to open
the setup hotspot again. If no saved network can be reached for about 4 minutes,
the hotspot opens by itself for 5 minutes, then the controller restarts and tries again.

## Using the rain gauge

Optional: a [Rain Gauge](https://777bdvgp.github.io/RainGauge/) on the same network
decides whether the garden needs water today.

1. **SETTINGS** — tick **Use Rain Gauge** and enter its IP as **Rain Gauge IP**
   (e.g. `192.168.1.50`). The controller asks the gauge at once and then every hour.
2. **IRRIGATION** — for every zone that must follow the gauge, set
   **Respect Rain Gauge Recommendation = Yes**. A zone set to **No** waters on its
   schedule whatever the gauge says.

SETTINGS shows the gauge's last answer and an **Irrigation Today** badge:

| Badge | Meaning |
|---|---|
| green **IRRIGATE** | the gauge says water (or Use Rain Gauge is off) |
| red **NO IRRIGATION TODAY** | the gauge says skip; zones with the setting on Yes are skipped |
| orange **SKIP - but no zone has Respect Rain Gauge Recommendation = Yes** | the gauge says skip, but no zone listens to it, so they all water |

A skipped run is written to the irrigation log, e.g.
`Skipped: FRONT - rain gauge: Skip - soil still has water …`, and the zone moves on
to its next date.

**If the gauge cannot be reached** (no power, WiFi down, wrong IP) the controller
keeps its last answer for 3 hours, then waters as normal — and after a restart (every
night at 00:13) it waters as normal at once — so a broken gauge never leaves the
garden dry for more than a day. The general log says "Rain gauge not reachable" and
"Rain gauge reachable again". If the rain gauge itself is set to *No Irrigation*
(plain rain meter) it always answers "irrigate".

## Updating

On the **ABOUT** page: **CHECK FOR UPDATE** compares the version with the one on the
install page; **INSTALL FROM GITHUB** downloads and installs it (not while a zone
irrigates or the borehole pump runs). Settings and logs are kept; a failed
download leaves the old firmware running. Without internet at the controller:
download [`prima3.bin`](docs/firmware/prima3.bin) and use **UPLOAD & INSTALL**.

## Building the firmware yourself

[PlatformIO](https://platformio.org/) project in [`Prima3/`](Prima3):

```bash
cd Prima3
pio run -e usb -t upload                                   # first flash over USB
pio run -e ota -t upload --upload-port <controller ip>     # later over WiFi
```

Optional built-in WiFi networks: copy `src/wifi_secrets.example.h` to
`src/wifi_secrets.h` (git-ignored). Builds with that file are named `_PRIVATE` —
do not share them, your passwords are inside.

## Licence

[CC BY-NC 4.0](LICENSE) — free for personal use, not for sale. No warranty: this
controls pumps and valves, and the pumps run on mains power — have mains wiring
done or checked by a qualified person.

Copyright (c) 2026 Bennie Vermeulen

# Aquarium Light Controller

An ESP32-C3 Mini controller for a dimmable 12 V LED-strip aquarium light.
The controller uses local time from NTP, gradual sunrise and sunset ramps,
and PWM through a MOSFET stage.

## Project files

- [`aquarium-light.ino`](aquarium-light.ino) — firmware and light schedule.
- [`aquarium-light.md`](aquarium-light.md) — hardware, wiring, pinouts, and power notes.
- [`secrets.h.example`](secrets.h.example) — safe template for local Wi-Fi credentials.
- `secrets.h` — local credentials; ignored by Git and must not be committed.
- [`.gitignore`](.gitignore) — keeps `secrets.h` out of Git.

## Current light schedule

The firmware uses Europe/Oslo local time:

| Time | Output |
|---|---|
| 08:00–09:00 | Gradual ramp from 0% to 30% PWM |
| 09:00–18:00 | 30% PWM |
| 18:00–19:00 | Gradual ramp from 30% to 0% PWM |
| 19:00–08:00 | Off |

`30%` is the maximum PWM duty cycle configured in the firmware. It is not a
measurement of the light reaching the plants. The light stays off until the
clock has successfully synchronized with NTP after startup.

## Over-the-air updates

OTA is disabled until `OTA_PASSWORD` is set in the local `secrets.h` file. The
first OTA-capable firmware must be flashed over USB, and the board must use an
OTA-capable partition scheme. After that, the ESP32 advertises itself as
`aquarium-light.local` and can receive authenticated updates over the LAN.

The current firmware uses the ESP32 Arduino core's built-in `ArduinoOTA`
library. Keep the OTA password private and only expose the device on a trusted
network.

## Setup

The commands below assume that `arduino-cli` and the ESP32 board package are
already installed.

1. Create the local credentials file and edit the placeholders. Set a unique
   OTA password as well:

   ```sh
   cp secrets.h.example secrets.h
   ```

   Then add or edit this line in `secrets.h`:

   ```cpp
   #define OTA_PASSWORD "your-long-unique-ota-password"
   ```

2. Compile the sketch with an OTA-capable partition scheme. Do not select a
   `no_ota` partition scheme:

   ```sh
   arduino-cli compile \
     --fqbn esp32:esp32:esp32c3 \
     --board-options=PartitionScheme=default \
     .
   ```

3. Flash this first OTA-capable build over USB. Replace the port if needed:

   ```sh
   arduino-cli upload \
     --port /dev/ttyACM0 \
     --fqbn esp32:esp32:esp32c3 \
     --board-options=PartitionScheme=default \
     --verify \
     .
   ```

4. Monitor startup and NTP/OTA messages:

   ```sh
   arduino-cli monitor \
     --port /dev/ttyACM0 \
     --config baudrate=115200
   ```

## Security

Do not commit `secrets.h` or paste its contents into issues, pull requests, or
public logs. If the real credentials are ever committed or exposed, change the
Wi-Fi password.

## Hardware safety

- Do not connect the LED strip directly to an ESP32 GPIO.
- Verify that the XL4015 output is exactly 5.0 V before connecting the ESP32.
- Disconnect power before changing wiring.
- Read [`aquarium-light.md`](aquarium-light.md) before assembling the MOSFET,
  transistor, buck-converter, and common-ground connections.

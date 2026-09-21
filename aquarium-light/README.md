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
| 08:00–09:00 | Gradual ramp from 0% to 40% PWM |
| 09:00–8:00 | 40% PWM |
| 18:00–19:00 | Gradual ramp from 40% to 0% PWM |
| 19:00–08:00 | Off |

`40%` is the maximum PWM duty cycle configured in the firmware. It is not a
measurement of the light reaching the plants. The light stays off until the
clock has successfully synchronized with NTP after startup.

## Setup

The commands below assume that `arduino-cli` and the ESP32 board package are
already installed.

1. Create the local credentials file and edit the placeholders:

   ```sh
   cp secrets.h.example secrets.h
   ```

2. Compile the sketch:

   ```sh
   arduino-cli compile --fqbn esp32:esp32:esp32c3 .
   ```

3. Upload it to the controller. Replace the port if needed:

   ```sh
   arduino-cli upload \
     --port /dev/ttyACM0 \
     --fqbn esp32:esp32:esp32c3 \
     --verify \
     .
   ```

4. Monitor startup and NTP messages:

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

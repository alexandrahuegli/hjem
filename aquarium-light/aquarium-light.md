# Aquarium Light Controller

## Overview
ESP32 C3 Mini based PWM controller for a 12V LED strip grow light. Timed on/off schedule via NTP, with MOSFET switching and buck converter power supply.

## Hardware

| Component | Role |
|---|---|
| ESP32 C3 Mini | Controller, WiFi, NTP, PWM |
| XL4015 buck converter | 12V → 5V for ESP32 |
| IRFB7545 MOSFET | Switches 12V LED strip |
| BC337 NPN transistor | Level shifts 3.3V GPIO to 5V gate drive |
| 12V 24W PSU | Main power input |
| 2835 double row LED strip | Grow light output |

## Wiring

### Power
- 12V PSU → XL4015 IN+ / IN- (set output to 5V)
- XL4015 OUT+ → ESP32 VIN
- 12V PSU → LED strip positive
- Common GND throughout

### Gate drive (inverted logic)
- ESP32 GPIO3 → 10kΩ → BC337 Base (middle pin)
- BC337 Emitter (left pin) → GND
- BC337 Collector (right pin) → 100Ω → MOSFET Gate (left pin)
- 5V → 10kΩ pullup → MOSFET Gate

### LED switching
- LED strip negative → MOSFET Drain (middle pin)
- MOSFET Source (right pin) → GND

## Component Pinouts

### BC337 (flat face toward you, legs down)
- Left → Emitter → GND
- Middle → Base → 10kΩ → GPIO3
- Right → Collector → 100Ω → MOSFET Gate + 10kΩ pullup to 5V

### IRFB7545 (flat face toward you, legs down)
- Left → Gate
- Middle → Drain → LED strip negative
- Right → Source → GND

## Logic (inverted)
| GPIO3 | BC337 | MOSFET | LEDs |
|---|---|---|---|
| HIGH (3.3V) | ON | OFF | OFF |
| LOW (0V) | OFF | ON | ON |

## Firmware
- Connect to WiFi and synchronize time via NTP
- Use Europe/Oslo local time, including daylight-saving changes
- Ramp the light from 0% to 30% between 08:00 and 09:00
- Hold the light at 30% PWM from 09:00 to 17:00
- Ramp the light from 30% to 0% between 17:00 and 18:00
- Keep the light off outside 08:00–18:00
- Fail dark until time synchronization succeeds

`MAX_BRIGHTNESS_PERCENT` is the maximum PWM duty cycle, not a measurement of
the light level reaching the aquarium. The current value is intentionally
limited to 30% of the strip's configured PWM range.

## Power Budget
| PSU | Max safe strip length |
|---|---|
| 9W | ~30cm |
| 24W | ~80cm |

Strip: 2835 double row 240 LEDs/m (~0.1W per LED)

## Notes
- Logic is inverted — account for this in firmware
- XL4015 has onboard voltage display — set to exactly 5.0V before connecting ESP32
- Common GND shared between 12V and 5V systems
- Strip length TBD pending 24W PSU arrival

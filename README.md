| Supported Targets | ESP32-C6 |
| ----------------- | -------- |

# Zigbee PWM Fan Controller

ESP32-C6 Zigbee end device that exposes a **fan** entity with percentage speed control in Home Assistant via Zigbee2MQTT.
Speed (0–100%) is mapped to a PWM signal on **GPIO9**, which you can use to drive a fan, dimmer, or any PWM-controlled load.

## How it works

- Uses Zigbee's standard On/Off + Level Control clusters (endpoint 10) — the same pair used for dimmable lights, since Zigbee's dedicated Fan Control cluster only supports discrete Low/Medium/High presets, not a percentage.
- A Zigbee2MQTT external converter ([`z2m/c6-pwm-fan.mjs`](z2m/c6-pwm-fan.mjs)) maps that Level Control value to a `fan` entity with a `speed` percentage instead of `brightness` — see [`docs/zigbee2mqtt-fan-converter.md`](docs/zigbee2mqtt-fan-converter.md) for setup.
- `speed` (0–254 on the wire, 0–100% in Home Assistant) sets the LEDC PWM duty cycle on `GPIO9` at 5 kHz, 8-bit resolution.
- `state` off drives duty to 0 regardless of the speed level.

## Hardware Required

* ESP32-C6 SuperMini (or any ESP32-C6 board)
* A USB cable for programming
* Zigbee coordinator already paired to Home Assistant via Zigbee2MQTT

## Wiring

| Signal | GPIO |
|--------|------|
| PWM output | **GPIO9** |

To change the pin or PWM frequency, edit `LIGHT_OUTPUT_GPIO` and `LIGHT_PWM_FREQUENCY_HZ` in `main/light_driver.h`.

## Configure the project

```
idf.py set-target esp32c6
```

* This board uses 4 MB flash — confirmed via `esptool flash_id` against the physical chip (ESP32-C6FH4). The partition table (two ~1.9 MB OTA app slots) is sized for that.

## Firmware Stack

Built on `espressif/esp-zigbee-lib` 2.x (Espressif's proprietary Zigbee stack, not the legacy ZBOSS-based 1.x line — see `main/idf_component.yml`). ESP-IDF stays pinned at v5.5.4, which is the version this Zigbee SDK release is actually tested against upstream; there's no need or documented support for a newer ESP-IDF major version here.

## Build and Flash

See [`docs/flash-and-home-assistant.md`](docs/flash-and-home-assistant.md) for the full step-by-step checklist.

Quick start:
```
idf.py -p COMx erase-flash
idf.py -p COMx flash monitor
```

## OTA Updates

The device advertises a Zigbee OTA Upgrade client, so once this build is on the device, future firmware updates can be pushed wirelessly through Zigbee2MQTT — no USB required. See [`docs/ota-updates.md`](docs/ota-updates.md) for how to build and publish an update.

**One-time exception:** this firmware changes the partition table (single factory slot → two OTA slots), which cannot itself be applied over the air. You must flash this version once via USB (`idf.py -p COMx erase-flash` then `flash`) before OTA works.

## Example Serial Output

```
I (548) ESP_ZB_PWM_FAN: Initialize Zigbee stack
I (568) ESP_ZB_PWM_FAN: Deferred driver initialization successful
I (568) ESP_ZB_PWM_FAN: Device started up in factory-reset mode
I (3558) ESP_ZB_PWM_FAN: Joined network successfully: PAN ID(0x1a62, EXT: 0x...), Channel(13), Short Address(0x0000)
I (10238) ESP_ZB_PWM_FAN: Fan sets to On
I (10238) ESP_ZB_PWM_FAN: Fan speed sets to 128/254
```

## Troubleshooting

- If the device does not join, erase flash first and retry pairing mode.
- If Home Assistant doesn't show a fan entity, confirm the Zigbee2MQTT external converter is installed and enabled — see [`docs/zigbee2mqtt-fan-converter.md`](docs/zigbee2mqtt-fan-converter.md) — and re-interview the device.
- To change the output GPIO or PWM frequency, edit `LIGHT_OUTPUT_GPIO` / `LIGHT_PWM_FREQUENCY_HZ` in `main/light_driver.h` and reflash.

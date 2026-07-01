| Supported Targets | ESP32-C6 |
| ----------------- | -------- |

# Zigbee PWM Output (Dimmable)

ESP32-C6 Zigbee end device that exposes a **dimmable light** entity in Home Assistant via Zigbee2MQTT.
The brightness slider (0–255) is mapped to a PWM signal on **GPIO1**, which you can use to drive a fan controller, dimmer, or any PWM-controlled load.

## How it works

- Advertises as a Zigbee HA Dimmable Light (On/Off + Level Control clusters, no Color Control).
- Zigbee2MQTT exposes `state` and `brightness` — no color or effect clutter.
- `brightness` (0–254) sets the LEDC PWM duty cycle on `GPIO1` at 5 kHz, 8-bit resolution.
- `state` off drives duty to 0 regardless of brightness level.

## Hardware Required

* ESP32-C6 SuperMini (or any ESP32-C6 board)
* A USB cable for programming
* Zigbee coordinator already paired to Home Assistant via Zigbee2MQTT

## Wiring

| Signal | GPIO |
|--------|------|
| PWM output | **GPIO1** |

To change the pin or PWM frequency, edit `LIGHT_OUTPUT_GPIO` and `LIGHT_PWM_FREQUENCY_HZ` in `main/light_driver.h`.

## Configure the project

```
idf.py set-target esp32c6
```

* If your board uses 4 MB flash (common on C6 SuperMini), verify `Serial flasher config -> Flash size` in `idf.py menuconfig`.

## Build and Flash

See [`docs/flash-and-home-assistant.md`](docs/flash-and-home-assistant.md) for the full step-by-step checklist.

Quick start:
```
idf.py -p COMx erase-flash
idf.py -p COMx flash monitor
```

## Example Serial Output

```
I (548) ESP_ZB_ON_OFF_LIGHT: Initialize Zigbee stack
I (568) ESP_ZB_ON_OFF_LIGHT: Deferred driver initialization successful
I (568) ESP_ZB_ON_OFF_LIGHT: Device started up in factory-reset mode
I (578) ESP_ZB_ON_OFF_LIGHT: Start network steering
I (3558) ESP_ZB_ON_OFF_LIGHT: Joined network successfully (Extended PAN ID: ..., Channel:13)
I (10238) ESP_ZB_ON_OFF_LIGHT: Light sets to On
I (10238) ESP_ZB_ON_OFF_LIGHT: Light level sets to 128
```

## Troubleshooting

- If the device does not join, erase flash first and retry pairing mode.
- If Home Assistant still shows color/effect after flashing, remove and re-interview the device in Zigbee2MQTT.
- To change the output GPIO or PWM frequency, edit `LIGHT_OUTPUT_GPIO` / `LIGHT_PWM_FREQUENCY_HZ` in `main/light_driver.h` and reflash.

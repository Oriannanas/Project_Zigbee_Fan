# Exposing This Device as a Fan in Home Assistant

The firmware exposes standard Zigbee On/Off + Level Control clusters (endpoint 10) - the same pair used for dimmable lights. Zigbee's dedicated Fan Control cluster only supports discrete Low/Medium/High/Auto presets in this SDK, not a percentage, so it isn't used here. Instead, [`z2m/c6-pwm-fan.mjs`](../z2m/c6-pwm-fan.mjs) tells Zigbee2MQTT to treat the Level Control cluster's speed as a fan, using converters (`fz.fan_speed`/`tz.fan_speed`) that already ship in `zigbee-herdsman-converters` for exactly this pattern. Home Assistant then gets a native `fan.` entity with a 0–100% speed slider.

## Install the converter

1. Copy [`z2m/c6-pwm-fan.mjs`](../z2m/c6-pwm-fan.mjs) into your Zigbee2MQTT `external_converters/` directory (same level as `configuration.yaml`).
2. Zigbee2MQTT 2.11+ disables running external converters by default - enable it (look for "external converters" / "external JS" in Settings → General/Advanced, or `enable_external_js` in `configuration.yaml`) and restart Zigbee2MQTT.
3. Since this repo's firmware also changed the device's manufacturer name/model identifier (see `main/esp_zb_light.h`) so this converter can uniquely match it, Z2M will see it as a new device. Remove the old paired device and re-pair/re-interview it.
4. Confirm in the Z2M frontend that the device now shows a `state` (on/off) and `speed` (1–254) expose, matched to model `C6_PWM_Fan` / vendor `DIY_Fans`.

## What shows up in Home Assistant

A `fan.<device_name>` entity with:
- On/off toggle
- A percentage speed slider (Home Assistant scales the Zigbee `speed` range 1–254 to 0–100% automatically via the `speed_range_min`/`speed_range_max` MQTT fan fields Z2M publishes)

## Notes

- `configure()` in the converter binds and sets up attribute reporting for both clusters, so state changes made at the device (if you ever add a physical button) are reflected back to Home Assistant.
- If you rename the manufacturer/model strings again later, update `zigbeeModel`/`model`/`vendor` in `z2m/c6-pwm-fan.mjs` to match, or Z2M will stop recognizing the device.

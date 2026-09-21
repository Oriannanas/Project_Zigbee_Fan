# OTA Updates via Zigbee2MQTT

Once the two-OTA-slot build in this repo is flashed to the device (one time, via USB — see below), subsequent firmware updates can be delivered wirelessly over Zigbee. Z2M's coordinator acts as the OTA server; the device is the OTA client and queries it for a new image (handled by `esp-zigbee-lib`'s OTA client internals — see `main/esp_zb_light.c`).

## One-time USB step (unavoidable)

Adding OTA support required changing the partition table from a single 900K `factory` app slot to two ~1.9 MB `ota_0`/`ota_1` slots (see `partitions.csv`). A partition table change can't be applied over the air — it has to be flashed once via USB:

```
idf.py -p COMx erase-flash
idf.py -p COMx flash monitor
```

After this, remove and re-pair/re-interview the device in Zigbee2MQTT so it picks up the new OTA cluster. Every update after this point can go over the air.

## What identifies an image to the device

These constants in `main/esp_zb_light.h` must match the OTA image you publish:

| Field | Value | Notes |
|---|---|---|
| Manufacturer code | `0x131B` | `OTA_UPGRADE_MANUFACTURER` |
| Image type | `0x1011` | `OTA_UPGRADE_IMAGE_TYPE` |
| Current file version | `0x01010000` | `OTA_UPGRADE_RUNNING_FILE_VERSION` |

**Before each release:** bump `OTA_UPGRADE_RUNNING_FILE_VERSION` in `esp_zb_light.h` to a value higher than what's currently running on the device, then rebuild. The OTA file's header version must match this new value — the device only accepts an image whose header file-version is greater than its own.

## Building the OTA image

1. `idf.py build` — produces `build/grille_fan.bin`.
2. Wrap that `.bin` with a Zigbee ZCL OTA upgrade header (manufacturer code, image type, and file version fields above, plus the file size). Zigbee2MQTT's OTA tooling (`zigbee-herdsman-converters`) and Espressif's `esp-zigbee-sdk` repo both ship scripts for this — check whichever you have installed for the exact header-packing tool, since none ships with this ESP-IDF install.
3. Because this is a fully custom device (not in the `zigbee-herdsman-converters` database), Zigbee2MQTT needs an **external converter** for it that points the `ota` extension at your image — either a local file path or an `index.json` served over HTTP, keyed by the manufacturer code / image type above.

## Rollback safety

`CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE` is on. If a newly-flashed image fails to successfully rejoin the Zigbee network, the bootloader automatically rolls back to the previous working image on next boot instead of leaving the device bricked. The new image is marked valid (`esp_ota_mark_app_valid_cancel_rollback()`) right after a successful network join — see `esp_zb_light.c`.

## Flash headroom

Flash is 4 MB total (ESP32-C6FH4, confirmed via `esptool flash_id`). Each OTA app slot is ~1.9 MB; the current build uses about 556 KB (~71% free per slot), so there's a lot of room to grow before needing to touch `partitions.csv` again.

// Zigbee2MQTT external converter for the ESP32-C6 PWM fan controller in this repo.
//
// The firmware only exposes standard On/Off + Level Control clusters (see main/esp_zb_light.c) -
// Zigbee's own Fan Control cluster can't do percentage speed, only discrete Low/Medium/High presets.
// zigbee-herdsman-converters already ships fz.fan_speed/tz.fan_speed for exactly this case: they read
// and write genLevelCtrl's currentLevel (0-254) under a `speed` property instead of `brightness`.
// exposes.presets.fan().withSpeed(1, 254) tells Home Assistant's MQTT fan integration to present that
// as a 0-100% slider - see docs/zigbee2mqtt-fan-converter.md for the install steps and background.
import * as fz from 'zigbee-herdsman-converters/converters/fromZigbee';
import * as tz from 'zigbee-herdsman-converters/converters/toZigbee';
import * as exposes from 'zigbee-herdsman-converters/lib/exposes';
import * as reporting from 'zigbee-herdsman-converters/lib/reporting';
import * as utils from 'zigbee-herdsman-converters/lib/utils';

const e = exposes.presets;
const ea = exposes.access;

// Stock tz.fan_speed sends genLevelCtrl 'moveToLevelWithOnOff'. Its "WithOnOff" variant is meant
// for devices that expose ONLY a level cluster (no separate on/off), so the level command also
// carries on/off intent. This device already has a real genOnOff cluster (handled by tz.on_off
// below) driving light_driver_set_power() independently of light_driver_set_level() - see
// main/esp_zb_light.c. Observed in the wild: every speed update to this device (even a 1-2%
// change) made the Zigbee stack transiently report currentLevel=0 mid-transition, which its
// "WithOnOff" handling reads as "target is 0" and latches OnOff=Off - leaving the fan stuck off
// (still holding the new, non-zero level) until the next explicit 'on' command corrected it. Using
// plain 'moveToLevel' (no on/off linkage) sidesteps that on/off coupling entirely.
const tzSpeedNoOnOff = {
    key: ['speed'],
    convertSet: async (entity, key, value, meta) => {
        await entity.command('genLevelCtrl', 'moveToLevel', {level: Number(value), transtime: 0}, utils.getOptions(meta.mapped, entity));
        return {state: {speed: value}};
    },
    convertGet: async (entity, key, meta) => {
        await entity.read('genLevelCtrl', ['currentLevel']);
    },
};

// Repurposes the standard Level Control "OnOffTransitionTime" attribute (ZCL units: 1/10s) as a
// configurable duration for the firmware's hardware PWM fade - see light_driver_set_fade_time_ms()
// in main/light_driver.c and the EZB_ZCL_ATTR_LEVEL_ON_OFF_TRANSITION_TIME_ID case in main/esp_zb_light.c.
const fzFadeTime = {
    cluster: 'genLevelCtrl',
    type: ['attributeReport', 'readResponse'],
    convert: (model, msg, publish, options, meta) => {
        if (msg.data.onOffTransitionTime !== undefined) {
            return {fade_time: msg.data.onOffTransitionTime * 100};
        }
    },
};
const tzFadeTime = {
    key: ['fade_time'],
    convertSet: async (entity, key, value, meta) => {
        const deciseconds = Math.round(Number(value) / 100);
        await entity.write('genLevelCtrl', {onOffTransitionTime: deciseconds});
        return {state: {fade_time: deciseconds * 100}};
    },
    convertGet: async (entity, key, meta) => {
        await entity.read('genLevelCtrl', ['onOffTransitionTime']);
    },
};

export default {
    zigbeeModel: ['C6_PWM_Fan'],
    model: 'C6_PWM_Fan',
    vendor: 'DIY_Fans',
    description: 'ESP32-C6 Zigbee PWM fan controller (percentage speed)',
    ota: true,
    fromZigbee: [fz.on_off, fz.fan_speed, fzFadeTime],
    toZigbee: [tz.on_off, tzSpeedNoOnOff, tzFadeTime],
    exposes: [
        e.fan().withState('state').withSpeed(1, 254),
        e.numeric('fade_time', ea.ALL)
            .withUnit('ms')
            .withDescription('Duration of the hardware PWM fade applied to power/level changes')
            .withValueMin(0)
            .withValueMax(5000)
            .withValueStep(100),
    ],
    configure: async (device, coordinatorEndpoint) => {
        const endpoint = device.getEndpoint(10); // HA_ESP_FAN_ENDPOINT in main/esp_zb_light.h
        await reporting.bind(endpoint, coordinatorEndpoint, ['genOnOff', 'genLevelCtrl']);
        await reporting.onOff(endpoint);
        await reporting.brightness(endpoint); // configures genLevelCtrl/currentLevel reporting
        await endpoint.read('genLevelCtrl', ['onOffTransitionTime']); // populate fade_time on first interview
    },
};

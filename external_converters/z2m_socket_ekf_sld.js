import {Zcl, ZSpec} from 'zigbee-herdsman';
import * as m from 'zigbee-herdsman-converters/lib/modernExtend';
import * as exposes_1 from 'zigbee-herdsman-converters/lib/exposes';
import * as reporting from 'zigbee-herdsman-converters/lib/reporting';
import * as utils_1 from 'zigbee-herdsman-converters/lib/utils';
import * as logger from 'zigbee-herdsman-converters/lib/logger';

const e = exposes_1.presets;
const ea = exposes_1.access;

const attrPlugKeyLock = 0xf000;
const attrPlugLedCtrl = 0xf001;
const attrPlugSwitchCurrentMax = 0xf002;
const attrPlugSwitchPowerMax = 0xf003;
const attrPlugSwitchTimeReload = 0xf004;
const attrPlugSwitchProtectCtrl = 0xf005;
const attrPlugSwitchAutoRestart = 0xf006;
const attrSocketAdjustCurrent = 0xf007;
const attrSocketAdjustVoltage = 0xf008;
const attrSocketAdjustPower = 0xf009;

const energyResetExtend = {
    energyReset: () => {
      const exposes = [
        e.enum("energy_reset", ea.SET, ["reset"]).withDescription("Reset of accumulated energy"),
      ];
      const toZigbee = [
        {
          key: ["energy_reset"],
          convertSet: async (entity, key, value, meta) => {
            await entity.command('seMetering', 0x80, {}, utils_1.getOptions(meta.mapped, entity));
          },
        },
      ];
      const fromZigbee = [];
      return {
          exposes,
          fromZigbee,
          toZigbee,
          isModernExtend: true,
      };
    },
}
  

export default {
        zigbeeModel: ["RCS-ST16-z-SlD"],
        model: "RCS-ST16-z-SlD",
        vendor: "Slacky-DIY",
        description: "Socket EKF with power monitoring with custom firmware",
        extend: [
            m.deviceAddCustomCluster("seMetering", {
                name: "seMetering",
                ID: 0x0702,
                attributes: {},
                commands: {
                    resetEnergyMeters: {
                        name: "resetEnergyMeters",
                        ID: 0x80,
                        parameters: [],
                    },
                },
                commandsResponse: {},
            }),
            m.deviceAddCustomCluster("haElectricalMeasurement", {
                name: "haElectricalMeasurement",
                ID: 0x0b04,
                attributes: {
                    customAdjustCurrent: {
                        name: "customAdjustCurrent",
                        ID: attrSocketAdjustCurrent,
                        type: 0x39,
                        write: true,
                        max: 0xffffffff,
                    },
                    customAdjustVoltage: {
                        name: "customAdjustVoltage",
                        ID: attrSocketAdjustVoltage,
                        type: 0x39,
                        write: true,
                        max: 0xffffffff,
                    },
                    customAdjustPower: {
                        name: "customAdjustPower",
                        ID: attrSocketAdjustPower,
                        type: 0x39,
                        write: true,
                        max: 0xffffffff,
                    },
                },
                commands: {},
                commandsResponse: {},
            }),
            m.onOff({powerOnBehavior: true}),
            m.binary({
                name: "key_lock",
                valueOn: ["LOCK", 1],
                valueOff: ["UNLOCK", 0],
                cluster: "genOnOff",
                attribute: {ID: attrPlugKeyLock, type: 0x10},
                description: "Key lock enable/disable",
            }),
            m.enumLookup({
                name: "led_control",
                lookup: {off: 0, on: 1, "on/off": 2, "off/on": 3},
                cluster: "genOnOff",
                attribute: {ID: attrPlugLedCtrl, type: 0x30},
                description: "Led control",
            }),
            m.electricityMeter({
                current: {divisor: 100},
                voltage: {divisor: 100},
                power: {divisor: 1},
                energy: {divisor: 1000000},
            }),
            energyResetExtend.energyReset(),
            m.binary({
                name: "protect_control",
                valueOn: ["ON", 1],
                valueOff: ["OFF", 0],
                cluster: "haElectricalMeasurement",
                attribute: {ID: attrPlugSwitchProtectCtrl, type: 0x10},
                description: "Protection control enable/disable",
            }),
            m.binary({
                name: "automatic_restart",
                valueOn: ["ON", 1],
                valueOff: ["OFF", 0],
                cluster: "haElectricalMeasurement",
                attribute: {ID: attrPlugSwitchAutoRestart, type: 0x10},
                description: "Automatic restart enable/disable for voltage only",
            }),
            m.numeric({
                name: "voltage_min",
                unit: "V",
                cluster: "haElectricalMeasurement",
                attribute: "rmsExtremeUnderVoltage",
                description: "Minimum voltage value",
                valueMin: 0,
                valueMax: 300,
                scale: 100,
            }),
            m.numeric({
                name: "voltage_max",
                unit: "V",
                cluster: "haElectricalMeasurement",
                attribute: "rmsExtremeOverVoltage",
                description: "Maximum voltage value",
                valueMin: 0,
                valueMax: 300,
                scale: 100,
            }),
            m.numeric({
                name: "current_max",
                unit: "A",
                cluster: "haElectricalMeasurement",
                attribute: {ID: attrPlugSwitchCurrentMax, type: 0x21},
                description: "Maximum current value",
                scale: 100,
                valueMin: 0,
                valueMax: 16,
                valueStep: 0.1,
            }),
            m.numeric({
                name: "power_max",
                unit: "W",
                cluster: "haElectricalMeasurement",
                attribute: {ID: attrPlugSwitchPowerMax, type: 0x29},
                description: "Maximum power value",
                valueMin: 0,
                valueMax: 3600,
            }),
            m.numeric({
                name: "time_reload",
                unit: "sec",
                cluster: "haElectricalMeasurement",
                attribute: {ID: attrPlugSwitchTimeReload, type: 0x21},
                description: "Reload time",
                valueMin: 5,
                valueMax: 60,
            }),
        ],
        meta: {},
        ota: true,
};

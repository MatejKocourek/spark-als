#pragma once

#if defined(ARDUINO) && ARDUINO >= 100
#include <Arduino.h>
#else
#include <WProgram.h>
#endif

// Set these according to your device.
#define SCL_PIN PIN_USI_SCL
#define SCL_PORT PORTB
#define SDA_PIN PIN_USI_SDA
#define SDA_PORT PORTB
// #define I2C_HARDWARE 1 //If your device supports hardware I2C, turn this on to increase speed and decrease code size.

#define I2C_FASTMODE 1 // Disable only when troubleshooting.
#define I2C_TIMEOUT 10 // ms. A stuck bus (e.g. a disconnected sensor, the pull-ups are on its module) is then reported as an error instead of waiting forever.
#include <SoftI2CMaster.h>

namespace BH1750
{
    enum BH1750Address : byte
    {
        TO_GROUND = 0x23,
        TO_VCC = 0x5C
    };
    enum BH1750Opcode : byte
    {
        POWER_DOWN = 0b0000000,
        POWER_ON = 0b0000001,
        RESET = 0b0000111,
        CHANGE_MT_HIGH_MASK = 0b01000000,
        CHANGE_MT_LOW_MASK = 0b01100000,

        // Here are the options for measurement.
        // Continuous, as the name suggests, starts measuring repeatedly
        // until stopped. One time measurement on the other hand
        // switches the power off after measuring once.
        // Low-res mode is faster, but the resolution is worse
        //(~4lux for default config). Both high-res modes
        // are the same speed, where mode 2 provides twice the resolution
        //(~0.5lux for default config), but with twice narrower range.
        // Refer to datasheet for extensive explanation.
        SWITCH_TO_CONTINUOUS_HIGH_RES_MODE = 0b00010000,
        SWITCH_TO_CONTINUOUS_HIGH_RES_MODE_2 = 0b00010001,
        SWITCH_TO_CONTINUOUS_LOW_RES_MODE = 0b00010011,
        SWITCH_TO_ONE_TIME_HIGH_RES_MODE = 0b00100000,
        SWITCH_TO_ONE_TIME_HIGH_RES_MODE_2 = 0b00100001,
        SWITCH_TO_ONE_TIME_LOW_RES_MODE = 0b00100011
    };
    enum BH1750MTREG : byte
    {
        BH1750_MTREG_LOW = 31,
        BH1750_MTREG_HIGH = 254,
        BH1750_MTREG_DEFAULT = 69
    };

    // Change this section according to your needs.
    using lux_t = float;                                                              // If you want to make the code size about 1kB smaller and make it run faster, you can change this to some custom fixed decimal point datatype.
    static constexpr byte _address = BH1750Address::TO_GROUND;                        // Change this to your chip's I2C address.
    static constexpr byte _mode = BH1750Opcode::SWITCH_TO_CONTINUOUS_HIGH_RES_MODE_2; // Select mode from enum above.
    static constexpr byte _mtreg = 188;                                               // Define the sampling time of the sensor. Higher values increase resolution, but also the the time needed for measurement. Default is 69. It is recommended to stick in the range provided above.
    static constexpr lux_t luxFactor = 1.2;                                           // Fine-tunes the value. Use 1.2 unless you calibrate with proffesional tools.

    // Internal functions, do not change
    namespace
    {
        constexpr lux_t qualFactor = (_mode == SWITCH_TO_CONTINUOUS_HIGH_RES_MODE_2 || _mode == SWITCH_TO_ONE_TIME_HIGH_RES_MODE_2) ? 2 : 1;
        constexpr byte _addressRead = (_address << 1) | 0b00000001;
        constexpr byte _addressWrite = (_address << 1);
        // constexpr uint16_t maxRawValue = -1;

        // Reads value directly from sensor. This is NOT in lux, and needs to be converted. Returns -1 if the sensor did not respond.
        int32_t readRawValue()
        {
            byte buff[2];

            if (!i2c_start(_addressRead))
            {
                i2c_stop();
                return -1;
            }
            buff[0] = i2c_read(false);
            buff[1] = i2c_read(true);
            i2c_stop();
            if (!(_SFR_IO8(SCL_IN) & (1 << SCL_PIN))) // A read that timed out returns a partial byte, but SCL stays stuck low.
                return -1;

            return (uint16_t(buff[0]) << 8) | buff[1];
        }

        // Sends byte directly to the sensor. Internal funciton.
        bool writeByte(byte b)
        {
            bool errorlevel = true;
            errorlevel &= i2c_start(_addressWrite);
            errorlevel &= i2c_write(b);
            i2c_stop();
            return errorlevel;
        }
        // Changes the measurement time of the sensor, if needed. Internal function.
        bool writeMtreg()
        {
            constexpr uint8_t hiByteDefault = (BH1750_MTREG_DEFAULT >> 5) | CHANGE_MT_HIGH_MASK;
            constexpr uint8_t loByteDefault = (BH1750_MTREG_DEFAULT & 0b00011111) | CHANGE_MT_LOW_MASK;

            // Change sensitivity measurement time
            // We send two bytes: 3 Bits und 5 Bits, with a prefix.
            // High bit: 01000_MT[7,6,5]
            // Low bit:  011_MT[4,3,2,1,0]
            constexpr uint8_t hiByte = (_mtreg >> 5) | CHANGE_MT_HIGH_MASK;
            constexpr uint8_t loByte = (_mtreg & 0b00011111) | CHANGE_MT_LOW_MASK;
            bool result = true;
            if constexpr (hiByte != hiByteDefault)
                result &= writeByte(hiByte);
            if constexpr (loByte != loByteDefault)
                result &= writeByte(loByte);
            return result;
        }
        // Writes the measurement mode to the sensor which starts the measurement.
        bool writeMode()
        {
            return writeByte(_mode);
        }

        // Turn the device in low-power mode.
        bool powerDown()
        {
            return writeByte(POWER_DOWN);
        }
    }

    // Provides the longest time one measurement takes in this configuration, in ms (in compile time). The datasheet gives
    // 180 ms for the high resolution modes and 24 ms for the low resolution mode at the default MTREG, it scales with MTREG.
    constexpr unsigned long measurementTime()
    {
        return ((_mode & 0b11) == 0b11 ? 24ul : 180ul) * _mtreg / BH1750_MTREG_DEFAULT + 1; // +1 rounds up
    }

    // Provides the resolution in lux achieved using this configuration (in compile time).
    constexpr lux_t resolution()
    {
        return (BH1750_MTREG_DEFAULT / luxFactor) / (_mtreg * qualFactor);
    }

    // Calculates lux from raw sensor value. Compiler pre-computes everything so the device only has to multiply with one number.
    constexpr lux_t calcLux(uint16_t raw)
    {
        return raw * resolution();
    }

    // Provides the maximum value one can measure using this configuration (in compile time).
    constexpr lux_t absoluteMaximum()
    {
        return calcLux(-1);
    }

    // Initilizes the library. Needs to be called only once at the beginning.
    bool begin()
    {
        return i2c_init() && writeMtreg();
    }
    // Call this to start measuring.
    bool start()
    {
        return writeMode();
    }
    // Call this to stop measuring (useful only for continuous modes).
    bool stop()
    {
        return powerDown();
    }

    // Main function of the library. Call this to retrieve lux values from the sensor. Returns a negative value if the sensor did not respond.
    lux_t readLux()
    {
        int32_t raw = readRawValue();
        return raw < 0 ? -1 : calcLux(raw);
    }

}

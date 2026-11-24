# spark-als
This project contains open-source firmware for an USB HID Ambient Light Sensor (ALS) implementation using Digispark Attiny85-based microcontroller development board and BH1750FVI I2C light sensor module.

# Fork
This repository is a fork of `3cky/tiny-hid-als`. Their work inspired my project, which aims to improve on:

### Functionality
My project extends the functionality of the ALS sensor, for example introducing support for absolute and relative thresholding and reporting more information about the sensor to the driver.
### Adhering to standard
The original project did not follow Microsoft's requirements for ALS sensors, meaning there wasn't any guarantee that functionality such as automatic brightness will work. I tried to change that to the best of my understanding of the official sensor implementation guide ([here](https://learn.microsoft.com/en-us/windows-hardware/design/whitepapers/hid-sensors-usages), [here](https://learn.microsoft.com/en-us/windows-hardware/design/whitepapers/integrating-ambient-light-sensors-with-computers-running-windows-10-creators-update), [here](https://learn.microsoft.com/en-us/windows-hardware/design/component-guidelines/ambient-light-sensors) and [here](https://learn.microsoft.com/en-us/windows-hardware/drivers/sensors/light-sensor-thresholds)). Theoretically, the project should now pass WHQL certification.
### Resolution
My project increases the sampling resolution to about 0.15 lux. This increases the usability of the sensor in dark conditions, while also making it possible to detect very small fluctuations of light across the whole spectrum. It is possible to drive down the resolution to 0.11 lux, but this limits the dynamic range under the required 10,000 lux for auto brightness control in Windows.
### Code size and RAM usage
My project reduces the code size (it is around 17% smaller), while at the same time incorporating significantly more code logic. This is largely due to creating my own extremely lightweight library for communication with the sensor from the ground up, making use of `felias-fogg/SoftI2CMaster` more efficient I2C implementation instead of widely used Wire.h, and using zero-overhead constexpr functions for pre-calculating parts of formulas for conversion of values in compile time. The code size could potentially be reduced even further, for example by using fixed-point decimal library instead of floats, or focusing on rewriting main logic for code size, but there shouldn't be a need as it fits onto Attine85 with ease.
### Documentation
I tried to explain the functionality of the project in the code through comments. Extra attention is added to the new sensor library, making it easy to change parameters, if necessary.

# Issues
Please feel free to create an issue if you discover a bug, or discuss if you think I misinterpreted the standard. I tried to debug with all tools available to me, but this is my first bigger project implementing HID device and mistakes could have been made.

# Schematic

![Digispark connected to BH1750](https://github.com/MatejKocourek/spark-als/raw/main/doc/tiny-hid-als.png)

# OS support

USB HID sensors framework is supported out of the box since Linux 3.7 and Windows 8.

# Compiling and installing

First, please install [PlatformIO](http://platformio.org/) open source ecosystem for IoT development compatible with **Arduino** code and its command line tools (Windows, MacOs and Linux). Also, you may need to install [git](http://git-scm.com/) in your system. 

Note: with `platformIO` you don't need the Arduino IDE and install libraries, this will do it for you.

Compiling and installing:
``` bash
git clone --recurse-submodules https://github.com/MatejKocourek/spark-als.git
cd spark-als
pio run --target upload
```

V-USB is included as a git submodule in `external/v-usb`. If you cloned without `--recurse-submodules` or downloaded a ZIP archive, the build fetches it automatically.

## Optional features
Features that the standards do not require can be turned off to save flash, in `include/alsconfig.h` or with `build_flags = -D <NAME>=0` in `platformio.ini`:

| Define | Feature | Flash saved |
|---|---|---|
| `FEATURE_USB_SUSPEND` | Sleep while the USB bus is suspended (required by USB 2.0) | 116 B |
| `FEATURE_PERIPHERALS_OFF` | Turn off the unused peripherals | 10 B |
| `FEATURE_ABSOLUTE_SENSITIVITY` | Absolute (lux) change sensitivity, Windows requires only the relative one | 148 B |
| `FEATURE_RANGE_PROPERTIES` | Range maximum, range minimum and resolution properties | 62 B |
| `FEATURE_I2C_TIMEOUT` | Report a stuck I2C bus as an error instead of hanging | 34 B |

Note: you need connect your Digispark after each compiling for upload the new firmware or reset it. More info [here](http://digistump.com/wiki/digispark/tutorials/connectingpro).

## License
This project is distributed with GPL license, see [LICENSE](https://github.com/MatejKocourek/spark-als/blob/main/LICENSE) file for more informations.
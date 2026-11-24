/*
 * Optional features, which neither the HID sensor usages nor Microsoft's ambient light sensor requirements need.
 * Set one to 0 here, or with build_flags = -D <NAME>=0 in platformio.ini, to save flash.
 */
#ifndef ALSCONFIG_H
#define ALSCONFIG_H

// Sleeps while the host suspends the USB bus. USB 2.0 requires a suspended device to save power, so turning it off breaks USB compliance.
#ifndef FEATURE_USB_SUSPEND
#define FEATURE_USB_SUSPEND 1
#endif

// Turns off the unused ADC, analog comparator, Timer0 and USI to save power.
#ifndef FEATURE_PERIPHERALS_OFF
#define FEATURE_PERIPHERALS_OFF 1
#endif

// The absolute change sensitivity (lux threshold). Microsoft only requires the relative one.
#ifndef FEATURE_ABSOLUTE_SENSITIVITY
#define FEATURE_ABSOLUTE_SENSITIVITY 1
#endif

// Range Maximum, Range Minimum and Resolution properties, for information only.
#ifndef FEATURE_RANGE_PROPERTIES
#define FEATURE_RANGE_PROPERTIES 1
#endif

// Reports a stuck I2C bus (e.g. a disconnected sensor) as an error instead of hanging.
#ifndef FEATURE_I2C_TIMEOUT
#define FEATURE_I2C_TIMEOUT 1
#endif

#endif

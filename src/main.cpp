/*
 * Copyright (c) 2023 Victor Antonovich, Matej Kocourek
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <stddef.h>
#include <util/delay.h>

extern "C"
{
#include <usbdrv.h>
}
#include "HidSensorSpec.h"
#include "bh1750.h"

#define LED_OUT() pinMode(LED_BUILTIN, OUTPUT)
#define LED_OFF() digitalWrite(LED_BUILTIN, 0)
#define LED_ON() digitalWrite(LED_BUILTIN, 1)
#define LED_TOGGLE() digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN))

// These are linked with the exponent field in the descriptors. Scales the values. It basically allows us to send decimals, because floats are not supported.
static constexpr uint16_t exponentPercantage = 100u;
static constexpr uint32_t exponentLux = 10000u;

//We expect the sensor to definitely be done measuring after this time. With the highest precision, it is between 400-500, depends on sensor.
static constexpr unsigned long sensorMeasurementTime = 500u;

// HID Report Descriptor
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnarrowing" // V-USB declares the descriptor as char, but its bytes go up to 0xFF.
#pragma GCC diagnostic ignored "-Woverflow"
PROGMEM const char usbHidReportDescriptor[] = {
    HID_USAGE_PAGE_SENSOR,                    // USAGE_PAGE (Sensor)
    HID_USAGE_SENSOR_TYPE_LIGHT_AMBIENTLIGHT, // USAGE (AmbientLight)
    HID_COLLECTION(Application),

    // feature reports (xmit/receive)
    HID_USAGE_PAGE_SENSOR,
    // Reporting State - RW
    HID_USAGE_SENSOR_PROPERTY_REPORTING_STATE,
    HID_LOGICAL_MIN_8(1),
    HID_LOGICAL_MAX_8(6),
    HID_REPORT_SIZE(8),
    HID_REPORT_COUNT(1),
    HID_COLLECTION(Logical),
    HID_USAGE_SENSOR_PROPERTY_REPORTING_STATE_NO_EVENTS_SEL,
    HID_USAGE_SENSOR_PROPERTY_REPORTING_STATE_ALL_EVENTS_SEL,
    HID_USAGE_SENSOR_PROPERTY_REPORTING_STATE_THRESHOLD_EVENTS_SEL,
    HID_USAGE_SENSOR_PROPERTY_REPORTING_STATE_NO_EVENTS_WAKE_SEL,
    HID_USAGE_SENSOR_PROPERTY_REPORTING_STATE_ALL_EVENTS_WAKE_SEL,
    HID_USAGE_SENSOR_PROPERTY_REPORTING_STATE_THRESHOLD_EVENTS_WAKE_SEL,
    HID_FEATURE(Data_Arr_Abs),
    HID_END_COLLECTION,
    // Power State - RW
    HID_USAGE_SENSOR_PROPERTY_POWER_STATE,
    HID_LOGICAL_MIN_8(1),
    HID_LOGICAL_MAX_8(6),
    HID_REPORT_SIZE(8),
    HID_REPORT_COUNT(1),
    HID_COLLECTION(Logical),
    HID_USAGE_SENSOR_PROPERTY_POWER_STATE_UNDEFINED_SEL,
    HID_USAGE_SENSOR_PROPERTY_POWER_STATE_D0_FULL_POWER_SEL,
    HID_USAGE_SENSOR_PROPERTY_POWER_STATE_D1_LOW_POWER_SEL,
    HID_USAGE_SENSOR_PROPERTY_POWER_STATE_D2_STANDBY_WITH_WAKE_SEL,
    HID_USAGE_SENSOR_PROPERTY_POWER_STATE_D3_SLEEP_WITH_WAKE_SEL,
    HID_USAGE_SENSOR_PROPERTY_POWER_STATE_D4_POWER_OFF_SEL,
    HID_FEATURE(Data_Arr_Abs),
    HID_END_COLLECTION,
    // Report Interval - RW
    HID_USAGE_SENSOR_PROPERTY_REPORT_INTERVAL,
    HID_LOGICAL_MIN_8(0),
    HID_LOGICAL_MAX_32(0xFF, 0xFF, 0xFF, 0x7F),
    HID_REPORT_SIZE(32),
    HID_REPORT_COUNT(1),
    HID_UNIT_EXPONENT(0),
    HID_FEATURE(Data_Var_Abs),
    // Change Sensitivity Absolute - RW
    HID_USAGE_SENSOR_DATA(HID_USAGE_SENSOR_DATA_LIGHT_ILLUMINANCE,
                          HID_USAGE_SENSOR_DATA_MOD_CHANGE_SENSITIVITY_ABS),
    HID_LOGICAL_MIN_8(0),
    HID_LOGICAL_MAX_32(0xFF, 0xFF, 0xFF, 0x7F),
    HID_REPORT_SIZE(32),
    HID_REPORT_COUNT(1),
    HID_UNIT_EXPONENT(0x0C), // scale default unit to provide 4 digits past decimal point
    HID_FEATURE(Data_Var_Abs),
    // Change Sensitivity Relative - RW
    HID_USAGE_SENSOR_DATA(HID_USAGE_SENSOR_DATA_LIGHT_ILLUMINANCE,
                          HID_USAGE_SENSOR_DATA_MOD_CHANGE_SENSITIVITY_REL_PCT),
    HID_LOGICAL_MIN_8(0),
    HID_LOGICAL_MAX_16(0x10, 0x27), // 10000 = 0.00 to 100.00 percent with 2 digits past decimal point
    HID_REPORT_SIZE(16),
    HID_REPORT_COUNT(1),
    HID_UNIT_EXPONENT(0x0E), // scale default unit to provide 2 digits past decimal point
    HID_FEATURE(Data_Var_Abs),
    // Read-only fields follow, the host can only write the fields above.
    // Sensor Connection Type - RO
    HID_USAGE_SENSOR_PROPERTY_SENSOR_CONNECTION_TYPE, // NAry
    HID_LOGICAL_MIN_8(1),
    HID_LOGICAL_MAX_8(3),
    HID_REPORT_SIZE(8),
    HID_REPORT_COUNT(1),
    HID_COLLECTION(Logical),
    HID_USAGE_SENSOR_PROPERTY_CONNECTION_TYPE_PC_INTEGRATED_SEL,
    HID_USAGE_SENSOR_PROPERTY_CONNECTION_TYPE_PC_ATTACHED_SEL,
    HID_USAGE_SENSOR_PROPERTY_CONNECTION_TYPE_PC_EXTERNAL_SEL,
    HID_FEATURE(Data_Arr_Abs),
    HID_END_COLLECTION,
    // Sensor State - RO
    HID_USAGE_SENSOR_STATE,
    HID_LOGICAL_MIN_8(1),
    HID_LOGICAL_MAX_8(7),
    HID_REPORT_SIZE(8),
    HID_REPORT_COUNT(1),
    HID_COLLECTION(Logical),
    HID_USAGE_SENSOR_STATE_UNKNOWN_SEL,
    HID_USAGE_SENSOR_STATE_READY_SEL,
    HID_USAGE_SENSOR_STATE_NOT_AVAILABLE_SEL,
    HID_USAGE_SENSOR_STATE_NO_DATA_SEL,
    HID_USAGE_SENSOR_STATE_INITIALIZING_SEL,
    HID_USAGE_SENSOR_STATE_ACCESS_DENIED_SEL,
    HID_USAGE_SENSOR_STATE_ERROR_SEL,
    HID_FEATURE(Data_Arr_Abs),
    HID_END_COLLECTION,
    // Minimum Report Interval  -  RO
    HID_USAGE_SENSOR_PROPERTY_MINIMUM_REPORT_INTERVAL,
    HID_LOGICAL_MIN_8(0),
    HID_LOGICAL_MAX_16(0xFF, 0x00),
    HID_REPORT_SIZE(8),
    HID_REPORT_COUNT(1),
    HID_UNIT_EXPONENT(0),
    HID_FEATURE(Data_Var_Abs),
    // Range Maximum - RO
    HID_USAGE_SENSOR_DATA(HID_USAGE_SENSOR_DATA_LIGHT_ILLUMINANCE,
                          HID_USAGE_SENSOR_DATA_MOD_MAX),
    HID_LOGICAL_MIN_8(0),
    HID_LOGICAL_MAX_32(0xFF, 0xFF, 0xFF, 0x7F),
    HID_REPORT_SIZE(32),
    HID_REPORT_COUNT(1),
    HID_UNIT_EXPONENT(0x0C), // scale default unit to provide 4 digits past decimal point
    HID_FEATURE(Data_Var_Abs),
    // Range Minimum - RO
    HID_USAGE_SENSOR_DATA(HID_USAGE_SENSOR_DATA_LIGHT_ILLUMINANCE,
                          HID_USAGE_SENSOR_DATA_MOD_MIN),
    HID_LOGICAL_MIN_8(0),
    HID_LOGICAL_MAX_16(0xFF, 0x00),
    HID_REPORT_SIZE(8),
    HID_REPORT_COUNT(1),
    HID_UNIT_EXPONENT(0x0C), // scale default unit to provide 4 digits past decimal point
    HID_FEATURE(Data_Var_Abs),
    // Resolution - RO
    HID_USAGE_SENSOR_DATA(HID_USAGE_SENSOR_DATA_LIGHT_ILLUMINANCE,
                          HID_USAGE_SENSOR_DATA_MOD_RESOLUTION),
    HID_LOGICAL_MIN_8(0),
    HID_LOGICAL_MAX_32(0xFF, 0xFF, 0xFF, 0x7F),
    HID_REPORT_SIZE(32),
    HID_REPORT_COUNT(1),
    HID_UNIT_EXPONENT(0x0C), // scale default unit to provide 4 digits past decimal point
    HID_FEATURE(Data_Var_Abs),

    // input reports (transmit)
    HID_USAGE_PAGE_SENSOR,
    // Sensor states
    HID_USAGE_SENSOR_STATE,
    HID_LOGICAL_MIN_8(1),
    HID_LOGICAL_MAX_8(7),
    HID_REPORT_SIZE(8),
    HID_REPORT_COUNT(1),
    HID_COLLECTION(Logical),
    HID_USAGE_SENSOR_STATE_UNKNOWN_SEL,
    HID_USAGE_SENSOR_STATE_READY_SEL,
    HID_USAGE_SENSOR_STATE_NOT_AVAILABLE_SEL,
    HID_USAGE_SENSOR_STATE_NO_DATA_SEL,
    HID_USAGE_SENSOR_STATE_INITIALIZING_SEL,
    HID_USAGE_SENSOR_STATE_ACCESS_DENIED_SEL,
    HID_USAGE_SENSOR_STATE_ERROR_SEL,
    HID_INPUT(Data_Arr_Abs),
    HID_END_COLLECTION,
    // Sensor Events
    HID_USAGE_SENSOR_EVENT,
    HID_LOGICAL_MIN_8(1),
    HID_LOGICAL_MAX_8(6),
    HID_REPORT_SIZE(8),
    HID_REPORT_COUNT(1),
    HID_COLLECTION(Logical),
    HID_USAGE_SENSOR_EVENT_UNKNOWN_SEL,
    HID_USAGE_SENSOR_EVENT_STATE_CHANGED_SEL,
    HID_USAGE_SENSOR_EVENT_PROPERTY_CHANGED_SEL,
    HID_USAGE_SENSOR_EVENT_DATA_UPDATED_SEL,
    HID_USAGE_SENSOR_EVENT_POLL_RESPONSE_SEL,
    HID_USAGE_SENSOR_EVENT_CHANGE_SENSITIVITY_SEL,
    HID_INPUT(Data_Arr_Abs),
    HID_END_COLLECTION,
    // Sensor data
    HID_USAGE_SENSOR_DATA_LIGHT_ILLUMINANCE,
    HID_LOGICAL_MIN_8(0),
    HID_LOGICAL_MAX_32(0xFF, 0xFF, 0xFF, 0x7F),
    HID_UNIT_EXPONENT(0x0C), // scale default unit to provide 4 digits past decimal point
    HID_REPORT_SIZE(32),
    HID_REPORT_COUNT(1),
    HID_INPUT(Data_Var_Abs),

    HID_END_COLLECTION};

// Configuration descriptor, the same as V-USB's but without the remote wakeup attribute, which this device does not support.
PROGMEM const char usbDescriptorConfiguration[] = {
    9, USBDESCR_CONFIG, 34, 0, 1, 1, 0, 1 << 7, USB_CFG_MAX_BUS_POWER / 2,
    9, USBDESCR_INTERFACE, 0, 0, 1, USB_CFG_INTERFACE_CLASS, USB_CFG_INTERFACE_SUBCLASS, USB_CFG_INTERFACE_PROTOCOL, 0,
    9, USBDESCR_HID, 0x01, 0x01, 0x00, 0x01, 0x22, USB_CFG_HID_REPORT_DESCRIPTOR_LENGTH & 0xFF, USB_CFG_HID_REPORT_DESCRIPTOR_LENGTH >> 8,
    7, USBDESCR_ENDPOINT, 0x81, 0x03, 8, 0, USB_CFG_INTR_POLL_INTERVAL};
#pragma GCC diagnostic pop
static_assert(sizeof(usbDescriptorConfiguration) == USB_CFG_DESCR_PROPS_CONFIGURATION, "Update USB_CFG_DESCR_PROPS_CONFIGURATION in usbconfig.h");

static_assert(sizeof(usbHidReportDescriptor) == USB_CFG_HID_REPORT_DESCRIPTOR_LENGTH, "Update USB_CFG_HID_REPORT_DESCRIPTOR_LENGTH in usbconfig.h");

// Converts the floating point value into fixed point decimal.
static constexpr uint32_t convertLux(float luxSource)
{
  return luxSource * exponentLux;
}

// Feature Report buffer
struct FeatureReport
{
  // common properties
  uint8_t reportingState;             // HID_USAGE_SENSOR_PROPERTY_REPORTING_STATE
  uint8_t powerState;                 // HID_USAGE_SENSOR_PROPERTY_POWER_STATE
  uint32_t reportInterval;            // HID_USAGE_SENSOR_PROPERTY_REPORT_INTERVAL
  uint32_t lightChangeSensitivity;    // HID_USAGE_SENSOR_PROPERTY_CHANGE_SENSITIVITY_ABS
  uint16_t lightChangeSensitivityRel; // HID_USAGE_SENSOR_PROPERTY_CHANGE_SENSITIVITY_REL_PCT
  // read-only properties, the host writes only the fields above
  uint8_t connectionType;             // HID_USAGE_SENSOR_PROPERTY_SENSOR_CONNECTION_TYPE
  uint8_t sensorState;                // HID_USAGE_SENSOR_STATE
  uint8_t minReportInterval;          // HID_USAGE_SENSOR_PROPERTY_MINIMUM_REPORT_INTERVAL
  uint32_t rangeMax;                  // HID_USAGE_SENSOR_PROPERTY_RANGE_MAXIMUM
  uint8_t rangeMin;                   // HID_USAGE_SENSOR_PROPERTY_RANGE_MINIMUM
  uint32_t resolution;                // HID_USAGE_SENSOR_DATA_MOD_RESOLUTION
} featureReportBuf = {
    HID_USAGE_SENSOR_PROPERTY_REPORTING_STATE_NO_EVENTS_SEL_ENUM,   // Computer changes this to let the sensor know if it should send regular reports or not.
    HID_USAGE_SENSOR_PROPERTY_POWER_STATE_D4_POWER_OFF_ENUM,        // Computer changes this to turn the device into low-power state.
    500,                                                            // Default report interval. Driver usually ignores this and sets this field to the minimum allowed.
    convertLux(0),                                                  // Default absolute sensitivity for reporting (in lux). Computer changes this (and/or the relative counterpart) to limit the amount of reports it gets, to not overwhelm the bus.
    0,                                                              // Default relative sensitivity for reporting (in percantage, eg. 50 is 50%).
    HID_USAGE_SENSOR_PROPERTY_CONNECTION_TYPE_PC_ATTACHED_SEL_ENUM, // How the device is physically placed relative to the computer. Does not change.
    HID_USAGE_SENSOR_STATE_NO_DATA_SEL_ENUM,                        // Sensor changes this to let the computer know about the current state of the sensor.
    250,                                                            // Min. report interval. To comply with standard for ALS, must be <=250. Does not change.
    convertLux(BH1750::absoluteMaximum()),                          // Max. range of the sensor. The library provides it as constexpr. Does not change.
    convertLux(0),                                                  // Min. range of the sensor, is always 0. Does not change.
    convertLux(BH1750::resolution())};                              // The smallest step the sensor can differentiate. The library provides it as constexpr. For information only. Does not change.

// Input Report buffer. Contents of this are sent regularly to provide the current sensor values.
struct
{
  // common values
  uint8_t sensorState; // HID_USAGE_SENSOR_STATE
  uint8_t eventType;   // HID_USAGE_SENSOR_EVENT

  // values specific to this sensor
  uint32_t lightValue; // HID_USAGE_SENSOR_TYPE_LIGHT_AMBIENTLIGHT
} inputReportBuf = {
    HID_USAGE_SENSOR_STATE_NO_DATA_SEL_ENUM, // The same field as in the feature report. To comply with standard, has to always contain the same value.
    HID_USAGE_SENSOR_EVENT_UNKNOWN_SEL_ENUM, // The reason the current report is being sent.
    0,                                       // The main value of the sensor, in lux (scaled, as the rest).
};

// Input reports waiting for the interrupt endpoint to be free. Handing V-USB a new report before the previous one was sent would overwrite it.
bool stateChangePending = false;
uint8_t pendingDataEvent = 0; // 0 if no data report is waiting.

static void sendPendingReport()
{
  if (!usbInterruptIsReady())
    return;
  if (inputReportBuf.sensorState == HID_USAGE_SENSOR_STATE_ERROR_SEL_ENUM) // The sensor failed, a sample waiting from before is not sent.
    pendingDataEvent = 0;
  uint8_t event = pendingDataEvent;
  if (stateChangePending) // A data report waiting behind it follows as its own event.
  {
    stateChangePending = false;
    event = HID_USAGE_SENSOR_EVENT_STATE_CHANGED_SEL_ENUM;
  }
  else
    pendingDataEvent = 0;
  if (event)
  {
    inputReportBuf.eventType = event;
    usbSetInterrupt((uchar *)&inputReportBuf, sizeof(inputReportBuf));
  }
}

// If the I2C fails to cummunicate, the code falls into this. If that happens, check the connection of the sensor and if the I2C pin macros are configured correctly in bh1750.h
static void error()
{
  LED_ON();
  inputReportBuf.sensorState = HID_USAGE_SENSOR_STATE_ERROR_SEL_ENUM;
  featureReportBuf.sensorState = HID_USAGE_SENSOR_STATE_ERROR_SEL_ENUM;
  stateChangePending = true;
  while (true)
  {
    usbPoll();
    sendPendingReport();
  }
}

void setup()
{
  // Turn off onboard LED
  LED_OUT();
  LED_OFF();

  // Turn off unused peripherals to save power. Timer1 is used by millis().
  ADCSRA = 0;
  ACSR = 1 << ACD;
  PRR = (1 << PRTIM0) | (1 << PRUSI) | (1 << PRADC);

  noInterrupts();
  usbInit();
  usbDeviceDisconnect(); /* enforce re-enumeration */

  byte timeoutCounter = 0;
  do
  { /* fake USB disconnect for > 250 ms */
    _delay_ms(1);
  } while (--timeoutCounter);
  usbDeviceConnect();
  interrupts();

  if (!BH1750::begin())
    error();
}

unsigned long timeout = 0;         // Contains the time when the intializing sensor will be done with the first measurement.
unsigned long nextReportTime = 0;  // When should the next report be sent to the computer.
bool running = false;              // If the sensor is currently sampling data.
bool featureReportChanged = false; // If the computer changed the feature report.
float previousLux = 0;             // For the sensitivty functionality.

// Checks if the given time has passed, also when millis() overflows (every ~49 days).
static bool timeReached(unsigned long now, unsigned long time)
{
  return long(now - time) >= 0;
}


// Computer changed the feature report, restart everything.
static void featureReportReload(unsigned long now)
{
  if (featureReportBuf.reportInterval < featureReportBuf.minReportInterval)
    featureReportBuf.reportInterval = featureReportBuf.minReportInterval;
  nextReportTime = now + featureReportBuf.reportInterval; // Set on every change so it can never be older than the overflow-safe comparison allows.
  previousLux = -INFINITY;                                // Passes both thresholds, so the next measurement is always reported.

  switch (featureReportBuf.powerState) // What the computer tells us the required power mode is.
  {
  case HID_USAGE_SENSOR_PROPERTY_POWER_STATE_D0_FULL_POWER_ENUM: // Full power means sampling AND reporting to the computer in regular intervals.
  case HID_USAGE_SENSOR_PROPERTY_POWER_STATE_D1_LOW_POWER_ENUM: // Low power means sampling, but not reporting unless the computer explicitly asks for the value.
    // The sensor has to be running in these two modes.
    {
      if (!running)
      {
        if (!BH1750::start())
          error();
        timeout = now + sensorMeasurementTime;
        running = true;
        inputReportBuf.sensorState = HID_USAGE_SENSOR_STATE_INITIALIZING_SEL_ENUM;
        featureReportBuf.sensorState = HID_USAGE_SENSOR_STATE_INITIALIZING_SEL_ENUM;
        stateChangePending = true;
      }
    }
    break;
  case HID_USAGE_SENSOR_PROPERTY_POWER_STATE_D2_STANDBY_WITH_WAKE_ENUM:
  case HID_USAGE_SENSOR_PROPERTY_POWER_STATE_D3_SLEEP_WITH_WAKE_ENUM:
  case HID_USAGE_SENSOR_PROPERTY_POWER_STATE_D4_POWER_OFF_ENUM: // The computer tells us the sensor is not needed at all and should conserve power. When no application uses our sensor, this will be its state.
  {
    if (running)
    {
      if (!BH1750::stop())
        error();
      running = false;
      inputReportBuf.sensorState = HID_USAGE_SENSOR_STATE_NO_DATA_SEL_ENUM;
      featureReportBuf.sensorState = HID_USAGE_SENSOR_STATE_NO_DATA_SEL_ENUM;
      stateChangePending = true;
    }
  }
  break;
  default:
    break;
  }

  // LED_ON();
  //_delay_ms(50);
  // LED_OFF();
}

// Checks if the computer wants data reports. The *_WAKE variants only add waking up the computer, which this device does not support.
static bool reportingEnabled()
{
  switch (featureReportBuf.reportingState)
  {
  case HID_USAGE_SENSOR_PROPERTY_REPORTING_STATE_ALL_EVENTS_SEL_ENUM:
  case HID_USAGE_SENSOR_PROPERTY_REPORTING_STATE_THRESHOLD_EVENTS_SEL_ENUM:
  case HID_USAGE_SENSOR_PROPERTY_REPORTING_STATE_ALL_EVENTS_SEL_WAKE_ENUM:
  case HID_USAGE_SENSOR_PROPERTY_REPORTING_STATE_THRESHOLD_EVENTS_WAKE_SEL_ENUM:
    return true;
  default:
    return false;
  }
}

// Checks if our new value has broken out of the threshold the computer set, in lux.
static bool absoluteSensitivityReached(const float &newVal)
{
  float absoluteStep = float(featureReportBuf.lightChangeSensitivity) / exponentLux;
  float absoluteThresholdHigh = previousLux + absoluteStep;
  float absoluteThresholdLow = previousLux - absoluteStep;

  return (newVal <= absoluteThresholdLow || newVal >= absoluteThresholdHigh);
}
// Checks if our new value has broken out of the threshold the computer set, in percantage of the old value.
static bool relativeSensitivityReached(const float &newVal)
{
  float percentage = float(featureReportBuf.lightChangeSensitivityRel) / (100.0 * exponentPercantage);
  float relativeThresholdLow = previousLux * (1u - percentage);
  float relativeThresholdHigh = previousLux * (1u + percentage);

  return (newVal <= relativeThresholdLow || newVal >= relativeThresholdHigh);
}

void loop()
{
  usbPoll(); // Needs to be called often to keep connection with the computer.
  sendPendingReport();
  unsigned long now = millis();

  if (inputReportBuf.sensorState == HID_USAGE_SENSOR_STATE_ERROR_SEL_ENUM) // Sensor failed while answering a GET_REPORT, which cannot block in error().
    error();

  if(featureReportChanged)
  {
    featureReportReload(now);
    featureReportChanged = false;
    return;
  }

  if (inputReportBuf.sensorState == HID_USAGE_SENSOR_STATE_INITIALIZING_SEL_ENUM) // Only after wake up. We cannot report because we are waiting for the first measurement from the sensor.
  {
    if (!timeReached(now, timeout))
      return;
    inputReportBuf.sensorState = HID_USAGE_SENSOR_STATE_READY_SEL_ENUM;
    featureReportBuf.sensorState = HID_USAGE_SENSOR_STATE_READY_SEL_ENUM;
    stateChangePending = true;
    nextReportTime = now; // The initial sample is reported right after the sensor becomes ready.
  }

  if (running && reportingEnabled() && timeReached(now, nextReportTime)) // When it's a time for a report and the computer wants us reporting.
  {
    float lux = BH1750::readLux();
    if (lux < 0)
      error();
    bool streaming = featureReportBuf.lightChangeSensitivity == 0 && featureReportBuf.lightChangeSensitivityRel == 0; // Thresholds not set, every sample is reported.
    if (streaming || (lux != previousLux && relativeSensitivityReached(lux) && absoluteSensitivityReached(lux))) // Otherwise both thresholds need to be met and an unchanged value (e.g. in darkness) is not reported.
    {
      previousLux = lux;
      inputReportBuf.lightValue = convertLux(lux);
      pendingDataEvent = streaming ? HID_USAGE_SENSOR_EVENT_DATA_UPDATED_SEL_ENUM : HID_USAGE_SENSOR_EVENT_CHANGE_SENSITIVITY_SEL_ENUM;
    }
    nextReportTime = now + featureReportBuf.reportInterval;
  }
  //_delay_ms(2);
}

uint8_t writeOffset;        // Position in the feature report the host is writing.
usbMsgLen_t bytesRemaining; // Of the whole transfer, including the read-only fields.

#ifdef __cplusplus
extern "C"
{
#endif
  usbMsgLen_t usbFunctionSetup(uchar data[8])
  {
    usbRequest_t *rq = (usbRequest_t *)data;
    if ((rq->bmRequestType & USBRQ_TYPE_MASK) == USBRQ_TYPE_CLASS) /* class request type */
    {
      switch (rq->bRequest)
      {
      case USBRQ_HID_GET_REPORT:
      {
        /* wValue: ReportType (highbyte), ReportID (lowbyte) */
        switch (rq->wValue.bytes[1])
        {
        case 0x03: /* Get Feature report */
        {
          usbMsgPtr = (uchar *)&featureReportBuf;
          return sizeof(featureReportBuf);
        }
        break;
        case 0x01: /* Get Input report */ // This is the computer explicitly asking for current value of the sensor. Does not usually happen after initilization, but it is allowed.
        {
          inputReportBuf.eventType = HID_USAGE_SENSOR_EVENT_POLL_RESPONSE_SEL_ENUM;
          if (inputReportBuf.sensorState == HID_USAGE_SENSOR_STATE_READY_SEL_ENUM) // Otherwise the sensor has no finished measurement, and the last valid value is returned.
          {
            float lux = BH1750::readLux();
            if (lux < 0)
              inputReportBuf.sensorState = HID_USAGE_SENSOR_STATE_ERROR_SEL_ENUM; // loop() calls error()
            else
              inputReportBuf.lightValue = convertLux(lux);
          }
          usbMsgPtr = (uchar *)&inputReportBuf;
          return sizeof(inputReportBuf);
        }
        break;
        default:
          break;
        }
      }
      break;
      case USBRQ_HID_SET_REPORT:
      {
        switch (rq->wValue.bytes[1])
        {
        case 0x03: /* Set Feature report */
        {
          writeOffset = 0;
          bytesRemaining = rq->wLength.word;
          return USB_NO_MSG; /* Use usbFunctionWrite() to get data from host */
        }
        break;
        default:
          break;
        }
      }
      break;
      default:
        break;
      }
    }
    return 0; /* default for not implemented requests: return no data back to host */ // and ignores the input
  }

  // This function changes our feature report, and is called from V-USB library after we return USB_NO_MSG from the function above.
  uchar usbFunctionWrite(uchar *data, uchar len)
  {
    if (len > bytesRemaining)
      len = bytesRemaining;
    for (uint8_t i = 0; i < len; ++i)
      if (writeOffset < offsetof(FeatureReport, connectionType)) // Read-only fields are not written.
        ((uint8_t *)&featureReportBuf)[writeOffset++] = data[i];
    bytesRemaining -= len; // The transfer ends with its last packet, V-USB would pass any further ones here too.
    bool isEnd = bytesRemaining == 0;
    if (isEnd)
    {
      featureReportChanged = true;
      pendingDataEvent = 0; // A sample still waiting for the endpoint was taken under the old settings, which may not allow reporting. Dropped here, it could be sent before the settings are reloaded.
    }
    return (uchar)isEnd; /* return 1 if this was the last chunk */
  }
#ifdef __cplusplus
} // extern "C"
#endif

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

#include <util/delay.h>

#include <usbdrv.h>
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
PROGMEM const uchar usbHidReportDescriptor[] = {
    HID_USAGE_PAGE_SENSOR,                    // USAGE_PAGE (Sensor)
    HID_USAGE_SENSOR_TYPE_LIGHT_AMBIENTLIGHT, // USAGE (AmbientLight)
    HID_COLLECTION(Application),

    // feature reports (xmit/receive)
    HID_USAGE_PAGE_SENSOR,
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
    // Sensor State - RW
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
    // Report Interval - RW
    HID_USAGE_SENSOR_PROPERTY_REPORT_INTERVAL,
    HID_LOGICAL_MIN_8(0),
    HID_LOGICAL_MAX_32(0xFF, 0xFF, 0xFF, 0x7F),
    HID_REPORT_SIZE(32),
    HID_REPORT_COUNT(1),
    HID_UNIT_EXPONENT(0),
    HID_FEATURE(Data_Var_Abs),
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

    // Resolution
    HID_USAGE_SENSOR_DATA(HID_USAGE_SENSOR_DATA_LIGHT_ILLUMINANCE,
                          HID_USAGE_SENSOR_DATA_MOD_RESOLUTION),
    HID_LOGICAL_MIN_8(0),
    HID_LOGICAL_MAX_32(0xFF, 0xFF, 0xFF, 0x7F),
    HID_REPORT_SIZE(32),
    HID_REPORT_COUNT(1),
    HID_UNIT_EXPONENT(0x0C), // scale default unit to provide 4 digits past decimal point
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

    // Change Sensitivity Relative
    HID_USAGE_SENSOR_DATA(HID_USAGE_SENSOR_DATA_LIGHT_ILLUMINANCE,
                          HID_USAGE_SENSOR_DATA_MOD_CHANGE_SENSITIVITY_REL_PCT),
    HID_LOGICAL_MIN_8(0),
    HID_LOGICAL_MAX_16(0x10, 0x27), // 10000 = 0.00 to 100.00 percent with 2 digits past decimal point
    HID_REPORT_SIZE(16),
    HID_REPORT_COUNT(1),
    HID_UNIT_EXPONENT(0x0E), // scale default unit to provide 2 digits past decimal point
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

static_assert(sizeof(usbHidReportDescriptor) == USB_CFG_HID_REPORT_DESCRIPTOR_LENGTH, "Update USB_CFG_HID_REPORT_DESCRIPTOR_LENGTH in usbconfig.h");

// Converts the floating point value into fixed point decimal.
static constexpr uint32_t convertLux(float luxSource)
{
  return luxSource * exponentLux;
}

// Feature Report buffer
struct
{
  // common properties
  uint8_t connectionType;             // HID_USAGE_SENSOR_PROPERTY_SENSOR_CONNECTION_TYPE
  uint8_t reportingState;             // HID_USAGE_SENSOR_PROPERTY_REPORTING_STATE
  uint8_t powerState;                 // HID_USAGE_SENSOR_PROPERTY_POWER_STATE
  uint8_t sensorState;                // HID_USAGE_SENSOR_STATE
  uint32_t reportInterval;            // HID_USAGE_SENSOR_PROPERTY_REPORT_INTERVAL
  uint8_t minReportInterval;          // HID_USAGE_SENSOR_PROPERTY_MINIMUM_REPORT_INTERVAL
  uint32_t rangeMax;                  // HID_USAGE_SENSOR_PROPERTY_RANGE_MAXIMUM
  uint8_t rangeMin;                   // HID_USAGE_SENSOR_PROPERTY_RANGE_MINIMUM
  uint32_t resolution;                // HID_USAGE_SENSOR_DATA_MOD_RESOLUTION
  uint32_t lightChangeSensitivity;    // HID_USAGE_SENSOR_PROPERTY_CHANGE_SENSITIVITY_ABS
  uint16_t lightChangeSensitivityRel; // HID_USAGE_SENSOR_PROPERTY_CHANGE_SENSITIVITY_REL_PCT
} featureReportBuf = {
    HID_USAGE_SENSOR_PROPERTY_CONNECTION_TYPE_PC_ATTACHED_SEL_ENUM, // How the device is physically placed relative to the computer. Does not change.
    HID_USAGE_SENSOR_PROPERTY_REPORTING_STATE_NO_EVENTS_SEL_ENUM,   // Computer changes this to let the sensor know if it should send regular reports or not.
    HID_USAGE_SENSOR_PROPERTY_POWER_STATE_D4_POWER_OFF_ENUM,        // Computer changes this to turn the device into low-power state.
    HID_USAGE_SENSOR_STATE_NO_DATA_SEL_ENUM,                        // Sensor changes this to let the computer know about the current state of the sensor.
    500,                                                            // Default report interval. Driver usually ignores this and sets this field to the minimum allowed.
    250,                                                            // Min. report interval. To comply with standard for ALS, must be <=250. Does not change.
    convertLux(BH1750::absoluteMaximum()),                          // Max. range of the sensor. The library provides it as constexpr. Does not change.
    convertLux(0),                                                  // Min. range of the sensor, is always 0. Does not change.
    convertLux(BH1750::resolution()),                               // The smallest step the sensor can differentiate. The library provides it as constexpr. For information only. Does not change.
    convertLux(0),                                                  // Default absolute sensitivity for reporting (in lux). Computer changes this (and/or the relative counterpart) to limit the amount of reports it gets, to not overwhelm the bus.
    0};                                                             // Default relative sensitivity for reporting (in percantage, eg. 50 is 50%).

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

// If the I2C fails to cummunicate, the code falls into this. If that happens, check the connection of the sensor and if the I2C pin macros are configured correctly in bh1750.h
static void error()
{
  LED_ON();
  inputReportBuf.eventType = HID_USAGE_SENSOR_EVENT_STATE_CHANGED_SEL_ENUM;
  inputReportBuf.sensorState = HID_USAGE_SENSOR_STATE_ERROR_SEL_ENUM;
  featureReportBuf.sensorState = HID_USAGE_SENSOR_STATE_ERROR_SEL_ENUM;
  usbSetInterrupt((uchar *)&inputReportBuf, sizeof(inputReportBuf));
  while (true)
    usbPoll();
}

void setup()
{
  // Turn off onboard LED
  LED_OUT();
  LED_OFF();

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


// Computer changed the feature report, restart everything.
static void featureReportReload()
{
  if (featureReportBuf.reportInterval < featureReportBuf.minReportInterval)
    featureReportBuf.reportInterval = featureReportBuf.minReportInterval;

  switch (featureReportBuf.powerState) // What the computer tells us the required power mode is.
  {
  case HID_USAGE_SENSOR_PROPERTY_POWER_STATE_D0_FULL_POWER_ENUM: // Full power means sampling AND reporting to the computer in regular intervals.
    nextReportTime = millis() + featureReportBuf.reportInterval;
    inputReportBuf.lightValue = 0;
    previousLux = 0;
  case HID_USAGE_SENSOR_PROPERTY_POWER_STATE_D1_LOW_POWER_ENUM: // Low power means sampling, but not reporting unless the computer explicitly asks for the value.
    // The sensor has to be running in these two modes.
    {
      if (!running)
      {
        if (!BH1750::start())
          error();
        timeout = millis() + sensorMeasurementTime;
        running = true;
        inputReportBuf.sensorState = HID_USAGE_SENSOR_STATE_INITIALIZING_SEL_ENUM;
        featureReportBuf.sensorState = HID_USAGE_SENSOR_STATE_INITIALIZING_SEL_ENUM;
        inputReportBuf.eventType = HID_USAGE_SENSOR_EVENT_STATE_CHANGED_SEL_ENUM;
        usbSetInterrupt((uchar *)&inputReportBuf, sizeof(inputReportBuf));
      }
    }
    break;
  case HID_USAGE_SENSOR_PROPERTY_POWER_STATE_D4_POWER_OFF_ENUM: // The computer tells us the sensor is not needed at all and should conserve power. When no application uses our sensor, this will be its state.
  {
    if (running)
    {
      if (!BH1750::stop())
        error();
      running = false;
      inputReportBuf.sensorState = HID_USAGE_SENSOR_STATE_NO_DATA_SEL_ENUM;
      featureReportBuf.sensorState = HID_USAGE_SENSOR_STATE_NO_DATA_SEL_ENUM;
      inputReportBuf.eventType = HID_USAGE_SENSOR_EVENT_STATE_CHANGED_SEL_ENUM;
      usbSetInterrupt((uchar *)&inputReportBuf, sizeof(inputReportBuf));
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

// Checks if the sensor finished initiliazing, and lets the computer know if that is the case.
bool checkUpdateState()
{
  if (millis() >= timeout)
  {
    if (inputReportBuf.sensorState != HID_USAGE_SENSOR_STATE_READY_SEL_ENUM)
    {
      inputReportBuf.sensorState = HID_USAGE_SENSOR_STATE_READY_SEL_ENUM;
      featureReportBuf.sensorState = HID_USAGE_SENSOR_STATE_READY_SEL_ENUM;
      inputReportBuf.eventType = HID_USAGE_SENSOR_EVENT_STATE_CHANGED_SEL_ENUM;
      usbSetInterrupt((uchar *)&inputReportBuf, sizeof(inputReportBuf));
    }
    // LED_OFF();
    return true;
  }
  // LED_ON();
  return false;
}

void loop()
{
  usbPoll(); // Needs to be called often to keep connection with the computer.

  if(featureReportChanged)
  {
    featureReportReload();
    featureReportChanged = false;
    return;
  }

  if (inputReportBuf.sensorState == HID_USAGE_SENSOR_STATE_INITIALIZING_SEL_ENUM) // Only after wake up. We cannot report because we are waiting for the first measurement from the sensor.
  {
    checkUpdateState();
  }
  else if (featureReportBuf.reportingState != HID_USAGE_SENSOR_PROPERTY_REPORTING_STATE_NO_EVENTS_SEL_ENUM && (millis() >= nextReportTime)) // When it's a time for a report and the computer wants us reporting.
  {
    float lux = BH1750::readLux();
    // uint32_t convertedLux = convertLux(lux);
    // if (convertedLux != inputReportBuf.lightValue) // If the value is the same as last time, it should not be sent -> OR DOES IT? NEEDS INVESTIGATING, DOCUMENTATION NOT CLEAR
    if (relativeSensitivityReached(lux) && absoluteSensitivityReached(lux)) // Checks if the value is in the threshold. When both thresholds are set, both needs to be met.
    {
      previousLux = lux;
      inputReportBuf.lightValue = convertLux(lux);
      if (featureReportBuf.lightChangeSensitivity == 0 && featureReportBuf.lightChangeSensitivityRel == 0) // Thresholds not set, we should report every change.
        inputReportBuf.eventType = HID_USAGE_SENSOR_EVENT_DATA_UPDATED_SEL_ENUM; // The report is sent because the value changed and no thresholds are set.
      else
        inputReportBuf.eventType = HID_USAGE_SENSOR_EVENT_CHANGE_SENSITIVITY_SEL_ENUM; // The report is sent because the thresholds are met.

      inputReportBuf.sensorState = HID_USAGE_SENSOR_STATE_READY_SEL_ENUM;
      featureReportBuf.sensorState = HID_USAGE_SENSOR_STATE_READY_SEL_ENUM;
      usbSetInterrupt((uchar *)&inputReportBuf, sizeof(inputReportBuf)); // Send it
    }
    nextReportTime = millis() + featureReportBuf.reportInterval;
  }
  //_delay_ms(2);
}

uint8_t *writeBuf;
uint8_t bytesRemaining;

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
          inputReportBuf.lightValue = convertLux(BH1750::readLux());
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
          writeBuf = (uchar *)&featureReportBuf;
          bytesRemaining = min(rq->wLength.word, sizeof(featureReportBuf));
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
    for (uint8_t timeoutCounter = 0; timeoutCounter < len; ++timeoutCounter)
      writeBuf[timeoutCounter] = data[timeoutCounter];
    writeBuf += len;
    bytesRemaining -= len;
    bool isEnd = bytesRemaining == 0;
    if (isEnd)
      featureReportChanged = true;
    return (uchar)isEnd; /* return 1 if this was the last chunk */
  }
#ifdef __cplusplus
} // extern "C"
#endif

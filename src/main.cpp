/* 
CODE FOR ESP32. 
Acts as a peripheral module that sends noise level data via CAN bus when requested by gateway node via HEARTBEAT_REQUEST.
Sends HEARTBEAT_RESPONSE.
*/

#include <Arduino.h>
#include <driver/twai.h>
#include "can_common.h"
#include "outgoing.h"
#include "incoming.h"
#include "data_buffer.h"
#include "sensor.h"

#define DEBUG_MODE 1
#define SENSOR_MOCK 0 // set to 1 to use mock sensor readings, set to 0 to use real sensor readings from SoundSensor class
#define ENABLE_ACK 1 

// --- ALERTING THRESHOLDS --- 
// if curr_value >= threshold, trigger alert sending, and keep sending as long as the next sample is also above threshold
// if curr_vale >= threshold, do not add to buffer 


AirQualitySensor airQualitySensor; 

AlertState alertState = ALERT_IDLE;
unsigned long suppressUntil = 0;
unsigned long lastAlertTx = 0;
AQReading curr_reading = {0, 0, 0, 0, 0, 0, 0, 0};
AQReading alert_reading = {0, 0, 0, 0, 0, 0, 0, 0}; // to hold the reading that triggered the alert, so we can resend same values until condition clears

// global variables for exponential backoff 
uint8_t alertRetryCount = 0;
unsigned long alertRetryInterval = ALERT_RETRY_INTERVAL_MS; 

// create buffer globally
int bufferSize = int(5*60*1000/SAMPLE_INTERVAL_MS); 
unsigned long lastSample = 0;
// create buffers to hold 3 different AQI values
DataBuffer pm25AqiBuffer(bufferSize);
DataBuffer pm100AqiBuffer(bufferSize);
DataBuffer ubaAqiBuffer(bufferSize);

// ====== SENSOR MOCKS ======
#if SENSOR_MOCK
AQReading mockAQReading() {
    return {12, 8, 3, 150, 400, 2, 25, 10}; // normal air quality
}
#endif

// ===== HELPERS ======
void handleSampling() { 
  if (millis() - lastSample >= SAMPLE_INTERVAL_MS) {

    #if SENSOR_MOCK
    curr_reading = mockAQReading();
    #else
    if (airQualitySensor.update()) {
        curr_reading = airQualitySensor.getReading();
    }
    #endif

    // check alert condition
    bool alertCondition = isAlertNeeded(curr_reading);
    if (alertCondition) { 
      if (alertState == ALERT_IDLE && millis() > suppressUntil) { 
        alert_reading = curr_reading; // capture the reading that triggered the alert
        if (sendAlertMsg(&alert_reading.aqi_uba, &alert_reading.aqi_pm25_us, &alert_reading.aqi_pm100_us)) {
          #if ENABLE_ACK 
          alertState = ALERT_PENDING;
          #else 
          alertState = ALERT_IDLE;
          #endif
          lastAlertTx = millis();
          #if DEBUG_MODE
          Serial.println("Alert sent. Entering ALERT_PENDING state.");
          #endif
        } else {
          #if DEBUG_MODE
          Serial.println("Failed to send alert message.");
          #endif
          // still set state to pending 
          alertState = ALERT_PENDING;
          lastAlertTx = millis();
        }
      }
    } 
    else {
      // feed buffers 
      pm25AqiBuffer.addSample(curr_reading.aqi_pm25_us);
      pm100AqiBuffer.addSample(curr_reading.aqi_pm100_us);
      ubaAqiBuffer.addSample(curr_reading.aqi_uba);
    }


    lastSample = millis();

    #if DEBUG_MODE
    Serial.printf("Sampled - PM2.5 AQI: %d, PM10 AQI: %d, UBA: %d\n",
        curr_reading.aqi_pm25_us, curr_reading.aqi_pm100_us, curr_reading.aqi_uba);
    #endif

  }

}


void handleAlertStates() {
  #if ENABLE_ACK

  if (alertState == ALERT_PENDING && millis() > suppressUntil) { 
    if ((millis() - lastAlertTx >= alertRetryInterval)){ 
      if (sendAlertMsg(&alert_reading.aqi_uba, &alert_reading.aqi_pm25_us, &alert_reading.aqi_pm100_us)) {  // Use saved alert value, not current reading
        lastAlertTx = millis();
        alertRetryCount++;

        // bounded exponential backoff for retry interval
        alertRetryInterval = min(ALERT_RETRY_INTERVAL_MS * (1 << alertRetryCount), 
        ALERT_MAX_RETRY_PERIOD); // double the interval, cap at max
      }

    }

  }

  // when ACK is received, reset backoff 
  if (alertState == ALERT_ACKED) {
    alertRetryCount = 0;
    alertRetryInterval = ALERT_RETRY_INTERVAL_MS; // reset to initial value
    #if DEBUG_MODE
    Serial.println("[ALERT] ACK received, reset retry count and interval.");
    #endif
    // reset alert state
    alertState = ALERT_IDLE; 
    /* NOTE: 
    Set back to IDLE state so if we get a new alert condition after taking the next sample, then we can still trigger new alert 
    (relying on condition check in handleSampling to trigger alert immediately after sampling.
    There should only be one alert per current sample.
   */
  }

  #endif 

  // Handle manual ALERT_CLEARED (set by gateway button/command)
  // Stop retries, set suppression, return to IDLE immediately
  if (alertState == ALERT_CLEAR) {
    alertRetryCount = 0;
    alertRetryInterval = ALERT_RETRY_INTERVAL_MS;
    suppressUntil = millis() + ALERT_SUPPRESS_DURATION;  // Set 1 minute suppression
    alertState = ALERT_IDLE;
    #if DEBUG_MODE
    Serial.println("Exiting ALERT_CLEAR state. Returning to ALERT_IDLE.");
    #endif
  }
}

// ====== SETUP AND LOOP ====

void setup() {
    Serial.begin(115200);
    while (!initCAN()) {
        Serial.println("CAN initialization failed. Retrying...");
        delay(30);
    }

    #if SENSOR_MOCK == 0
    airQualitySensor.begin();
    #endif

    twai_filter_config_t f_config = {
    .acceptance_code = ((uint32_t)GATEWAY_NODE << 21), // accept when NodeID == 0x01 
    .acceptance_mask =  ((uint32_t)0x07 << 21), // check only Node ID bits
    .single_filter = true
  };

}

void loop() { 
  handleSampling();
  handleIncomingMsg(pm25AqiBuffer, pm100AqiBuffer, ubaAqiBuffer, alertState, suppressUntil, airQualitySensor);
  handleAlertStates();
  delay(100);
}
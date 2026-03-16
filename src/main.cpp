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
#define ALERT_INJECTION 0 // set to 1 to randomly inject alerts for testing, set to 0 for normal operation
#define INJECTION_INTERVAL_MS 60000 

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

// Sequence number tracking for duplicate detection
uint8_t alert_seq_num = 0;       // global sequence counter (0-255, wraps)
uint8_t alert_seq_saved = 0;     // saved seq_num for current alert being retried

// create buffer globally
int bufferSize = int(5*60*1000/SAMPLE_INTERVAL_MS); 
unsigned long lastPoll = 0;
unsigned long lastSample = 0;

#if ALERT_INJECTION
unsigned long lastInjection = 0;
#endif
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
  // Poll sensor every 1 second to keep it active and check for alerts
  if (millis() - lastPoll >= POLL_INTERVAL_MS) {

    #if SENSOR_MOCK
    curr_reading = mockAQReading();
    #else
    if (airQualitySensor.update()) {
        curr_reading = airQualitySensor.getReading();
    }
    #endif

    // Alert injection mode for testing
    #if ALERT_INJECTION
    if (millis() - lastInjection >= INJECTION_INTERVAL_MS) {
      curr_reading.aqi_pm25_us = 150;  // Inject alert-triggering value (threshold is 100)
      curr_reading.aqi_pm100_us = 50;  // Keep other values normal
      curr_reading.aqi_uba = 2;
      lastInjection = millis();
      #if DEBUG_MODE
      Serial.println("[ALERT_INJECTION] Injecting alert condition: PM2.5 AQI = 150");
      #endif
    }
    #endif

    // Check alert condition on every poll (1 second)
    bool alertCondition = isAlertNeeded(curr_reading);
    if (alertCondition) { 
      if (alertState == ALERT_IDLE && millis() > suppressUntil) { 
        alert_reading = curr_reading; // capture the reading that triggered the alert
        alert_seq_saved = alert_seq_num; // save the current sequence number
        alert_seq_num = (alert_seq_num + 1) % 256; // increment global sequence number for next alert

        if (sendAlertMsg(&alert_reading.aqi_uba, &alert_reading.aqi_pm25_us, &alert_reading.aqi_pm100_us, alert_seq_saved)) {
          #if ENABLE_ACK 
          alertState = ALERT_PENDING;
          #else 
          alertState = ALERT_IDLE;
          #endif
          lastAlertTx = millis();
          #if DEBUG_MODE
          Serial.println("Alert sent. Entering ALERT_PENDING state. Seq_number: " + String(alert_seq_saved));
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

    lastPoll = millis();

    #if DEBUG_MODE
    Serial.printf("Polled - PM2.5 AQI: %d, PM10 AQI: %d, UBA: %d\n",
        curr_reading.aqi_pm25_us, curr_reading.aqi_pm100_us, curr_reading.aqi_uba);
    #endif
  }

  // Buffer samples every 5 seconds (every 5th poll) if NOT an alert
  if (millis() - lastSample >= SAMPLE_INTERVAL_MS) {
    bool alertCondition = isAlertNeeded(curr_reading);
    if (!alertCondition) {
      // Only feed buffers if not in alert condition
      pm25AqiBuffer.addSample(curr_reading.aqi_pm25_us);
      pm100AqiBuffer.addSample(curr_reading.aqi_pm100_us);
      ubaAqiBuffer.addSample(curr_reading.aqi_uba);
      
      #if DEBUG_MODE
      Serial.printf("Buffered sample - PM2.5 AQI: %d, PM10 AQI: %d, UBA: %d\n",
          curr_reading.aqi_pm25_us, curr_reading.aqi_pm100_us, curr_reading.aqi_uba);
      #endif
    }

    lastSample = millis();
  }

}


void handleAlertStates() {
  #if ENABLE_ACK

  if (alertState == ALERT_PENDING && millis() > suppressUntil) { 
    if ((millis() - lastAlertTx >= alertRetryInterval)){ 
      if (sendAlertMsg(&alert_reading.aqi_uba, &alert_reading.aqi_pm25_us, &alert_reading.aqi_pm100_us, alert_seq_saved)) {  // Use saved alert value, not current reading
        lastAlertTx = millis();
        alertRetryCount++;

        #if DEBUG_MODE
        Serial.println("Resent alert message. Seq_number: " + String(alert_seq_saved) + ". Retry count: " + String(alertRetryCount));
        #endif

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

    // Note: Using ACCEPT_ALL filter from initCAN() - no need to reconfigure
    // If filtering needed in future, apply with twai_driver_install() or twai_reconfigure_alerts()

}

void loop() { 
  // Check CAN bus health
  twai_status_info_t status_info;
  if (twai_get_status_info(&status_info) == ESP_OK) {
    if (status_info.state == TWAI_STATE_BUS_OFF) {
      #if DEBUG_MODE
      Serial.println("CAN BUS-OFF detected! Attempting recovery...");
      #endif
      twai_initiate_recovery();
      delay(100);
    } else if (status_info.state == TWAI_STATE_RECOVERING) {
      #if DEBUG_MODE
      Serial.println("CAN bus in recovery mode...");
      #endif
    }
  }

  handleSampling();
  handleIncomingMsg(pm25AqiBuffer, pm100AqiBuffer, ubaAqiBuffer, alertState, suppressUntil, airQualitySensor);
  handleAlertStates();
  delay(100);
}
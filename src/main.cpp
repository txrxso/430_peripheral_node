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
#define ENABLE_ACK 1 // 0: fire and forget; 1: require ACK for alerts and resend if no ACK
#define ALERT_INJECTION 1 // 1: inject alert values for testing (uses real sensor + injected alerts)
#define INJECTION_INTERVAL_MS 180000 // inject alert every N milliseconds (e.g., 3 minutes, 5 minutes, 10 minutes)
// 5 min = 300000
// 10 min = 600000
SoundSensor noiseSensor(34); 
// GPIO 36? Need to check. Or see if can use other pin with internal pull up/down already.
AlertState alertState = ALERT_IDLE;
unsigned long suppressUntil = 0;
unsigned long lastAlertTx = 0;
uint16_t curr_reading = 0;
uint16_t alert_reading = 0; // stores the value that triggered the current alert (for retries)

// Sequence number tracking for duplicate detection
uint8_t alert_seq_num = 0;       // global sequence counter (0-255, wraps)
uint8_t alert_seq_saved = 0;     // saved seq_num for current alert being retried

// global variables for exponential backoff
uint8_t alertRetryCount = 0;
unsigned long alertRetryInterval = ALERT_RETRY_INTERVAL_MS; // starts at this value

// create buffer globally
int bufferSize = int(5*60*1000/SAMPLE_INTERVAL_MS); 
DataBuffer noiseBuffer(bufferSize); // number of samples
unsigned long lastSample = 0;

#if ALERT_INJECTION
unsigned long lastInjection = 0; // track last injection time
#endif


/* 
==================================
MOCKS FOR SENSOR READING 
==================================
*/
#if SENSOR_MOCK
uint16_t mockNoiseReading() { 
  return 70; 
}

uint16_t mockAlertNoiseReading() { 
  return 120; // instant damage, always send alerts for values detected > 100 dB 
} // for manual testing, make it so that every 1/5 values read is this 'alert' case to test that alerts can be sent

static int internalCounter = 0;
uint16_t mockReadNoiseSensor() { 
  internalCounter++; // increase by 1
  if (internalCounter == 5) {
    internalCounter = 0; // reset to 0
    return mockAlertNoiseReading();
    
  }

  else { 
    return mockNoiseReading();
  }
}
#endif 

void handleSampling(uint16_t& curr_reading) {
  // sample at fixed interval
  if (millis() - lastSample >= SAMPLE_INTERVAL_MS) {
    // continuously sample noise 
    #if SENSOR_MOCK 
    curr_reading = mockReadNoiseSensor();
    #else 
    if (noiseSensor.update()) {
      curr_reading = noiseSensor.getCurrentReading();
      
      // overwrite with alert value if injection is enabled and interval elapsed
      #if ALERT_INJECTION
      if (millis() - lastInjection >= INJECTION_INTERVAL_MS) {
        curr_reading = 115; // inject alert value > 100 dB
        lastInjection = millis();
        #if DEBUG_MODE
        Serial.println("[INJECTION] Injected alert value: 115 dB");
        #endif
      }
      #endif
    }
    #endif

    lastSample = millis();
    
    // CHECK FOR ALERT CONDITION IMMEDIATELY AFTER SAMPLING
    // This ensures we don't miss alerts when curr_reading gets overwritten by next sample
    bool alertCondition = isAlertNeeded(curr_reading);

    // only add to buffer if not in alert condition and not currently suppressing alerts, to avoid polluting the average with alert readings
    if (alertCondition) { 
      // do not add to buffer to avoid polluting the average

      if (alertState == ALERT_IDLE && millis() > suppressUntil) {
        alert_reading = curr_reading; // Save the alert value for retries
        alert_seq_saved = alert_seq_num; // Save sequence number for retries
        alert_seq_num = (alert_seq_num + 1) % 256; // Increment for next alert (wraps at 256)
        
        if (sendAlertMsg(alert_reading, alert_seq_saved)) {
          #if ENABLE_ACK
          alertState = ALERT_PENDING;  // Wait for ACK, keep retrying
          #else
          alertState = ALERT_IDLE;     // Fire-and-forget: sent once, done
          #endif
          lastAlertTx = millis();
          #if DEBUG_MODE
          Serial.printf("[ALERT] Alert sent (seq: %u, %d dB)\n", alert_seq_saved, alert_reading);
          #endif
        }
      }
    } 
    else {
      noiseBuffer.addSample(curr_reading);
      #if DEBUG_MODE
      Serial.printf("Sampled: %d dB\n", curr_reading);
      #endif
    }

  }
}

void handleAlertStates() {
  #if ENABLE_ACK
  // ACK BASED MODE 
  
  // Keep retrying until ACK received, regardless of current reading (TCP-like reliability)
  // Only check: state is PENDING, not suppressed, and retry interval elapsed
  if (alertState == ALERT_PENDING && millis() > suppressUntil) { 
    // check if need to retry 
    if ((millis() - lastAlertTx >= alertRetryInterval)){ 
      if (sendAlertMsg(alert_reading, alert_seq_saved)) {  // Use saved alert value and seq_num
        #if DEBUG_MODE
        Serial.printf("[ALERT] Retry #%d (seq: %u, %d dB), waiting for ACK...\n", alertRetryCount + 1, alert_seq_saved, alert_reading);
        #endif
        lastAlertTx = millis();
        alertRetryCount++;

        // bounded exponential backoff for retry interval
        alertRetryInterval = min(ALERT_RETRY_INTERVAL_MS * (1 << alertRetryCount), 
        ALERT_MAX_RETRY_PERIOD); // double the interval, cap at max
      }
    }
  }

  // when ACK is received (handled in handleIncomingMsg), reset backoff 
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
   
  // Handle manual ALERT_CLEARED (set by incoming.cpp when gateway sends clear command)
  if (alertState == ALERT_CLEAR) {
    // Stop retrying, set suppression, return to IDLE
    alertRetryCount = 0;
    alertRetryInterval = ALERT_RETRY_INTERVAL_MS;
    suppressUntil = millis() + ALERT_SUPPRESS_DURATION;
    alertState = ALERT_IDLE;
    
    #if DEBUG_MODE
    Serial.println("[ALERT] Received ALERT_CLEARED, suppressing alerts for 1 minute");
    #endif
  }

}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  while (!initCAN()) {
    delay(20);
  } 

  #if SENSOR_MOCK == 0 
  noiseSensor.begin();
  #endif 

  // configure filter to ignore messages from itself
  twai_filter_config_t f_config = {
    .acceptance_code = ((uint32_t)GATEWAY_NODE << 21), // accept when NodeID == 0x01 
    .acceptance_mask =  ((uint32_t)0x07 << 21), // check only Node ID bits
    .single_filter = true
  };


}

void loop() {
  
  handleSampling(curr_reading);
  handleIncomingMsg(noiseBuffer, alertState, suppressUntil, noiseSensor);
  handleAlertStates();

  delay(100);
}



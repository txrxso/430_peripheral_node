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
#define SENSOR_MOCK 1 // set to 1 to use mock sensor readings, set to 0 to use real sensor readings from SoundSensor class

SoundSensor noiseSensor(A0); 
// GPIO 36? Need to check. Or see if can use other pin with internal pull up/down already.
AlertState alertState = ALERT_IDLE;
unsigned long suppressUntil = 0;
unsigned long lastAlertTx = 0;
uint16_t curr_reading = 0;


// create buffer globally
int bufferSize = int(5*60*1000/SAMPLE_INTERVAL_MS); 
DataBuffer noiseBuffer(bufferSize); // number of samples
unsigned long lastSample = 0;


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

bool isAlertNeeded(uint16_t value) {
  // for noise, send alert if > 100 dB 
  if (value >= ALERT_THRESHOLD_DB) {
    return true;
  }
  else {
    return false;
  }
}

void handleSampling(uint16_t& curr_reading) {
  // sample at fixed interval
  if (millis() - lastSample >= SAMPLE_INTERVAL_MS) {
    // continuously sample noise 
    #if SENSOR_MOCK 
    curr_reading = mockReadNoiseSensor(); // TO DO: replace with real sensor reading
    #else 
    if (noiseSensor.update()) {
      curr_reading = noiseSensor.getCurrentReading();
    }
    #endif

    noiseBuffer.addSample(curr_reading);
    lastSample = millis();
    
    #if DEBUG_MODE
    Serial.printf("Sampled: %d dB\n", curr_reading);
    #endif

    // CHECK FOR ALERT CONDITION IMMEDIATELY AFTER SAMPLING
    // This ensures we don't miss alerts when curr_reading gets overwritten by next sample
    bool alertCondition = isAlertNeeded(curr_reading);
    if (alertCondition && alertState == ALERT_IDLE && millis() > suppressUntil) {
      if (sendAlertMsg(curr_reading)) {
        #if DEBUG_MODE
        Serial.println("[ALERT] Sent initial alert immediately after sampling, waiting for ACK...");
        #endif
        alertState = ALERT_PENDING;
        lastAlertTx = millis();
      }
    }
  }
}

void handleAlertStates() {
  bool alertCondition = isAlertNeeded(curr_reading);
  
  // NOTE: Initial alert sending moved to handleSampling() to prevent missing alerts
  // when curr_reading gets overwritten by next sample
  
  // only resend alert if no ACK after timeout
  if (alertCondition && alertState == ALERT_PENDING && (millis() - lastAlertTx >= ALERT_RETRY_INTERVAL_MS)) {
    if (sendAlertMsg(curr_reading)) {
      #if DEBUG_MODE
      Serial.println("[ALERT] Resent alert, still waiting for ACK...");
      #endif
      lastAlertTx = millis();
    }
  }

  else if (!alertCondition && alertState != ALERT_IDLE) {
    // clear alert state
    alertState = ALERT_IDLE;
    suppressUntil = millis() + ALERT_SUPPRESS_DURATION; // set suppress duration
    #if DEBUG_MODE
    Serial.println("[ALERT] Alert condition cleared, returning to IDLE state.");
    #endif

  }

  // always check the timer for ALERT_SUPRESS_DURATION and see if suppression over
  if (alertState == ALERT_IDLE && millis() > suppressUntil) {
    // suppression period over, ready to send alerts again
    #if DEBUG_MODE
    Serial.println("[ALERT] Suppression period over, ready to send alerts.");
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



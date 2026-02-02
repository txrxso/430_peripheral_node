/* 
CODE FOR ESP32. 
Acts as a peripheral module that sends noise level data via CAN bus when requested by gateway node via HEARTBEAT_REQUEST.
Sends HEARTBEAT_RESPONSE.


TO DO: 
- work in ALERT SENDING
- work in handling ALERT ACKs 
- work in handling MANUAL_CLEAR (ie. set a 1-2 min timer to disregard next alerts)
- work in incomingCANMEssage monitoring (FreeRTOS?)
*/

#include <Arduino.h>
#include <driver/twai.h>
#include "can_common.h"
#include "outgoing.h"
#include "incoming.h"
#include "data_buffer.h"

#define DEBUG_MODE 1


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
    curr_reading = mockReadNoiseSensor(); // TO DO: replace with real sensor reading
    noiseBuffer.addSample(curr_reading);
    lastSample = millis();

    #if DEBUG_MODE
    Serial.printf("Sampled: %d dB\n", curr_reading);
    #endif
  }
}

void handleAlertStates() {
  bool alertCondition = isAlertNeeded(curr_reading);
  
  // Initial alert send
  if (alertCondition && alertState == ALERT_IDLE && millis() > suppressUntil) {
    if (sendAlertMsg(curr_reading)) {
      #if DEBUG_MODE
      Serial.println("[ALERT] Sent initial alert, waiting for ACK...");
      #endif
      alertState = ALERT_PENDING;
      lastAlertTx = millis();
    }
  }

  // only resend alert if no ACK after timeout
  else if (alertCondition && alertState == ALERT_PENDING && (millis() - lastAlertTx >= ALERT_RETRY_INTERVAL_MS)) {
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

  // configure filter to ignore messages from itself
  twai_filter_config_t f_config = {
    .acceptance_code = ((uint32_t)GATEWAY_NODE << 21), // accept when NodeID == 0x01 
    .acceptance_mask =  ((uint32_t)0x07 << 21), // check only Node ID bits
    .single_filter = true
  };


}

void loop() {
  
  handleSampling(curr_reading);
  handleIncomingMsg(noiseBuffer, alertState, suppressUntil);
  handleAlertStates();

  delay(100);
}



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

AirQualitySensor airQualitySensor; 

AlertState alertState = ALERT_IDLE;
unsigned long suppressUntil = 0;
unsigned long lastAlertTx = 0;
AQReading curr_reading = {0, 0, 0, 0, 0, 0, 0, 0};

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

    // feed buffers 
    pm25AqiBuffer.addSample(curr_reading.aqi_pm25_us);
    pm100AqiBuffer.addSample(curr_reading.aqi_pm100_us);
    ubaAqiBuffer.addSample(curr_reading.aqi_uba);

    lastSample = millis();

    #if DEBUG_MODE
    Serial.printf("Sampled - PM2.5 AQI: %d, PM10 AQI: %d, UBA: %d\n",
        curr_reading.aqi_pm25_us, curr_reading.aqi_pm100_us, curr_reading.aqi_uba);
    #endif

    // TODO: check alert condition

  }

}

// TODO: do we want alerts for AQ? 
void handleAlertStates() {

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
  delay(100);
}
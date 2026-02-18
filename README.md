# Noise Node Peripheral Code 

## Configuration Parameters
In `include/sensor.h`: 
```c++
#define VREF 3.3  // 3.3V on ESP32, 5V on Arduino
#define ADC_MAX 4096 // ESP32 has 12 bit resolution. If on Arduino, this would be 1024 because 10-bit resolution
```

In ``: 
```c++
#define CAN_TX_PIN 5
#define CAN_RX_PIN 4
#define SAMPLE_INTERVAL_MS 5000 // 5 seconds
#define ALERT_THRESHOLD_DB 100 // trigger alert threshold
#define ALERT_RETRY_INTERVAL_MS 1000 // how often to resend unacked alert messages
#define ALERT_SUPPRESS_DURATION 60000 // suppress further alerts for 1 minute if receive ALERT_CLEARED via CAN (to avoid clogging up the bus)
```

In `src/main.cpp`: 
```c++
#define DEBUG_MODE 1 // set to 1 if want print statements for debugging
#define SENSOR_MOCK 1 // set to 1 to use mock sensor readings, set to 0 to use real sensor readings from SoundSensor class

SoundSensor noiseSensor(A0); // change analog pin correspondingly 
```

## Usage
1. Clone the noise branch: `git clone -b noise https://github.com/txrxso/430_peripheral_node`
2. `pio run --target upload`

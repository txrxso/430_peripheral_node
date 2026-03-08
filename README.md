# Noise Node Peripheral Code 

## Configuration Parameters
In `include/sensor.h`: 
```c++
#define VREF 3.3  // 3.3V on ESP32, 5V on Arduino
#define ADC_MAX 4096 // ESP32 has 12 bit resolution. If on Arduino, this would be 1024 because 10-bit resolution
```

In `include/can_common.h`: 
```c++
#define CAN_TX_PIN 5
#define CAN_RX_PIN 4
#define SAMPLE_INTERVAL_MS 5000 // 5 seconds
#define ALERT_RETRY_INTERVAL_MS 1000 // how often to resend unacked alert messages
#define ALERT_RETRY_BACKOFF_FACTOR 2 // exponential backoff factor for resending alerts (e.g. 1s, 2s, 4s, etc.)
#define ALERT_MAX_RETRY_PERIOD 8000 // cap at 8 seconds
#define ALERT_SUPPRESS_DURATION 60000 // suppress further alerts for 1 minute if receive ALERT_CLEARED via CAN (to avoid clogging up the bus)

```

In `src/main.cpp`: 
```c++
#define DEBUG_MODE 1 // set to 1 if want print statements for debugging
#define SENSOR_MOCK 0 // set to 1 to use mock sensor readings, set to 0 to use real sensor readings from SoundSensor class
#define ENABLE_ACK 1 // 0: fire and forget; 1: require ACK for alerts and resend if no ACK
#define ALERT_INJECTION 1 // 1: inject alert values for testing (uses real sensor + injected alerts)
#define INJECTION_INTERVAL_MS 180000 // inject alert every N milliseconds (e.g., 3 minutes, 5 minutes, 10 minutes)

SoundSensor noiseSensor(34); // change analog pin correspondingly 
```

## Usage
1. Clone the noise branch: `git clone -b noise https://github.com/txrxso/430_peripheral_node`
2. `pio run --target upload`

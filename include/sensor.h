/* 
- Sensor reading code for PM and ENS sensor
- Detecting sensor prescence (pull-down required)
*/

#ifndef SENSOR_H
#define SENSOR_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_PM25AQI.h> 

#define SENSOR_DEBUG_MODE 1
#define ENS160_AHT21_I2C_ADDR 0x53  // may also be 0x52 - need to check
#define PM_RX_PIN 16  // ESP32 RX connected to PM sensor TX
#define PM_TX_PIN 17  // ESP32 TX connected to PM sensor RX

struct AQReading {
    uint16_t pm25, pm10, pm100;
    uint16_t tvoc, eco2;
    uint16_t  aqi_uba;
    uint16_t aqi_pm25_us, aqi_pm100_us;
};

class ENS160; // forward declaration b/c of compiler issues

// group both PM and ENS sensor into one class for easier handling
class AirQualitySensor {
    public: 
        // declare an instance of it
        AirQualitySensor(); 
        ~AirQualitySensor(); // destructor 
        void begin(); 
        bool update(); 
        AQReading getReading() const;

    private: 
        Adafruit_PM25AQI _pmSensor;
        ENS160* _ensSensor;

        // check connected
        bool _pmConnected;
        bool _ensConnected;
        AQReading _readings;
        
        bool isPMConnected() const;
        bool isENSConnected() const;

};



#endif
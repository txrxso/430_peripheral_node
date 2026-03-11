/* 
- Sensor reading code for PM and ENS sensor
- Detecting sensor prescence (pull-down required)
*/

#ifndef SENSOR_H
#define SENSOR_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_PM25AQI.h> 
#include <DFRobot_AHT20.h>
#include <DFRobot_ENS160.h>

#define SENSOR_DEBUG_MODE 1
#define ENS160_AHT21_I2C_ADDR 0x52  // may also be 0x52 - need to check
//#define PM_RX_PIN 16  // ESP32 RX connected to PM sensor TX
//#define PM_TX_PIN 17  // ESP32 TX connected to PM sensor RX
#define PM_RX_PIN 26 // Connects to PMS5003 TX - taken from newest commit
#define PM_TX_PIN 27 // Connects to PMS5003 RX

#define AQI_UBA_THRESHOLD 4 
#define AQI_PM25_THRESHOLD 100
#define AQI_PM10_THRESHOLD 100

struct AQReading {
    uint16_t pm25, pm10, pm100;
    uint16_t tvoc, eco2;
    uint16_t  aqi_uba;
    uint16_t aqi_pm25_us, aqi_pm100_us;
};

bool isAlertNeeded(const AQReading& reading);

// group both PM and ENS sensor into one class for easier handling
class AirQualitySensor {
    public: 
        // declare an instance of it
        AirQualitySensor(); 
        void begin(); 
        bool update(); 
        AQReading getReading() const;

    private: 
        Adafruit_PM25AQI _pmSensor;
        DFRobot_AHT20 _aht21Sensor;
        DFRobot_ENS160_I2C _ensSensor;

        // check connected
        bool _pmConnected;
        bool _ensConnected;
        AQReading _readings;
        
        bool isPMConnected() const;
        bool isENSConnected() const;

};



#endif
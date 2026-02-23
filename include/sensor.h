/* 
- Sensor reading code for PM and ENS sensor
- Detecting sensor prescence (pull-down required)
*/

#ifndef SENSOR_H
#define SENSOR_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_PM25AQI.h> 
#include <ScioSense_ENS16x.h>


struct AQReading {
    uint16_t pm25, pm10, pm100;
    uint16_t tvoc, eco2;
    uint16_t  aqi;
};


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
        ENS160 _ensSensor;

        // check connected
        bool _pmConnected;
        bool _ensConnected;
        AQReading _reading;
        
        bool isPMConnected() const;
        bool isENSConnected() const;

};


#endif
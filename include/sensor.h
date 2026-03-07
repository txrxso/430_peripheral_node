/* 
- Sensor reading code
- Detecting sensor prescence (pull-down required)
*/

#ifndef SENSOR_H
#define SENSOR_H

#include <Arduino.h>

#define VREF 3.3 
#define ADC_MAX 4096 // ESP32 has 12 bit resolution
#define ALERT_THRESHOLD_DB 100 // trigger alert threshold

bool isAlertNeeded(uint16_t value);

class SoundSensor {
    public: 
        // init
        SoundSensor(uint8_t pin); // valid pins : GPIO 12-15, 25-27, 32-39

        void begin();
        bool update(); 
        uint16_t getCurrentReading() const; // return current reading
        bool isConnected() const;

    private: 
        uint8_t _pin;
        uint16_t _currentReading; // dB
        bool _connected;

        static const uint16_t DISCONNECTED_THRESHOLD = 500; // min_valid_adc 
        static const uint16_t MAX_ADC_VALUE = 4095;
};



#endif
#include "sensor.h"

// init
SoundSensor::SoundSensor(uint8_t pin) 
: _pin(pin), _currentReading(0), _connected(false) {}

void SoundSensor::begin() {
    pinMode(_pin, INPUT);
}

bool SoundSensor::update() {
    uint16_t adcValue = analogRead(_pin);
    _currentReading = adcValue; // for now, just return raw ADC value as 'dB' reading

    // check if sensor is connected based on threshold
    if (adcValue < MAX_ADC_VALUE && adcValue >= DISCONNECTED_THRESHOLD) {
        _connected = true;

        // perform a reading
        float voltage = (adcValue / float(ADC_MAX)) * VREF; // convert ADC to voltage
        _currentReading = voltage * 50.0;
    }
    else {
        _connected = false;
    }

    return _connected;
}

uint16_t SoundSensor::getCurrentReading() const {
    return _currentReading;
}

bool SoundSensor::isConnected() const {
    return _connected;
}   
#include <ScioSense_ENS16x.h>
#include "sensor.h"

AirQualitySensor::AirQualitySensor() : 
    _pmSensor(), _ensSensor(new ENS160()),
    // set all connected states to false  
    _pmConnected(false), _ensConnected(false), 
    // set all reading values to 0 
    _readings{0, 0, 0, 0, 0, 0, 0, 0} {}

AirQualitySensor::~AirQualitySensor() {
    delete _ensSensor;
}

bool AirQualitySensor::isPMConnected()  const { return _pmConnected; }
bool AirQualitySensor::isENSConnected() const { return _ensConnected; }

void AirQualitySensor::begin() {
    Wire.begin();

    _pmConnected = _pmSensor.begin_I2C();

    _ensSensor->begin(&Wire, ENS160_AHT21_I2C_ADDR);
    _ensConnected = _ensSensor->init();

    while (!_pmConnected || !_ensConnected) {
        #if SENSOR_DEBUG_MODE
        Serial.println("At least one sensor was unable to be initialized.");
        Serial.println(isPMConnected() ? "PM sensor is connected." : "PM sensor is not connected.");
        Serial.println(isENSConnected() ? "ENS sensor is connected." : "ENS sensor is not connected.");
        // try again
        #endif
        _pmConnected = _pmSensor.begin_I2C();
        _ensConnected = _ensSensor->init();
        delay(1000);
    }
    
}

bool AirQualitySensor::update() {
    PM25_AQI_Data pmData;
    bool pm_updated = false; 
    bool ens_updated = false;

    if (_pmSensor.read(&pmData) == true) {
        _pmConnected = true; // sync again
        _readings.pm25 = pmData.pm25_standard;
        _readings.pm10 = pmData.pm10_standard;
        _readings.pm100 = pmData.pm100_standard;
        _readings.aqi_pm100_us = pmData.aqi_pm100_us;
        _readings.aqi_pm25_us = pmData.aqi_pm25_us;
        pm_updated = true;
    } else {
        _pmConnected = false; // sync again
    }

    if (_ensSensor->update() == RESULT_OK) {
        _ensConnected = true; // sync again
        _readings.tvoc = _ensSensor->getTvoc();
        _readings.eco2 = _ensSensor->getEco2();
        _readings.aqi_uba = (uint16_t)_ensSensor->getAirQualityIndex_UBA(); // cast from enum to uint16_t
        ens_updated = true;
    } else {
        _ensConnected = false; // sync again
    }

    return pm_updated || ens_updated;   

}

AQReading AirQualitySensor::getReading() const {
    return _readings;
}


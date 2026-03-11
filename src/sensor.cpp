#include "sensor.h"

bool isAlertNeeded(const AQReading& reading) {
    return (reading.aqi_uba >= AQI_UBA_THRESHOLD) || 
           (reading.aqi_pm25_us >= AQI_PM25_THRESHOLD) || 
           (reading.aqi_pm100_us >= AQI_PM10_THRESHOLD);
}

AirQualitySensor::AirQualitySensor() : 
    _pmSensor(), _ensSensor(&Wire, ENS160_AHT21_I2C_ADDR),
    // set all connected states to false  
    _pmConnected(false), _ensConnected(false), 
    // set all reading values to 0 
    _readings{0, 0, 0, 0, 0, 0, 0, 0} {}


bool AirQualitySensor::isPMConnected()  const { return _pmConnected; }
bool AirQualitySensor::isENSConnected() const { return _ensConnected; }

void AirQualitySensor::begin() {
    Wire.begin();

    // Initialize UART for PM sensor
    Serial2.begin(9600, SERIAL_8N1, PM_RX_PIN, PM_TX_PIN);
    delay(3000); // Wait for PM sensor to boot up

    _pmConnected = _pmSensor.begin_UART(&Serial2);

    // begin AHT sensor 
    bool aht_success = _aht21Sensor.begin();
    #if SENSOR_DEBUG_MODE
    if (aht_success == 0 ) {
        Serial.println("AHT21 initialized.");
    } else {
        Serial.println("AHT21 failed to initialize.");
        while (1) {
            Serial.println("AHT21 failed to initialize.");
            delay(1000);
        }
    }
    #endif

    // begin ENS sensor 
    _ensSensor.begin();
    _ensConnected = (_ensSensor.begin() == NO_ERR);

    _ensSensor.setPWRMode(ENS160_STANDARD_MODE);

    while (!_pmConnected || !_ensConnected) {
        #if SENSOR_DEBUG_MODE
        Serial.println("At least one sensor was unable to be initialized.");
        Serial.println(isPMConnected() ? "PM sensor is connected." : "PM sensor is not connected.");
        Serial.println(isENSConnected() ? "ENS sensor is connected." : "ENS sensor is not connected.");
        // try again
        #endif
        _pmConnected = _pmSensor.begin_UART(&Serial2);
        _ensConnected = (_ensSensor.begin() == 0);
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

    uint8_t ens_status = _ensSensor.getENS160Status();
    if (ens_status == 0x02 || ens_status == 0x03) { // Normal operation or warm-up mode
        Serial.println("ENS160 data ready");
        // if Temp and Humidity available, use to get more accurate reading
        if (_aht21Sensor.startMeasurementReady(true)) {
            float t = _aht21Sensor.getTemperature_C();
            float h = _aht21Sensor.getHumidity_RH();
            _ensSensor.setTempAndHum(t,h);
        }
        _ensConnected = true; // sync again
        _readings.tvoc = _ensSensor.getTVOC();
        _readings.eco2 = _ensSensor.getECO2();
        _readings.aqi_uba = _ensSensor.getAQI();
        ens_updated = true;
    } else {
        _ensConnected = false; // sync again
    }

    return pm_updated || ens_updated;   

}

AQReading AirQualitySensor::getReading() const {
    return _readings;
}


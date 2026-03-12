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
    Wire.begin(21, 22);
    Wire.setClock(100000);
    delay(500); // Increased delay for I2C stabilization

    #if SENSOR_DEBUG_MODE
    // Scan I2C bus for debugging
    Serial.println("Scanning I2C bus...");
    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            Serial.print("I2C device found at 0x");
            Serial.println(addr, HEX);
        }
    }
    #endif

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
    }
    #endif

    // begin ENS sensor - call only once and check result
    int8_t ens_status = _ensSensor.begin();
    _ensConnected = (ens_status == NO_ERR);
    
    #if SENSOR_DEBUG_MODE
    Serial.print("ENS160 begin() returned: ");
    Serial.println(ens_status);
    if (_ensConnected) {
        Serial.println("ENS160 initialized successfully.");
    } else {
        Serial.println("ENS160 failed to initialize. Check I2C address (0x52 or 0x53) and wiring.");
    }
    #endif

    if (_ensConnected) {
        _ensSensor.setPWRMode(ENS160_STANDARD_MODE);
    }

    // Retry sensor initialization with timeout to prevent infinite blocking
    unsigned long retryStart = millis();
    const unsigned long INIT_TIMEOUT_MS = 30000; // 30 second timeout
    int retryCount = 0;
    const int MAX_RETRIES = 5;

    while ((!_pmConnected || !_ensConnected) && 
           (millis() - retryStart < INIT_TIMEOUT_MS) && 
           (retryCount < MAX_RETRIES)) {
        #if SENSOR_DEBUG_MODE
        Serial.println("At least one sensor was unable to be initialized.");
        Serial.println(isPMConnected() ? "PM sensor is connected." : "PM sensor is not connected.");
        Serial.println(isENSConnected() ? "ENS sensor is connected." : "ENS sensor is not connected.");
        Serial.printf("Retry attempt %d/%d\n", retryCount + 1, MAX_RETRIES);
        #endif
        
        if (!_pmConnected) {
            _pmConnected = _pmSensor.begin_UART(&Serial2);
            delay(3000);
        }
        
        if (!_ensConnected) {
            int8_t ens_result = _ensSensor.begin();
            _ensConnected = (ens_result == NO_ERR);
            #if SENSOR_DEBUG_MODE
            Serial.print("ENS160 retry returned: ");
            Serial.println(ens_result);
            #endif
            if (_ensConnected) {
                _ensSensor.setPWRMode(ENS160_STANDARD_MODE);
            }
            delay(1000);
        }
        
        retryCount++;
    }
    
    // Final status report
    #if SENSOR_DEBUG_MODE
    Serial.println("=== Sensor Initialization Complete ===");
    Serial.printf("PM Sensor: %s\n", _pmConnected ? "CONNECTED" : "FAILED");
    Serial.printf("ENS160 Sensor: %s\n", _ensConnected ? "CONNECTED" : "FAILED");
    if (!_pmConnected || !_ensConnected) {
        Serial.println("WARNING: Continuing with partial sensor functionality");
        Serial.println("CAN communication will proceed normally");
    }
    Serial.println("======================================");
    #endif
    
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


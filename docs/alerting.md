# Alerting 

For every sample collected (sampling frequency = 5 seconds): 
1. Check if reading exceeds its corresponding alert threshold.
2. If **above threshold**, send immediately to the gateway as an alert and **exclude from the moving average buffer**.
3. If **below threshold**, store in buffer as normal.
4. On the next sample, repeat the cycle regardless of previous state.

## Alerting Thresholds
| Sensor   | Threshold |
|----------|-----------|
| PM2.5 AQI | ≥ 100   |
| PM10 AQI  | ≥ 100   |
| AQI UBA   | ≥ 4     |

## CAN Alert Frame Structure For Air Quality Alerts
Alerts are transmitted as a fixed 8-byte CAN frame. The `alert_mask` byte indicates which sensors triggered, and only the corresponding value fields are considered valid by the receiver.
```cpp
#define ALERT_AQI_UBA  (1 << 0)  // 0b00000001
#define ALERT_PM25     (1 << 1)  // 0b00000010
#define ALERT_PM100    (1 << 2)  // 0b00000100

typedef struct {
    uint8_t  alert_mask;   // bitmask of which sensors triggered
    uint8_t  reserved;     // always zero
    uint16_t aqi_uba;      // valid if alert_mask & ALERT_AQI_UBA
    uint16_t pm25_aqi;     // valid if alert_mask & ALERT_PM25
    uint16_t pm100_aqi;    // valid if alert_mask & ALERT_PM100
} __attribute__((packed)) airQualityAlert_t;  // 8 bytes exactly
```

For example, if PM2.5 and PM10 both exceed their thresholds, `alert_mask = 0b00000110` and only `pm25_aqi` and `pm100_aqi` carry valid readings.

# Data Buffer Usage
- Circular buffer to compute moving averages of air quality sensor readings 
- Buffer size calculated to hold 5 minutes of samples (1 heartbeat cycle)

```c++ 
// three buffers, each for a diff type of reading 
DataBuffer pm25AqiBuffer(bufferSize);   // PM2.5 AQI readings
DataBuffer pm100AqiBuffer(bufferSize);  // PM10 AQI readings  
DataBuffer ubaAqiBuffer(bufferSize);    // UBA AQI readings
```
## Usage 
### Sampling 
```c++
pm25AqiBuffer.addSample(curr_reading.aqi_pm25_us);
pm100AqiBuffer.addSample(curr_reading.aqi_pm100_us);
ubaAqiBuffer.addSample(curr_reading.aqi_uba);
```

### Passing to Heartbeat Response
```c++
handleHeartbeatRTRMsg(incoming_msg, 
    pm25AqiBuffer.getAverage(),   // 5-min average of PM2.5
    pm100AqiBuffer.getAverage(),  // 5-min average of PM10
    ubaBuffer.getAverage()        // 5-min average of UBA
);
```

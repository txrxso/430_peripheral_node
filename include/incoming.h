#ifndef INCOMING_H
#define INCOMING_H

#include "data_buffer.h"
#include "can_common.h"
#include "sensor.h"

// outgoing message functions
void handleIncomingMsg(
    // data buffers for 3 AQI values that will be sent
    DataBuffer& pm25AqiBuffer, 
    DataBuffer& pm100AqiBuffer, 
    DataBuffer& ubaBuffer,
    // alert state info 
    AlertState& alertState, 
    unsigned long& suppressUntil,
    AirQualitySensor& airQualitySensor
    ); 

void handleHeartbeatRTRMsg(const twai_message_t& msg, int16_t avg_pm25, uint16_t avg_pm100, uint16_t avg_uba); 

#endif 
#ifndef OUTGOING_H
#define OUTGOING_H


#include "can_common.h"

// outgoing message functions
bool sendHeartbeatResponse(const HeartbeatFrame& hbFrame); 

// pass nullptr for any value that is not relevant to the alert 
// (e.g. if only PM2.5 exceeds threshold, pass in the PM2.5 value and nullptr for the other 2 parameters)
bool sendAlertMsg(const uint16_t* aqi_uba, const uint16_t* pm25_aqi, const uint16_t* pm100_aqi);

#endif 
#ifndef OUTGOING_H
#define OUTGOING_H


#include "can_common.h"

// outgoing message functions
bool sendHeartbeatResponse(const HeartbeatFrame& hbFrame); 
// bool sendAlertMsg(); // for now, not implemented because more on long-term stats

#endif 
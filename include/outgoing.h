#ifndef OUTGOING_H
#define OUTGOING_H


#include "can_common.h"


// outgoing message functions
bool sendHeartbeatResponse(const HeartbeatFrame& hbFrame);
bool sendAlertMsg(uint16_t noise_db, uint8_t seq_num);

#endif 
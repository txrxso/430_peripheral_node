#ifndef INCOMING_H
#define INCOMING_H

#include "data_buffer.h"
#include "can_common.h"

// outgoing message functions
void handleIncomingMsg(DataBuffer& dataBuffer, AlertState& alertState, unsigned long& suppressUntil);
void handleHeartbeatRTRMsg(const twai_message_t& msg, uint16_t average);


#endif 
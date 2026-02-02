#include "incoming.h"
#include "outgoing.h"

#define DEBUG_MODE_INCOMING 1

void handleIncomingMsg(DataBuffer& dataBuffer, AlertState& alertState, unsigned long& suppressUntil) {
    twai_message_t incoming_msg;
    esp_err_t status = twai_receive(&incoming_msg, pdMS_TO_TICKS(100));

    if (status == ESP_OK) {
    // parse msg 
    uint32_t id = incoming_msg.identifier;
    CANPriority priority = static_cast<CANPriority>((id >> 8) & 0x07);
    CANMessageType msgType = static_cast<CANMessageType>((id >> 3) & 0x1F);
    NodeID nodeId = static_cast<NodeID>(id & 0x07);

    // ignore messages from self
    if (nodeId == THIS_NODE) {
        return;
    }

    #if DEBUG_MODE_INCOMING
    Serial.printf("Received CAN Msg - ID: 0x%03X, Priority: %d, Type: %d, From Node: 0x%02X\n", id, priority, msgType, nodeId);
    #endif

    // handle based on message type
    if (msgType == HEARTBEAT_REQUEST && incoming_msg.rtr == 1) {
        handleHeartbeatRTRMsg(incoming_msg, dataBuffer.getAverage());
    }

    else if (msgType == ALERT_ACK && nodeId == GATEWAY_NODE) { 
        if (alertState == ALERT_PENDING) {
            alertState = ALERT_ACKED; 
            #if DEBUG_MODE_INCOMING
            Serial.println("Received ALERT_ACK from Gateway. Alert confirmed.");
            #endif
        }
    }

    else if (msgType == ALERT_CLEARED && nodeId == GATEWAY_NODE) { 
        suppressUntil = millis() + ALERT_SUPPRESS_DURATION; 
        alertState = ALERT_CLEAR;
        #if DEBUG_MODE_INCOMING
        Serial.println("Received ALERT_CLEARED from Gateway. Suppressing alerts for a duration.");
        #endif
    }


    }

    else if (status != ESP_ERR_TIMEOUT) {
        #if DEBUG_MODE_INCOMING
        Serial.printf("Error receiving CAN message: %d\n", status);
        #endif
    }

}

void handleHeartbeatRTRMsg(const twai_message_t& msg, uint16_t average) {
    // prepare and send heartbeat response 
    HeartbeatFrame hbFrame;
    hbFrame.noise_db = average;
    memset(hbFrame.reserved, 0, sizeof(hbFrame.reserved));

    if (sendHeartbeatResponse(hbFrame)) {
        Serial.printf("Sent noise level: %d dB in heartbeat response.\n", hbFrame.noise_db);
    }
}




#include "incoming.h"
#include "outgoing.h"

#define DEBUG_MODE_INCOMING 1

void handleIncomingMsg(
    // data buffers for 3 AQI values that will be sent
    DataBuffer& pm25AqiBuffer, 
    DataBuffer& pm100AqiBuffer, 
    DataBuffer& ubaBuffer,
    // alert state info 
    AlertState& alertState, 
    unsigned long& suppressUntil,
    AirQualitySensor& airQualitySensor
    ) { 

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
            // only if connected do we call handleHeartbeatRTRMsg(incoming_msg, ) 
            if (airQualitySensor.update()) {  // // update sensor reading and connection status
                // feed buffers with latest readings
                AQReading reading = airQualitySensor.getReading();
                pm25AqiBuffer.addSample(reading.aqi_pm25_us);
                pm100AqiBuffer.addSample(reading.aqi_pm100_us);
                ubaBuffer.addSample(reading.aqi_uba);

                handleHeartbeatRTRMsg(incoming_msg, 
                    pm25AqiBuffer.getAverage(), 
                    pm100AqiBuffer.getAverage(), 
                    ubaBuffer.getAverage()
                );
            } else {
                #if DEBUG_MODE_INCOMING
                Serial.println("Received HEARTBEAT_REQUEST RTR, but no AQ sensor detected. Ignoring.");
                #endif
            }
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
        Serial.printf("Error receiving CAN message: %s\n", esp_err_to_name(status));
        #endif
    }

}

void handleHeartbeatRTRMsg(const twai_message_t& msg, int16_t avg_pm25, uint16_t avg_pm100, uint16_t avg_uba) {
    HeartbeatFrame hbFrame; 
    hbFrame.aqi_pm100_us = avg_pm100; 
    hbFrame.aqi_pm_25_us = avg_pm25; 
    hbFrame.aqi_uba = avg_uba;
    memset(hbFrame.reserved, 0, sizeof(hbFrame.reserved)); // zero out reserved bytes

    bool sent = sendHeartbeatResponse(hbFrame);
    #if DEBUG_MODE_INCOMING
    if (sent) {
        Serial.printf("Sent AQI values - PM2.5: %d, PM10: %d, UBA: %d in heartbeat response.\n", hbFrame.aqi_pm_25_us, hbFrame.aqi_pm100_us, hbFrame.aqi_uba);
    } else {
        Serial.println("Failed to send HEARTBEAT_RESPONSE.");
    }
    #endif
}





#include "outgoing.h"

#define DEBUG_MODE_OUTGOING 1

bool sendHeartbeatResponse(const HeartbeatFrame& hbFrame) {
    twai_message_t msg; 
    msg.identifier = buildCANID(HEARTBEAT, HEARTBEAT_RESPONSE, THIS_NODE);

    msg.extd = 0;
    msg.rtr = 0; 
    msg.data_length_code = sizeof(HeartbeatFrame);
    memcpy(msg.data, &hbFrame, sizeof(HeartbeatFrame));

    // attempt to send

    esp_err_t status = twai_transmit(&msg, pdMS_TO_TICKS(100));

    #if DEBUG_MODE_OUTGOING
    if (status == ESP_OK) {
        Serial.println("Heartbeat response sent successfully");
    } else if (status == ESP_ERR_TIMEOUT) {
        Serial.println("CAN TX failed: Timeout");
    } else {
        Serial.printf("CAN TX failed: Error code %d\n", status);
    }
    #endif

    // TO DO: add retry logic later

    return (status == ESP_OK);

}
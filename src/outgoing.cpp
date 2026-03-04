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

bool sendAlertMsg(const uint16_t* aqi_uba, const uint16_t* pm25_aqi, const uint16_t* pm100_aqi) {
    twai_message_t outgoing_msg;
    AlertFrame alertFrame;
    memset(&alertFrame, 0, sizeof(AlertFrame)); // initialize all fields to 0

    // populate alert frame based on which parmeters did not have a nullptr
    if (aqi_uba) {
        alertFrame.aqi_uba = *aqi_uba; // dereference
        alertFrame.alert_mask |= ALERT_AQI_UBA; // bitwise OR to set the corresponding bit in the alert mask
    }
    if (pm25_aqi) {
        alertFrame.pm25_aqi = *pm25_aqi;
        alertFrame.alert_mask |= ALERT_PM25;
    }
    if (pm100_aqi) {
        alertFrame.pm100_aqi = *pm100_aqi;
        alertFrame.alert_mask |= ALERT_PM100;
    }

    /*
    NOTE: 
    receiver side can then check indiviudal bits with & 
    if (frame.alert_mask & ALERT_PM25) {
    // this means the PM2.5 value is valid and included in the alert 
    }
    */

    outgoing_msg.identifier = buildCANID(SAFETY_ALERT, ALERT_NOTIFICATION, THIS_NODE);
    outgoing_msg.extd = 0;
    outgoing_msg.rtr = 0;
    outgoing_msg.data_length_code = sizeof(AlertFrame);
    memcpy(outgoing_msg.data, &alertFrame, sizeof(AlertFrame));

    esp_err_t status = twai_transmit(&outgoing_msg, pdMS_TO_TICKS(100));

    #if DEBUG_MODE_OUTGOING
    if (status == ESP_OK) {
        Serial.println("Alert sent successfully");
    } else if (status == ESP_ERR_TIMEOUT) {
        Serial.println("CAN TX failed: Timeout");
    } else {
        Serial.printf("CAN TX failed: Error code %d\n", status);
    }
    #endif

    return (status == ESP_OK);
}

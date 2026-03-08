#include "outgoing.h"

#define DEBUG_MODE_OUTGOING 1


bool sendHeartbeatResponse(const HeartbeatFrame& hbFrame) { 
  twai_message_t msg;
  msg.identifier = buildCANID(HEARTBEAT, HEARTBEAT_RESPONSE, THIS_NODE);

  msg.extd = 0;
  msg.rtr = 0; 
  msg.data_length_code = sizeof(HeartbeatFrame);
  memcpy(msg.data, &hbFrame, sizeof(HeartbeatFrame));

  // attempt to transmit 
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

  return (status == ESP_OK);

  // TO DO: add retry logic later


}

bool sendAlertMsg(uint16_t noise_db, uint8_t seq_num) { 
  // spikes in values are less concerning for noise than continuous exposure - EXCEPT for 'instant damage/hazard' cases
  twai_message_t outgoing_msg;
  AlertFrame frame;

  frame.seq_num = seq_num;
  frame.reserved1 = 0;
  frame.noise_db = noise_db;
  memset(frame.reserved, 0, sizeof(frame.reserved));

  outgoing_msg.identifier = buildCANID(SAFETY_ALERT, ALERT_NOTIFICATION, THIS_NODE);
  outgoing_msg.extd = 0;
  outgoing_msg.rtr = 0;
  outgoing_msg.data_length_code = sizeof(AlertFrame);
  memcpy(outgoing_msg.data, &frame, sizeof(AlertFrame));

  esp_err_t status = twai_transmit(&outgoing_msg, 0); // non-blocking

  #if DEBUG_MODE_OUTGOING
  if (status == ESP_OK) {
    Serial.printf("ALERT_NOTIFICATION sent (seq: %u, %d dB)\n", seq_num, noise_db);
  } else {
    Serial.printf("ALERT_NOTIFICATION TX failed (%d)\n", status);
  }
  #endif

  return (status == ESP_OK);
}
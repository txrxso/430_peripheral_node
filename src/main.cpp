#include <Arduino.h>
#include <driver/twai.h>

#define CAN_TX_PIN 5
#define CAN_RX_PIN 4

#define DEBUG_MODE 1


// heartbeat frame
struct __attribute__((packed)) HeartbeatFrame {
  uint16_t noise_db;
  uint16_t reserved[3]; // to make sure 8 bytes in data expected
}; 

// indicates priority for arbitration
enum CANPriority : uint8_t {
    SAFETY_ALERT    = 0, // threshold exceeded, etc.
    CONTROL   = 1, // gateway commands, acks, etc.
    HEARTBEAT = 2, // anything related to heartbeats 
};

// types of messages that can be sent via CAN bus
enum CANMessageType : uint8_t {
  // HIGHEST PRIORITY 0x0- safety critical 
  // alerting lifecycle has two messages per alert: NOTIFICATION and CLEAR
  ALERT_NOTIFICATION = 0x01, // THRESHOLD EXCEEDED, NEEDS TO BE SENT IMMEDIATELY
  ALERT_ACK = 0x02, // GATEWAY ACKNOLWEDGES IT GOT IT
  ALERT_CLEARED = 0x03, // BACK TO NORMAL
  ALERT_CLEARED_ACK = 0x04, // GATEWAY ACKNOWLEDGES CLEAR 
  /* NOTE: 
   acks required, because need ack specifically from gateway. 
   twai filter still acks so if 2 peripheral modules, don't know which one acked it */

  // MEDIUM PRIORITY 0x2 - heartbeat-related (periodic sensor values, "online" modules)
  HEARTBEAT_REQUEST = 0x05,
  HEARTBEAT_RESPONSE = 0x06,

  // LOW PRIORITY 

};

// indicates which node is sending the message
enum NodeID : uint8_t { 
    GATEWAY_NODE = 0x01,
    NODE_NOISE = 0x02,
    NODE_AIR_Q = 0x03,
};


#define THIS_NODE NODE_NOISE

uint32_t mockNoiseReading() { 
  return 70; 
}

uint32_t buildCANID(CANPriority priority, CANMessageType type, NodeID nodeid) {
    // return 11-bit CAN identifier from the enum types
    return ((priority & 0x07) << 8) |   // Bits 10-8: Priority (3 bits)
           ((type & 0x1F) << 3) |        // Bits 7-3: Message Type (5 bits)
           (nodeid & 0x07);              // Bits 2-0: Node ID (3 bits)
}

bool initCAN() { 
  // get back to clean state before trying
  twai_stop(); 
  twai_driver_uninstall();

  twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)CAN_TX_PIN, (gpio_num_t)CAN_RX_PIN, TWAI_MODE_NORMAL);
  twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();  //Look in the api-reference for other speed sets.
  twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

  // install driver
  if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK) {
      Serial.println("Driver installed");
    } else {
      Serial.println("Failed to install driver");
      return false;
    }

  // start driver
  if (twai_start() == ESP_OK) {
      Serial.println("Driver started");
    } else {
      Serial.println("Failed to start driver");
      return false;
    }

  // set up filter 

  return true;

}

bool sendHeartbeatResponse(const HeartbeatFrame& hbFrame) { 
  twai_message_t msg;
  msg.identifier = buildCANID(HEARTBEAT, HEARTBEAT_RESPONSE, THIS_NODE);

  msg.extd = 0;
  msg.rtr = 0; 
  msg.data_length_code = sizeof(HeartbeatFrame);
  memcpy(msg.data, &hbFrame, sizeof(HeartbeatFrame));

  // attempt to transmit 
  esp_err_t status = twai_transmit(&msg, pdMS_TO_TICKS(100));

  #if DEBUG_MODE
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

bool sendAlertMsg() { 
  // TO DO: 
  return false;
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  while (!initCAN()) {
    delay(20);
  } 

}

void loop() {
  // put your main code here, to run repeatedly:
  twai_message_t rx_msg;

  // wait for incoming CAN msg 
  esp_err_t status = twai_receive(&rx_msg, pdMS_TO_TICKS(100));

  if (status == ESP_OK) {
    // parse msg 
    uint32_t id = rx_msg.identifier;
    CANPriority priority = static_cast<CANPriority>((id >> 8) & 0x07);
    CANMessageType msgType = static_cast<CANMessageType>((id >> 3) & 0x1F);
    NodeID nodeId = static_cast<NodeID>(id & 0x07);

    #if DEBUG_MODE
    Serial.printf("Received CAN msg. ID: 0x%03X, Priority: %d, Type: %d, From Node: 0x%02X\n", id, priority, msgType, nodeId);
    #endif

    if (msgType == HEARTBEAT_REQUEST && rx_msg.rtr == 1) {
      // prepare and send heartbeat response 
      HeartbeatFrame hbFrame;
      hbFrame.noise_db = mockNoiseReading();
      memset(hbFrame.reserved, 0, sizeof(hbFrame.reserved));

      sendHeartbeatResponse(hbFrame);

    } 
  }
}



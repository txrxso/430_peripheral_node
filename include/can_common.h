#ifndef CAN_COMMON_H
#define CAN_COMMON_H

#include <Arduino.h>
#include <driver/twai.h>

#define CAN_TX_PIN 5
#define CAN_RX_PIN 4
#define SAMPLE_INTERVAL_MS 5000 // 5 seconds
#define ALERT_THRESHOLD_DB 100 // trigger alert threshold
#define ALERT_RETRY_INTERVAL_MS 1000 // how often to resend unacked alert messages
#define ALERT_SUPPRESS_DURATION 60000 // suppress further alerts for 1 minute if receive ALERT_CLEARED via CAN (to avoid clogging up the bus)


// alert states 
enum AlertState : uint8_t { 
  ALERT_IDLE,  
  ALERT_PENDING, // notification sent, waiting for gateway ACK
  ALERT_ACKED, // notification sent, gateway ACKed 
  ALERT_CLEAR, // only trigger this state if receive CANMessageType ALERT_CLEARED 
}; 

// CAN message structures
// heartbeat frame
struct __attribute__((packed)) HeartbeatFrame {
  uint16_t aqi_pm_25_us;
  uint16_t aqi_pm100_us;
  uint16_t aqi_uba;
  uint16_t reserved[1]; // to make sure 8 bytes in data expected
}; 

// alert frame
struct __attribute__((packed)) AlertFrame {
  uint16_t aqi_pm_25_us;
  uint16_t aqi_pm100_us;
  uint16_t aqi_uba;
  uint16_t reserved[1];
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


#define THIS_NODE NODE_AIR_Q

// shared global variables (defined in main.cpp)
extern AlertState alertState;
extern unsigned long suppressUntil;
extern unsigned long lastAlertTx;
extern uint16_t curr_reading;

uint32_t buildCANID(CANPriority priority, CANMessageType type, NodeID nodeid);
bool initCAN();


#endif
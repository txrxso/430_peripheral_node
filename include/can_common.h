#ifndef CAN_COMMON_H
#define CAN_COMMON_H

#include <Arduino.h>
#include <driver/twai.h>

#define CAN_TX_PIN 5
#define CAN_RX_PIN 4
#define SAMPLE_INTERVAL_MS 5000 // 5 seconds
#define ALERT_RETRY_INTERVAL_MS 1000 // how often to resend unacked alert messages
#define ALERT_RETRY_BACKOFF_FACTOR 2 // exponential backoff factor for resending alerts (e.g. 1s, 2s, 4s, etc.)
#define ALERT_MAX_RETRY_PERIOD 8000 // cap at 8 seconds
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
  uint16_t noise_db;
  uint16_t reserved[3]; // to make sure 8 bytes in data expected
}; 

// alert frame (with sequence number for duplicate detection)
typedef struct {
  uint8_t seq_num;        // 0-255, wraps around
  uint8_t reserved1;      // padding
  uint16_t noise_db;
  uint16_t reserved[2];   // to make sure 8 bytes in data expected
} __attribute__((packed)) AlertFrame;

// indicates priority for arbitration
enum CANPriority : uint8_t {
    SAFETY_ALERT    = 0, // alerts AND their acks to break deadlock
    // NOTE: acks are part of safety-critical alert loop so needs to break deadlock
    CONTROL   = 1, // unused currently, but can be for future control messages 
                  // (e.g. change sampling rate, connectivity options, etc.)
    HEARTBEAT = 2, // anything related to heartbeats 
};

// types of messages that can be sent via CAN bus
enum CANMessageType : uint8_t {
  // HIGHEST PRIORITY 0x0- safety critical 
  // alerting lifecycle has two messages per alert: NOTIFICATION and CLEAR
  ALERT_NOTIFICATION = 0x01, // THRESHOLD EXCEEDED, NEEDS TO BE SENT IMMEDIATELY (SENT BY PERIPHERAL NODES)
  ALERT_ACK = 0x02, // GATEWAY ACKNOWLEDGES GOT ALERTS FROM PERIPHERAL NODES (SENT BY GATEWAY)
  ALERT_CLEARED = 0x03, // BACK TO NORMAL (DONE ONLY BY BUTTON, GATEWAY SENDS THIS)
  ALERT_CLEARED_ACK = 0x04, // PERIPHERAL NODES ACKNOWLEDGE CLEAR 
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

// shared global variables (defined in main.cpp)
extern AlertState alertState;
extern unsigned long suppressUntil;
extern unsigned long lastAlertTx;
extern uint16_t curr_reading;

uint32_t buildCANID(CANPriority priority, CANMessageType type, NodeID nodeid);
bool initCAN();


#endif
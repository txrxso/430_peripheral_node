/* 
CODE FOR ESP32. 
Acts as a peripheral module that sends noise level data via CAN bus when requested by gateway node via HEARTBEAT_REQUEST.
Sends HEARTBEAT_RESPONSE.


TO DO: 
- work in ALERT SENDING
- work in handling ALERT ACKs 
- work in handling MANUAL_CLEAR (ie. set a 1-2 min timer to disregard next alerts)
- work in incomingCANMEssage monitoring (FreeRTOS?)
*/

#include <Arduino.h>
#include <driver/twai.h>

#define CAN_TX_PIN 5
#define CAN_RX_PIN 4
#define SAMPLE_INTERVAL_MS 5000 // 5 seconds
#define ALERT_THRESHOLD_DB 100 
#define ALERT_RETRY_INTERVAL_MS 500 // how often to resend unacked alert messages

#define DEBUG_MODE 1

// alert states 
enum AlertState : uint8_t { 
  /* 
  This is what a cycle should look like for an alert: 
  1. Alert condition triggered. Immediately tries to send an alert message via CAN to gateway node.
  2. Waits for gateway to ACK with a timeout of 500 ms. If ACKed within timeout, OK. Go back to sampling. Else, keep retrying.
  3. If subsequent noise levels are lower (ie. don't trigger an ALERT), then won't send anything to gateway. 
  WE ONLY SEND ALERTS IF SOMETHING HAPPENED. 
  4. There would be another task running that should keep track if we receive an ALERT_CLEARED. 
  Which means, we ignore subsueqnet alert readings for the next 2 minutes.
  5. 
  */
}; 


// circular buffer for moving average filter
// must match uint16_t to match heartbeat frame .noise_db
class DataBuffer {
  public: 
    // attributes
    int size; // how many slots we have
    uint16_t* buffer; // pointer to array
    int index; // where currently are in the buffer
 
    DataBuffer(int bufferSize) {
      size = bufferSize;
      index = 0;
      buffer = new uint16_t[size];

      for(int i = 0; i < size; i++) {
        buffer[i] = 0; // init all values to 0 starting out
      }

    } // constructor

    void addSample(uint16_t newValue) {
      // circular buffer, so overwrite 
      buffer[index] = newValue;
      index = (index + 1) % size; 
    }

    uint16_t getAverage() {
      uint32_t  sum = 0;
      for(int i = 0; i < size; i++) {
        sum += buffer[i];
      }
      return uint16_t(sum / size);
    }

    uint16_t getMin() { 
      uint16_t minimum = buffer[0];
      for(int i = 1; i < size; i++) {
        if(buffer[i] < minimum) {
          minimum = buffer[i];
        }
      }
      return minimum;

    }
    
    uint16_t getMax() { 
      uint16_t maximum = buffer[0];
      for(int i = 1; i < size; i++) {
        if(buffer[i] > maximum) {
          maximum = buffer[i];
        }
      }
      return maximum;
    }
};

// create buffer globally
int bufferSize = int(5*60*1000/SAMPLE_INTERVAL_MS); 
DataBuffer noiseBuffer(bufferSize); // number of samples
unsigned long lastSample = 0;


/* 
==================================
CAN-RELATED
==================================
*/

// heartbeat frame
struct __attribute__((packed)) HeartbeatFrame {
  uint16_t noise_db;
  uint16_t reserved[3]; // to make sure 8 bytes in data expected
}; 

// alert frame
struct __attribute__((packed)) AlertFrame {
  uint16_t noise_db;
  uint16_t reserved[3];
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
  // spikes in values are less concerning for noise than continuous exposure - EXCEPT for 'instant damage/hazard' cases
  
  // TO DO: 

  return false;
}

/* 
==================================
MOCKS FOR SENSOR READING 
==================================
*/

uint16_t mockNoiseReading() { 
  return 70; 
}

uint16_t mockAlertNoiseReading() { 
  return 120; // instant damage, always send alerts for values detected > 100 dB 
} // for manual testing, make it so that every 1/5 values read is this 'alert' case to test that alerts can be sent

static int internalCounter = 0;
uint16_t mockReadNoiseSensor() { 
  if (internalCounter == 5) {
    return mockAlertNoiseReading();
    internalCounter = 0; // reset to 0
  }

  else { 
    return mockNoiseReading();
    internalCounter++; // increase by 1
  }
}


bool isAlertNeeded(uint16_t value) {
  // for noise, send alert if > 100 dB 
  if (value >= ALERT_THRESHOLD_DB) {
    return true;
  }
  else {
    return false;
  }
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

  // continuously sample at fixed interval
  if (millis() - lastSample >= SAMPLE_INTERVAL_MS) {
    // continuously sample noise 
    uint16_t curr_reading = mockReadNoiseSensor(); // TO DO: replace with real sensor reading
    bool alert = isAlertNeeded(curr_reading); 
    
    noiseBuffer.addSample(curr_reading);
    lastSample = millis();

    #if DEBUG_MODE
    Serial.printf("Sampled: %d dB", curr_reading);
    #endif
  }

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
      hbFrame.noise_db = noiseBuffer.getAverage();
      memset(hbFrame.reserved, 0, sizeof(hbFrame.reserved));

      if (sendHeartbeatResponse(hbFrame)) {
        Serial.printf("Sent noise level: %d dB in heartbeat response.\n", hbFrame.noise_db);
      };

    }

  }

  else if (status==ESP_ERR_TIMEOUT) { 
    Serial.println("No CAN msg recv'd within timeout.");
  }
  else {
    Serial.println("Error recv. CAN msg.");
    Serial.println(status);
  }

  delay(50); // some delay between loops

}



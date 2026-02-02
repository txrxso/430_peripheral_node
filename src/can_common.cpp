#include "can_common.h"


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

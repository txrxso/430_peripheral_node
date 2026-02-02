#ifndef DATA_BUFFER_H
#define DATA_BUFFER_H

#include <Arduino.h>

// circular buffer for moving average filter
// must match uint16_t to match heartbeat frame .noise_db
class DataBuffer {
  public: 
    // attributes
    int size; // how many slots we have
    uint16_t* buffer; // pointer to array
    int index; // where currently are in the buffer
    int count; // how many samples actually added so far 
 
    DataBuffer(int bufferSize) {
      size = bufferSize;
      index = 0;
      count = 0;
      buffer = new uint16_t[size];

      for(int i = 0; i < size; i++) {
        buffer[i] = 0; // init all values to 0 starting out
      }

    } // constructor

    void addSample(uint16_t newValue) {
      // circular buffer, so overwrite 
      buffer[index] = newValue;
      index = (index + 1) % size; 
      if (count < size) count++ ; // stop incrementing once buffer all filled up
    }

    uint16_t getAverage() {
      uint32_t  sum = 0;
      for(int i = 0; i < size; i++) {
        sum += buffer[i];
      }
      return uint16_t(sum / count); // divide by actual sample count
    }

    uint16_t getMin() { 
      if (count == 0) return 0;
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

#endif
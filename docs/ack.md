# Acknowledgements

## Alert States
```c++
enum AlertState : uint8_t { 
  ALERT_IDLE,      // No active alert
  ALERT_PENDING,   // Alert sent, waiting for gateway ACK
  ALERT_ACKED,     // Gateway acknowledged receipt
  ALERT_CLEAR,     // Manual suppression from gateway
}; 
```

## Operating Modes 
Controlled by `#define ENABLE_ACK`
0 : UDP-like (fire and forget)
1 : TCP-like (ACK-based)

### ACK-based (Reliable Delivery)

1. Initial Alert (will be triggered in `handleSampling()`): send alert and set state to `ALERT_PENDING`. Alert vaue not added to buffer.
2. Retry Logic (handled in `handleAlertState`): while state is `ALERT_PENDING`, retry with exponential backoff. 
3. ACK Received: gateway sends `ALERT_ACK` message and reset state to `ALERT_IDLE`. Reset all backoff-related values. Next high sample will trigger a new alert.


#ifndef CAN_CONFIG_H
#define CAN_CONFIG_H

// in the future, assign response like TX + node id or something
#define ISOTP_TX_ID 0x100
#define ISOTP_RX_ID 0x200

// max time to wait for the peer during a blocking tx/rx before giving up
#define ISOTP_TIMEOUT_MS 1000


#endif
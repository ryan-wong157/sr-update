#ifndef FAULT_PAUSE_H
#define FAULT_PAUSE_H

// TEMP fault tolerance testing. Stalls at a trailer write or swap step with the LED on so the
// power can be pulled by hand at exactly that point. The LED goes off for a moment after each
// pause so back to back pauses can still be counted.

#define CFG_FAULT_PAUSE_ON_MS 1500
#define CFG_FAULT_PAUSE_OFF_MS 300

void fault_pause(void);

#endif

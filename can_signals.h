#ifndef __CAN_SIGNALS_H
#define __CAN_SIGNALS_H

#include <linux/can.h>
#include <stdbool.h>

enum CanSignalType {
    CAN_SIGNAL_BOOL = 0,
    CAN_SIGNAL_UINT8,
    CAN_SIGNAL_UINT16
};

struct CanSignal {
    canid_t canId;
    uint32_t sigId;
    const char *sigName;
    uint8_t start;
    uint8_t end;
    uint32_t min;
    uint32_t max;
    enum CanSignalType type;
};

extern struct CanSignal geniviDemoSignals[];

extern uint8_t extractSignalUInt8(const struct CanSignal *sigDesc, const struct can_frame* frame);
extern uint16_t extractSignalUInt16(const struct CanSignal *sigDesc, const struct can_frame* frame);
extern bool extractSignalBool(const struct CanSignal *sigDesc, const struct can_frame* frame);

#endif /* !__CAN_SIGNALS_H */

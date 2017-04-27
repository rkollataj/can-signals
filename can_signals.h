#ifndef __CAN_SIGNALS_H
#define __CAN_SIGNALS_H

#include <stdint.h>
#include <stdbool.h>
#include <linux/can.h>

enum CanSignalType {
    CAN_SIGNAL_BOOL = 0,
    CAN_SIGNAL_UINT8,
    CAN_SIGNAL_UINT16
};

struct CanSignal {
    uint32_t canId;
    uint32_t sigId;
    const char *sigName;
    uint8_t start;
    uint8_t end;
    uint32_t min;
    uint32_t max;
    enum CanSignalType type;
};

typedef void (*signalUInt8Clbk)(const char *name, uint32_t id, uint8_t value);
typedef void (*signalUInt16Clbk)(const char *name, uint32_t id, uint16_t value);
typedef void (*signalBoolClbk)(const char *name, uint32_t id, bool value);
typedef void (*signalEncoded)(uint32_t id, uint8_t dlc, const uint8_t* data);
typedef void (*siglog_t)(int priority, const char* format, ...);

bool initCanSignals(const struct CanSignal *signals, uint32_t signals_cnt, siglog_t siglog, signalBoolClbk boolClbk, signalUInt8Clbk uint8Clbk, signalUInt16Clbk uint16Clbk);
void processCanFrame(const struct can_frame* frame);
void sendUInt8Signal(const char* name, uint8_t val);
void sendUInt16Signal(const char* name, uint16_t val);
void sendBoolSignal(const char* name, bool val);

#endif /* !__CAN_SIGNALS_H */

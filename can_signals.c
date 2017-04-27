#include "can_signals.h"
#include <string.h>
#include <syslog.h>

static const struct CanSignal* sig;
static uint32_t sigCnt;
static struct can_frame* frameValues;
static uint32_t frameValuesCnt;
static signalBoolClbk sigBoolClbk;
static signalUInt8Clbk sigUInt8Clbk;
static signalUInt16Clbk sigUInt16Clbk;
static bool decoderInitialized = false;
static bool encoderInitialized = false;
static siglog_t syslogger;
static signalEncoded sigEnc;

static uint32_t bitMask[] = {
    0x00000001,
    0x00000003,
    0x00000007,
    0x0000000f,
    0x0000001f,
    0x0000003f,
    0x0000007f,
    0x000000ff,
    0x000001ff,
    0x000003ff,
    0x000007ff,
    0x00000fff,
    0x00001fff,
    0x00003fff,
    0x00007fff,
    0x0000ffff,
};

bool initSignalsDecoder(const struct CanSignal* signals, uint32_t signals_cnt, siglog_t siglog, signalBoolClbk boolClbk, signalUInt8Clbk uint8Clbk, signalUInt16Clbk uint16Clbk)
{
    if (signals && siglog) {
        decoderInitialized = true;
        sig = signals;
        sigCnt = signals_cnt;
        syslogger = siglog;
        sigBoolClbk = boolClbk;
        sigUInt8Clbk = uint8Clbk;
        sigUInt16Clbk = uint16Clbk;
    }

    syslogger(LOG_INFO, "Signal extraction initialization (%hhu)", decoderInitialized);

    return decoderInitialized;
}

bool initiSignalEncoder(struct can_frame* fVals, uint32_t fValsCnt, siglog_t siglog, signalEncoded sigEncClbk)
{
    if (fVals && fValsCnt && siglog && sigEncClbk) {
        encoderInitialized = true;
        frameValues = fVals;
        frameValuesCnt = fValsCnt;
        syslogger = siglog;
        sigEnc = sigEncClbk;
    }

    return encoderInitialized;
}

static void processBoolSignal(const struct can_frame* frame, const struct CanSignal* sigDesc)
{
    uint8_t byteNum = sigDesc->start / 8;
    uint8_t valShift = sigDesc->start % 8;
    uint32_t mask = bitMask[sigDesc->end - sigDesc->start];
    uint8_t val;

    if (sigBoolClbk) {
        val = frame->data[byteNum];
        val >>= valShift;
        val = val & mask;

        if ((val >= sigDesc->min) && (val <= sigDesc->max)) {
            sigBoolClbk(sigDesc->sigName, sigDesc->sigId, (bool)val);
        } else {
            syslogger(LOG_WARNING, "%s(%u): value (%hhu) out of bounds min: %u, max: %u",
                sigDesc->sigName, sigDesc->sigId, val, sigDesc->min, sigDesc->max);
        }
    }
}

static void processUInt8Signal(const struct can_frame* frame, const struct CanSignal* sigDesc)
{
    uint8_t byteNum = sigDesc->start / 8;
    uint8_t valShift = sigDesc->start % 8;
    uint32_t mask = bitMask[sigDesc->end - sigDesc->start];
    uint8_t val;

    if (sigUInt8Clbk) {
        val = frame->data[byteNum];
        val >>= valShift;
        val = val & mask;

        if ((val >= sigDesc->min) && (val <= sigDesc->max)) {
            sigUInt8Clbk(sigDesc->sigName, sigDesc->sigId, val);
        } else {
            syslogger(LOG_WARNING, "%s(%d): value (%hhu) out of bounds min: %u, max: %u",
                sigDesc->sigName, sigDesc->sigId, val, sigDesc->min, sigDesc->max);
        }
    }
}

static void processUInt16Signal(const struct can_frame* frame, const struct CanSignal* sigDesc)
{
    uint8_t byteNum = sigDesc->start / 8;
    uint8_t valShift = sigDesc->start % 8;
    uint32_t mask = bitMask[sigDesc->end - sigDesc->start];
    uint16_t val;

    if (sigUInt16Clbk) {
        val = *((uint16_t*)&frame->data[byteNum]);
        val >>= valShift;
        val = val & mask;

        if ((val >= sigDesc->min) && (val <= sigDesc->max)) {
            sigUInt16Clbk(sigDesc->sigName, sigDesc->sigId, val);
        } else {
            syslogger(LOG_WARNING, "%s(%d): value (%hu) out of bounds min: %u, max: %u",
                sigDesc->sigName, sigDesc->sigId, val, sigDesc->min, sigDesc->max);
        }
    }
}

static const struct CanSignal* findSigDesc(const char* name)
{
    const struct CanSignal* desc = 0;

    /* not very efficient way but will be ok for the demo */
    for (uint32_t i = 0; i < sigCnt; ++i) {
        if (!strcmp(sig[i].sigName, name)) {
            desc = &sig[i];
            break;
        }
    }

    if (!desc) {
        syslogger(LOG_WARNING, "Signal '%s' not found", name);
    }

    return desc;
}

static struct can_frame* findCanBuffer(uint32_t canId)
{
    struct can_frame* frame = 0;

    for (uint32_t i = 0; i < frameValuesCnt; ++i) {
        if (frameValues[i].can_id == canId) {
            frame = &frameValues[i];
            break;
        }
    }

    if (!frame) {
        syslogger(LOG_WARNING, "Frame '%u' not found", canId);
    }

    return frame;
}

void sendUInt8Signal(const char* name, uint8_t val)
{
    const struct CanSignal* sigDesc = findSigDesc(name);
    struct can_frame* frame = 0;

    if (!sigDesc) {
        return;
    }

    frame = findCanBuffer(sigDesc->canId);
    if (!frame) {
        return;
    }

    if (encoderInitialized && (sigDesc->type == CAN_SIGNAL_UINT8) && (val >= sigDesc->min) && (val <= sigDesc->max)) {
        uint8_t byteNum = sigDesc->start / 8;
        uint8_t valShift = sigDesc->start % 8;
        uint8_t mask = bitMask[sigDesc->end - sigDesc->start];

        frame->data[byteNum] &= ~(mask << valShift);
        frame->data[byteNum] |= val << valShift;

        sigEnc(frame->can_id, frame->can_dlc, frame->data);
    } else {
        syslogger(LOG_WARNING, "%s(%d) provided val: %hhu, type: %d. Expoected init =: %hhu, min: %hhu, max: %hhu, type: %d",
            name, sigDesc->sigId, val, CAN_SIGNAL_UINT8, encoderInitialized, sigDesc->min, sigDesc->max, sigDesc->type);
    }
}

void sendUInt16Signal(const char* name, uint16_t val)
{
    const struct CanSignal* sigDesc = findSigDesc(name);
    struct can_frame* frame = 0;

    if (!sigDesc) {
        return;
    }

    frame = findCanBuffer(sigDesc->canId);
    if (!frame) {
        return;
    }

    if (encoderInitialized && (sigDesc->type == CAN_SIGNAL_UINT8) && (val >= sigDesc->min) && (val <= sigDesc->max)) {
        uint8_t byteNum = sigDesc->start / 8;
        uint8_t valShift = sigDesc->start % 8;
        uint16_t mask = bitMask[sigDesc->end - sigDesc->start];
        uint16_t* data = (uint16_t*)&frame->data[byteNum];

        *data &= ~(mask << valShift);
        *data |= val << valShift;

        sigEnc(frame->can_id, frame->can_dlc, frame->data);
    } else {
        syslogger(LOG_WARNING, "%s(%d) provided val: %hhu, type: %d. Expoected init =: %hhu, min: %hhu, max: %hhu, type: %d",
            name, sigDesc->sigId, val, CAN_SIGNAL_UINT8, encoderInitialized, sigDesc->min, sigDesc->max, sigDesc->type);
    }
}

void sendBoolSignal(const char* name, bool val)
{
    const struct CanSignal* sigDesc = findSigDesc(name);
    struct can_frame* frame = 0;

    if (!sigDesc) {
        return;
    }

    frame = findCanBuffer(sigDesc->canId);
    if (!frame) {
        return;
    }

    if (encoderInitialized && (sigDesc->type == CAN_SIGNAL_UINT8) && (val >= sigDesc->min) && (val <= sigDesc->max)) {
        uint8_t byteNum = sigDesc->start / 8;
        uint8_t valShift = sigDesc->start % 8;
        uint8_t mask = bitMask[sigDesc->end - sigDesc->start];

        frame->data[byteNum] &= ~(mask << valShift);
        frame->data[byteNum] |= val << valShift;

        sigEnc(frame->can_id, frame->can_dlc, frame->data);
    } else {
        syslogger(LOG_WARNING, "%s(%d) provided val: %hhu, type: %d. Expoected init =: %hhu, min: %hhu, max: %hhu, type: %d",
            name, sigDesc->sigId, val, CAN_SIGNAL_UINT8, encoderInitialized, sigDesc->min, sigDesc->max, sigDesc->type);
    }
}

void processCanFrame(const struct can_frame* frame)
{
    uint32_t i = 0;

    syslogger(LOG_DEBUG, "Can frame processing started can_id: %x, dlc: %hhu", frame->can_id, frame->can_dlc);

    if (decoderInitialized) {
        for (i = 0; i < sigCnt; ++i) {
            if (frame->can_id == sig[i].canId) {
                syslogger(LOG_DEBUG, "Processing signal [%u/%u], %s(%u), s: %hhu, e: %hhu, min: %u, max: %u, type: %d",
                    i + 1, sigCnt, sig[i].sigName, sig[i].sigId, sig[i].start, sig[i].end, sig[i].min, sig[i].max, sig[i].type);

                /* Check if DLC is big enough */
                if (frame->can_dlc * 8 >= sig[i].end) {
                    switch (sig[i].type) {

                    case CAN_SIGNAL_BOOL:
                        processBoolSignal(frame, &sig[i]);
                        break;

                    case CAN_SIGNAL_UINT8:
                        processUInt8Signal(frame, &sig[i]);
                        break;

                    case CAN_SIGNAL_UINT16:
                        processUInt16Signal(frame, &sig[i]);
                        break;

                    default:
                        syslogger(LOG_WARNING, "Signal type (%d) not supported", sig[i].type);
                        break;
                    }

                } else {
                    syslogger(LOG_WARNING, "DLC(%hhu) too small to process the signal", frame->can_dlc);
                }
            }
        }
    } else {
        syslogger(LOG_WARNING, "Signal extraction not decoderInitialized");
    }
}

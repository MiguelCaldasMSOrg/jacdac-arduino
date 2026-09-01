#pragma once

#include "JacdacProtocol.h"

namespace jacdac {

using TransportReceiveHandler = void (*)(const Frame &frame, void *context);
using TransportTransmitHandler = void (*)(void *context);

struct TransportDiagnostics {
    volatile uint32_t fallingEdges;
    volatile uint32_t receiveStarts;
    volatile uint32_t receiveCompletions;
    volatile uint32_t receiveBytes;
    volatile uint32_t receiveTimeouts;
    volatile uint32_t receiveShortFrames;
    volatile uint32_t receiveInvalidFrames;
    volatile uint32_t receiveHardwareErrors;
};

class NrfTransport {
public:
    static NrfTransport &instance();
    bool begin(uint8_t pin, TransportReceiveHandler receiveHandler, TransportTransmitHandler transmitHandler, void *context);
    void end();
    bool send(const Frame &frame);
    uint64_t deviceIdentifier() const;
    uint32_t busErrors() const;
    uint32_t collisions() const;
    const TransportDiagnostics &diagnostics() const;

    void handleLineFalling();
    void handleTimer();
#if defined(NRF52833_XXAA)
    void handleUarte();
#elif defined(NRF51)
    void handleUart();
#endif
#if defined(JACDAC_TEST)
    void injectFrame(const Frame &frame);
    bool takeSentFrame(Frame &frame);
    void completeTransmit();
#endif

private:
    NrfTransport();
    void schedule(uint32_t microseconds);
    void cancelSchedule();
    void scheduleTransmit();
    void startTransmit();
    void startReceive();
    void finishReceive(bool timeout);
#if defined(NRF52833_XXAA)
    void completeReceive();
#endif
    void finishTransmit();
    void configureInput();
    void configureUarteReceive();
    void configureUarteTransmit();
    bool lineHigh() const;
    void driveLine(bool high);
    uint32_t randomAround(uint32_t value);

    enum State : uint8_t { STOPPED, IDLE, WAITING_TO_TRANSMIT, RECEIVING, STOPPING_RECEIVE, TRANSMITTING };
    volatile State state_;
    volatile uint8_t timerPurpose_;
    uint8_t arduinoPin_;
    uint32_t gpioPin_;
    Frame receiveFrame_;
    Frame transmitFrame_;
    volatile bool transmitPending_;
#if defined(NRF52833_XXAA)
    volatile bool receiveTimedOut_;
#endif
#if defined(NRF51)
    volatile size_t receiveLength_;
    volatile size_t transmitOffset_;
#endif
    TransportReceiveHandler receiveHandler_;
    TransportTransmitHandler transmitHandler_;
    void *context_;
    uint32_t randomState_;
    volatile uint32_t busErrors_;
    volatile uint32_t collisions_;
    TransportDiagnostics diagnostics_;
};

} // namespace jacdac
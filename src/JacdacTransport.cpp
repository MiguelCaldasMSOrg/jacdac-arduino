#include "JacdacTransport.h"

#include <Arduino.h>
#include <string.h>

#if defined(NRF52833_XXAA) || defined(NRF51)
#include <nrf.h>
#include <WVariant.h>
#endif

namespace jacdac {

enum TimerPurpose : uint8_t { TIMER_NONE, TIMER_TRANSMIT, TIMER_RX_HEADER, TIMER_RX_FRAME
#if defined(NRF51)
    , TIMER_TX_END
#endif
};

#if defined(NRF52833_XXAA)
static NRF_GPIO_Type *gpioPort(uint32_t pin) {
    return pin < 32 ? NRF_P0 : NRF_P1;
}

static uint32_t gpioIndex(uint32_t pin) {
    return pin & 31;
}
#endif

static void lineFallingThunk() {
    NrfTransport::instance().handleLineFalling();
}

NrfTransport &NrfTransport::instance() {
    static NrfTransport transport;
    return transport;
}

NrfTransport::NrfTransport() : state_(STOPPED), timerPurpose_(TIMER_NONE), arduinoPin_(0), gpioPin_(0), transmitPending_(false)
#if defined(NRF52833_XXAA)
    , receiveTimedOut_(false)
#endif
#if defined(NRF51)
    , receiveLength_(0), transmitOffset_(0)
#endif
    , receiveHandler_(nullptr), transmitHandler_(nullptr), context_(nullptr), randomState_(0x6d2b79f5), busErrors_(0), collisions_(0) {
    memset(&receiveFrame_, 0, sizeof(receiveFrame_));
    memset(&transmitFrame_, 0, sizeof(transmitFrame_));
    memset(&diagnostics_, 0, sizeof(diagnostics_));
}

bool NrfTransport::begin(uint8_t pin, TransportReceiveHandler receiveHandler, TransportTransmitHandler transmitHandler, void *context) {
#if defined(NRF52833_XXAA) || defined(NRF51)
    if (state_ != STOPPED) {
        return false;
    }
    if (pin >= PINS_COUNT || g_ADigitalPinMap[pin] == static_cast<uint32_t>(-1)) {
        return false;
    }
    arduinoPin_ = pin;
    gpioPin_ = g_ADigitalPinMap[pin];
    receiveHandler_ = receiveHandler;
    transmitHandler_ = transmitHandler;
    context_ = context;
    randomState_ ^= NRF_FICR->DEVICEID[0] ^ micros();
    busErrors_ = 0;
    collisions_ = 0;
    memset(&diagnostics_, 0, sizeof(diagnostics_));
    transmitPending_ = false;

#if defined(NRF52833_XXAA)
    NRF_TIMER3->TASKS_STOP = 1;
    NRF_TIMER3->MODE = TIMER_MODE_MODE_Timer;
    NRF_TIMER3->BITMODE = TIMER_BITMODE_BITMODE_32Bit;
    NRF_TIMER3->PRESCALER = 4;
    NRF_TIMER3->TASKS_CLEAR = 1;
    NRF_TIMER3->EVENTS_COMPARE[0] = 0;
    NRF_TIMER3->INTENCLR = 0xffffffff;
    NVIC_SetPriority(TIMER3_IRQn, 2);
    NVIC_ClearPendingIRQ(TIMER3_IRQn);
    NVIC_EnableIRQ(TIMER3_IRQn);
    NRF_TIMER3->TASKS_START = 1;

    NRF_UARTE1->ENABLE = UARTE_ENABLE_ENABLE_Disabled;
    NRF_UARTE1->BAUDRATE = UARTE_BAUDRATE_BAUDRATE_Baud1M;
    NRF_UARTE1->CONFIG = (UARTE_CONFIG_HWFC_Disabled << UARTE_CONFIG_HWFC_Pos) | (UARTE_CONFIG_PARITY_Excluded << UARTE_CONFIG_PARITY_Pos);
    NRF_UARTE1->PSEL.CTS = 0xffffffff;
    NRF_UARTE1->PSEL.RTS = 0xffffffff;
    NRF_UARTE1->INTENCLR = 0xffffffff;
    NVIC_SetPriority(UARTE1_IRQn, 1);
    NVIC_ClearPendingIRQ(UARTE1_IRQn);
    NVIC_EnableIRQ(UARTE1_IRQn);
#else
    NRF_TIMER2->TASKS_STOP = 1;
    NRF_TIMER2->MODE = TIMER_MODE_MODE_Timer;
    NRF_TIMER2->BITMODE = TIMER_BITMODE_BITMODE_16Bit;
    NRF_TIMER2->PRESCALER = 4;
    NRF_TIMER2->TASKS_CLEAR = 1;
    NRF_TIMER2->EVENTS_COMPARE[0] = 0;
    NRF_TIMER2->INTENCLR = 0xffffffff;
    NVIC_SetPriority(TIMER2_IRQn, 2);
    NVIC_ClearPendingIRQ(TIMER2_IRQn);
    NVIC_EnableIRQ(TIMER2_IRQn);
    NRF_TIMER2->TASKS_START = 1;

    NRF_UART0->ENABLE = UART_ENABLE_ENABLE_Disabled;
    NRF_UART0->BAUDRATE = UART_BAUDRATE_BAUDRATE_Baud1M;
    NRF_UART0->CONFIG = (UART_CONFIG_PARITY_Excluded << UART_CONFIG_PARITY_Pos) | UART_CONFIG_HWFC_Disabled;
    NRF_UART0->PSELCTS = 0xffffffff;
    NRF_UART0->PSELRTS = 0xffffffff;
    NRF_UART0->INTENCLR = 0xffffffff;
    NVIC_SetPriority(UART0_IRQn, 1);
    NVIC_ClearPendingIRQ(UART0_IRQn);
    NVIC_EnableIRQ(UART0_IRQn);
#endif

    configureInput();
    attachInterrupt(arduinoPin_, lineFallingThunk, FALLING);
    state_ = IDLE;
    return true;
#else
    (void)pin;
    (void)receiveHandler;
    (void)transmitHandler;
    (void)context;
    return false;
#endif
}

void NrfTransport::end() {
#if defined(NRF52833_XXAA) || defined(NRF51)
    noInterrupts();
    state_ = STOPPED;
    timerPurpose_ = TIMER_NONE;
    transmitPending_ = false;
#if defined(NRF52833_XXAA)
    NRF_TIMER3->TASKS_STOP = 1;
    NRF_TIMER3->INTENCLR = 0xffffffff;
    NRF_UARTE1->TASKS_STOPRX = 1;
    NRF_UARTE1->TASKS_STOPTX = 1;
    NRF_UARTE1->INTENCLR = 0xffffffff;
    NRF_UARTE1->ENABLE = UARTE_ENABLE_ENABLE_Disabled;
    NVIC_DisableIRQ(TIMER3_IRQn);
    NVIC_DisableIRQ(UARTE1_IRQn);
#else
    NRF_TIMER2->TASKS_STOP = 1;
    NRF_TIMER2->INTENCLR = 0xffffffff;
    NRF_UART0->TASKS_STOPRX = 1;
    NRF_UART0->TASKS_STOPTX = 1;
    NRF_UART0->INTENCLR = 0xffffffff;
    NRF_UART0->ENABLE = UART_ENABLE_ENABLE_Disabled;
    NRF_UART0->PSELRXD = 0xffffffff;
    NRF_UART0->PSELTXD = 0xffffffff;
    NVIC_DisableIRQ(TIMER2_IRQn);
    NVIC_DisableIRQ(UART0_IRQn);
#endif
    interrupts();
    detachInterrupt(arduinoPin_);
    configureInput();
#else
    state_ = STOPPED;
#endif
}

bool NrfTransport::send(const Frame &frame) {
#if defined(NRF52833_XXAA) || defined(NRF51)
    if (state_ == STOPPED || transmitPending_ || !validateFrame(frame, frameSize(frame))) {
        return false;
    }
    noInterrupts();
    transmitFrame_ = frame;
    transmitPending_ = true;
    if (state_ == IDLE) {
        scheduleTransmit();
    }
    interrupts();
    return true;
#else
    (void)frame;
    return false;
#endif
}

uint32_t NrfTransport::busErrors() const {
    return busErrors_;
}

uint64_t NrfTransport::deviceIdentifier() const {
#if defined(NRF52833_XXAA) || defined(NRF51)
    return (static_cast<uint64_t>(NRF_FICR->DEVICEID[1]) << 32) | NRF_FICR->DEVICEID[0];
#else
    return 0;
#endif
}

uint32_t NrfTransport::collisions() const {
    return collisions_;
}

const TransportDiagnostics &NrfTransport::diagnostics() const {
    return diagnostics_;
}

void NrfTransport::handleLineFalling() {
#if defined(NRF52833_XXAA) || defined(NRF51)
    ++diagnostics_.fallingEdges;
    if (state_ == IDLE || state_ == WAITING_TO_TRANSMIT) {
        timerPurpose_ = TIMER_NONE;
        cancelSchedule();
        startReceive();
    }
#endif
}

void NrfTransport::handleTimer() {
#if defined(NRF52833_XXAA)
    if (NRF_TIMER3->EVENTS_COMPARE[0] == 0) {
        return;
    }
    cancelSchedule();
    const uint8_t purpose = timerPurpose_;
    timerPurpose_ = TIMER_NONE;
    if (purpose == TIMER_TRANSMIT && state_ == WAITING_TO_TRANSMIT) {
        startTransmit();
    } else if (purpose == TIMER_RX_HEADER && state_ == RECEIVING) {
        const uint32_t amount = NRF_UARTE1->RXD.AMOUNT;
        if (amount < 4) {
            finishReceive(true);
        } else {
            const size_t expected = frameSize(receiveFrame_);
            if (expected < SERIAL_HEADER_SIZE || expected > sizeof(Frame)) {
                finishReceive(true);
            } else {
                schedule(static_cast<uint32_t>(expected * 12 + 60));
                timerPurpose_ = TIMER_RX_FRAME;
            }
        }
    } else if (purpose == TIMER_RX_FRAME && state_ == RECEIVING) {
        finishReceive(true);
    }
#elif defined(NRF51)
    if (NRF_TIMER2->EVENTS_COMPARE[0] == 0) {
        return;
    }
    cancelSchedule();
    const uint8_t purpose = timerPurpose_;
    timerPurpose_ = TIMER_NONE;
    if (purpose == TIMER_TRANSMIT && state_ == WAITING_TO_TRANSMIT) {
        startTransmit();
    } else if (purpose == TIMER_RX_HEADER && state_ == RECEIVING) {
        if (receiveLength_ < SERIAL_HEADER_SIZE) {
            finishReceive(true);
        } else {
            const size_t expected = frameSize(receiveFrame_);
            if (expected < SERIAL_HEADER_SIZE || expected > sizeof(Frame)) {
                finishReceive(true);
            } else {
                schedule(static_cast<uint32_t>((expected - receiveLength_) * 12 + 60));
                timerPurpose_ = TIMER_RX_FRAME;
            }
        }
    } else if (purpose == TIMER_RX_FRAME && state_ == RECEIVING) {
        finishReceive(true);
    } else if (purpose == TIMER_TX_END && state_ == TRANSMITTING) {
        finishTransmit();
    }
#endif
}

#if defined(NRF52833_XXAA)
void NrfTransport::handleUarte() {
    if (NRF_UARTE1->EVENTS_ERROR != 0) {
        NRF_UARTE1->EVENTS_ERROR = 0;
        NRF_UARTE1->ERRORSRC = NRF_UARTE1->ERRORSRC;
        if (state_ == RECEIVING) {
            ++diagnostics_.receiveHardwareErrors;
            finishReceive(false);
        } else {
            ++busErrors_;
        }
    }
    if (NRF_UARTE1->EVENTS_ENDRX != 0) {
        NRF_UARTE1->EVENTS_ENDRX = 0;
        if (state_ == RECEIVING) {
            finishReceive(false);
        }
    }
    if (NRF_UARTE1->EVENTS_RXTO != 0) {
        NRF_UARTE1->EVENTS_RXTO = 0;
        if (state_ == STOPPING_RECEIVE) {
            completeReceive();
        }
    }
    if (NRF_UARTE1->EVENTS_ENDTX != 0) {
        NRF_UARTE1->EVENTS_ENDTX = 0;
        if (state_ == TRANSMITTING) {
            finishTransmit();
        }
    }
}
#endif

#if defined(NRF51)
void NrfTransport::handleUart() {
    if (NRF_UART0->EVENTS_ERROR != 0) {
        NRF_UART0->EVENTS_ERROR = 0;
        NRF_UART0->ERRORSRC = NRF_UART0->ERRORSRC;
        if (state_ == RECEIVING) {
            ++diagnostics_.receiveHardwareErrors;
            finishReceive(false);
        } else {
            ++busErrors_;
        }
    }
    if (NRF_UART0->EVENTS_RXDRDY != 0) {
        NRF_UART0->EVENTS_RXDRDY = 0;
        const uint8_t value = NRF_UART0->RXD;
        if (state_ == RECEIVING && receiveLength_ < sizeof(receiveFrame_)) {
            reinterpret_cast<uint8_t *>(&receiveFrame_)[receiveLength_++] = value;
            if (receiveLength_ >= SERIAL_HEADER_SIZE) {
                const size_t expected = frameSize(receiveFrame_);
                if (expected >= SERIAL_HEADER_SIZE && expected <= sizeof(Frame) && receiveLength_ == expected) {
                    finishReceive(false);
                }
            }
        }
    }
    if (NRF_UART0->EVENTS_TXDRDY != 0) {
        NRF_UART0->EVENTS_TXDRDY = 0;
        if (state_ == TRANSMITTING) {
            const size_t length = frameSize(transmitFrame_);
            if (transmitOffset_ < length) {
                NRF_UART0->TXD = reinterpret_cast<const uint8_t *>(&transmitFrame_)[transmitOffset_++];
            } else {
                NRF_UART0->INTENCLR = UART_INTENCLR_TXDRDY_Msk;
                timerPurpose_ = TIMER_TX_END;
                schedule(12);
            }
        }
    }
}
#endif

void NrfTransport::schedule(uint32_t microseconds) {
#if defined(NRF52833_XXAA)
    NRF_TIMER3->TASKS_CAPTURE[1] = 1;
    NRF_TIMER3->CC[0] = NRF_TIMER3->CC[1] + microseconds;
    NRF_TIMER3->EVENTS_COMPARE[0] = 0;
    NRF_TIMER3->INTENSET = TIMER_INTENSET_COMPARE0_Msk;
#elif defined(NRF51)
    NRF_TIMER2->TASKS_CAPTURE[1] = 1;
    NRF_TIMER2->CC[0] = (NRF_TIMER2->CC[1] + microseconds) & 0xffffUL;
    NRF_TIMER2->EVENTS_COMPARE[0] = 0;
    NRF_TIMER2->INTENSET = TIMER_INTENSET_COMPARE0_Msk;
#else
    (void)microseconds;
#endif
}

void NrfTransport::cancelSchedule() {
#if defined(NRF52833_XXAA)
    NRF_TIMER3->INTENCLR = TIMER_INTENCLR_COMPARE0_Msk;
    NRF_TIMER3->EVENTS_COMPARE[0] = 0;
#elif defined(NRF51)
    NRF_TIMER2->INTENCLR = TIMER_INTENCLR_COMPARE0_Msk;
    NRF_TIMER2->EVENTS_COMPARE[0] = 0;
#endif
}

void NrfTransport::scheduleTransmit() {
    state_ = WAITING_TO_TRANSMIT;
    timerPurpose_ = TIMER_TRANSMIT;
    schedule(randomAround(150));
}

void NrfTransport::startTransmit() {
#if defined(NRF52833_XXAA) || defined(NRF51)
    if (!transmitPending_) {
        state_ = IDLE;
        return;
    }
    configureInput();
    if (!lineHigh()) {
        ++collisions_;
        state_ = IDLE;
        startReceive();
        return;
    }
    state_ = TRANSMITTING;
    driveLine(false);
    delayMicroseconds(11);
    driveLine(true);
    delayMicroseconds(50);
    configureUarteTransmit();
#if defined(NRF52833_XXAA)
    NRF_UARTE1->TXD.PTR = reinterpret_cast<uint32_t>(&transmitFrame_);
    NRF_UARTE1->TXD.MAXCNT = frameSize(transmitFrame_);
    NRF_UARTE1->EVENTS_ENDTX = 0;
    NRF_UARTE1->INTENCLR = 0xffffffff;
    NRF_UARTE1->INTENSET = UARTE_INTENSET_ENDTX_Msk | UARTE_INTENSET_ERROR_Msk;
    NRF_UARTE1->TASKS_STARTTX = 1;
#else
    transmitOffset_ = 1;
    NRF_UART0->EVENTS_TXDRDY = 0;
    NRF_UART0->EVENTS_ERROR = 0;
    NRF_UART0->INTENCLR = 0xffffffff;
    NRF_UART0->INTENSET = UART_INTENSET_TXDRDY_Msk | UART_INTENSET_ERROR_Msk;
    NRF_UART0->TASKS_STARTTX = 1;
    NRF_UART0->TXD = reinterpret_cast<const uint8_t *>(&transmitFrame_)[0];
#endif
#endif
}

void NrfTransport::startReceive() {
#if defined(NRF52833_XXAA) || defined(NRF51)
    state_ = RECEIVING;
    ++diagnostics_.receiveStarts;
    configureInput();
    memset(&receiveFrame_, 0, sizeof(receiveFrame_));
#if defined(NRF51)
    receiveLength_ = 0;
#endif
    uint32_t timeout = 1000;
    while (!lineHigh() && timeout-- != 0) {
        __NOP();
    }
    if (timeout == 0) {
        ++diagnostics_.receiveTimeouts;
        ++busErrors_;
        state_ = IDLE;
        if (transmitPending_) {
            scheduleTransmit();
        }
        return;
    }
    configureUarteReceive();
#if defined(NRF52833_XXAA)
    NRF_UARTE1->RXD.PTR = reinterpret_cast<uint32_t>(&receiveFrame_);
    NRF_UARTE1->RXD.MAXCNT = sizeof(receiveFrame_);
    NRF_UARTE1->EVENTS_ENDRX = 0;
    NRF_UARTE1->EVENTS_ERROR = 0;
    NRF_UARTE1->EVENTS_RXTO = 0;
    NRF_UARTE1->INTENCLR = 0xffffffff;
    NRF_UARTE1->INTENSET = UARTE_INTENSET_ENDRX_Msk | UARTE_INTENSET_ERROR_Msk;
    NRF_UARTE1->TASKS_STARTRX = 1;
#else
    NRF_UART0->EVENTS_RXDRDY = 0;
    NRF_UART0->EVENTS_ERROR = 0;
    NRF_UART0->INTENCLR = 0xffffffff;
    NRF_UART0->INTENSET = UART_INTENSET_RXDRDY_Msk | UART_INTENSET_ERROR_Msk;
    NRF_UART0->TASKS_STARTRX = 1;
#endif
    timerPurpose_ = TIMER_RX_HEADER;
    schedule(250);
#endif
}

void NrfTransport::finishReceive(bool timeout) {
#if defined(NRF52833_XXAA) || defined(NRF51)
    timerPurpose_ = TIMER_NONE;
    cancelSchedule();
#if defined(NRF52833_XXAA)
    if (state_ == STOPPING_RECEIVE) {
        receiveTimedOut_ = receiveTimedOut_ || timeout;
        return;
    }
    state_ = STOPPING_RECEIVE;
    receiveTimedOut_ = timeout;
    NRF_UARTE1->INTENCLR = 0xffffffff;
    NRF_UARTE1->EVENTS_RXTO = 0;
    NRF_UARTE1->INTENSET = UARTE_INTENSET_RXTO_Msk;
    NRF_UARTE1->TASKS_STOPRX = 1;
#else
    NRF_UART0->INTENCLR = 0xffffffff;
    NRF_UART0->TASKS_STOPRX = 1;
    const size_t received = receiveLength_;
    NRF_UART0->ENABLE = UART_ENABLE_ENABLE_Disabled;
    NRF_UART0->PSELRXD = 0xffffffff;
    configureInput();
    state_ = IDLE;
    ++diagnostics_.receiveCompletions;
    diagnostics_.receiveBytes += received;
    if (timeout) {
        ++diagnostics_.receiveTimeouts;
    }
    if (validateFrame(receiveFrame_, received)) {
        if (receiveHandler_ != nullptr) {
            receiveHandler_(receiveFrame_, context_);
        }
    } else if (received != 0 || timeout) {
        if (received < SERIAL_HEADER_SIZE || (received >= SERIAL_HEADER_SIZE && received < frameSize(receiveFrame_))) {
            ++diagnostics_.receiveShortFrames;
        } else {
            ++diagnostics_.receiveInvalidFrames;
        }
        ++busErrors_;
    }
    if (transmitPending_) {
        scheduleTransmit();
    }
#endif
#else
    (void)timeout;
#endif
}

#if defined(NRF52833_XXAA)
void NrfTransport::completeReceive() {
    NRF_UARTE1->INTENCLR = 0xffffffff;
    const size_t received = NRF_UARTE1->RXD.AMOUNT;
    NRF_UARTE1->ENABLE = UARTE_ENABLE_ENABLE_Disabled;
    NRF_UARTE1->PSEL.RXD = 0xffffffff;
    configureInput();
    state_ = IDLE;
    ++diagnostics_.receiveCompletions;
    diagnostics_.receiveBytes += received;
    if (receiveTimedOut_) {
        ++diagnostics_.receiveTimeouts;
    }
    if (validateFrame(receiveFrame_, received)) {
        if (receiveHandler_ != nullptr) {
            receiveHandler_(receiveFrame_, context_);
        }
    } else if (received != 0 || receiveTimedOut_) {
        if (received < SERIAL_HEADER_SIZE || (received >= SERIAL_HEADER_SIZE && received < frameSize(receiveFrame_))) {
            ++diagnostics_.receiveShortFrames;
        } else {
            ++diagnostics_.receiveInvalidFrames;
        }
        ++busErrors_;
    }
    if (transmitPending_) {
        scheduleTransmit();
    }
}
#endif

void NrfTransport::finishTransmit() {
#if defined(NRF52833_XXAA) || defined(NRF51)
#if defined(NRF52833_XXAA)
    NRF_UARTE1->TASKS_STOPTX = 1;
    NRF_UARTE1->INTENCLR = 0xffffffff;
    NRF_UARTE1->ENABLE = UARTE_ENABLE_ENABLE_Disabled;
    NRF_UARTE1->PSEL.TXD = 0xffffffff;
#else
    NRF_UART0->TASKS_STOPTX = 1;
    NRF_UART0->INTENCLR = 0xffffffff;
    NRF_UART0->ENABLE = UART_ENABLE_ENABLE_Disabled;
    NRF_UART0->PSELTXD = 0xffffffff;
#endif
    driveLine(false);
    delayMicroseconds(11);
    driveLine(true);
    configureInput();
    transmitPending_ = false;
    state_ = IDLE;
    if (transmitHandler_ != nullptr) {
        transmitHandler_(context_);
    }
#endif
}

void NrfTransport::configureInput() {
#if defined(NRF52833_XXAA)
    NRF_GPIO_Type *port = gpioPort(gpioPin_);
    const uint32_t pin = gpioIndex(gpioPin_);
    port->DIRCLR = 1UL << pin;
    port->PIN_CNF[pin] = (GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos) | (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos) | (GPIO_PIN_CNF_PULL_Pullup << GPIO_PIN_CNF_PULL_Pos) | (GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos) | (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos);
#elif defined(NRF51)
    NRF_GPIO->DIRCLR = 1UL << gpioPin_;
    NRF_GPIO->PIN_CNF[gpioPin_] = (GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos) | (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos) | (GPIO_PIN_CNF_PULL_Pullup << GPIO_PIN_CNF_PULL_Pos) | (GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos) | (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos);
#endif
}

void NrfTransport::configureUarteReceive() {
#if defined(NRF52833_XXAA)
    NRF_UARTE1->ENABLE = UARTE_ENABLE_ENABLE_Disabled;
    NRF_UARTE1->PSEL.TXD = 0xffffffff;
    NRF_UARTE1->PSEL.RXD = gpioPin_;
    NRF_UARTE1->ENABLE = UARTE_ENABLE_ENABLE_Enabled;
#elif defined(NRF51)
    NRF_UART0->ENABLE = UART_ENABLE_ENABLE_Disabled;
    NRF_UART0->PSELTXD = 0xffffffff;
    NRF_UART0->PSELRXD = gpioPin_;
    NRF_UART0->ENABLE = UART_ENABLE_ENABLE_Enabled;
#endif
}

void NrfTransport::configureUarteTransmit() {
#if defined(NRF52833_XXAA)
    NRF_UARTE1->ENABLE = UARTE_ENABLE_ENABLE_Disabled;
    NRF_UARTE1->PSEL.RXD = 0xffffffff;
    NRF_UARTE1->PSEL.TXD = gpioPin_;
    NRF_UARTE1->ENABLE = UARTE_ENABLE_ENABLE_Enabled;
#elif defined(NRF51)
    NRF_UART0->ENABLE = UART_ENABLE_ENABLE_Disabled;
    NRF_UART0->PSELRXD = 0xffffffff;
    NRF_UART0->PSELTXD = gpioPin_;
    NRF_UART0->ENABLE = UART_ENABLE_ENABLE_Enabled;
#endif
}

bool NrfTransport::lineHigh() const {
#if defined(NRF52833_XXAA)
    NRF_GPIO_Type *port = gpioPort(gpioPin_);
    return (port->IN & (1UL << gpioIndex(gpioPin_))) != 0;
#elif defined(NRF51)
    return (NRF_GPIO->IN & (1UL << gpioPin_)) != 0;
#else
    return false;
#endif
}

void NrfTransport::driveLine(bool high) {
#if defined(NRF52833_XXAA)
    NRF_GPIO_Type *port = gpioPort(gpioPin_);
    const uint32_t pin = gpioIndex(gpioPin_);
    if (high) {
        port->OUTSET = 1UL << pin;
    } else {
        port->OUTCLR = 1UL << pin;
    }
    port->PIN_CNF[pin] = (GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos) | (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos) | (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos) | (GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos) | (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos);
    port->DIRSET = 1UL << pin;
#elif defined(NRF51)
    if (high) {
        NRF_GPIO->OUTSET = 1UL << gpioPin_;
    } else {
        NRF_GPIO->OUTCLR = 1UL << gpioPin_;
    }
    NRF_GPIO->PIN_CNF[gpioPin_] = (GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos) | (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos) | (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos) | (GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos) | (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos);
    NRF_GPIO->DIRSET = 1UL << gpioPin_;
#else
    (void)high;
#endif
}

uint32_t NrfTransport::randomAround(uint32_t value) {
    randomState_ ^= randomState_ << 13;
    randomState_ ^= randomState_ >> 17;
    randomState_ ^= randomState_ << 5;
    return value - 31 + (randomState_ & 63);
}

} // namespace jacdac

#if defined(NRF52833_XXAA)
extern "C" void TIMER3_IRQHandler(void) {
    jacdac::NrfTransport::instance().handleTimer();
}

extern "C" void UARTE1_IRQHandler(void) {
    jacdac::NrfTransport::instance().handleUarte();
}
#elif defined(NRF51)
extern "C" void TIMER2_IRQHandler(void) {
    jacdac::NrfTransport::instance().handleTimer();
}

extern "C" void UART0_IRQHandler(void) {
    jacdac::NrfTransport::instance().handleUart();
}
#endif
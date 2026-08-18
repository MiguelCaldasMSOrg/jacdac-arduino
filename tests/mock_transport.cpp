#include "../src/JacdacTransport.h"

#include <string.h>

namespace jacdac {

Nrf52Transport &Nrf52Transport::instance() {
    static Nrf52Transport transport;
    return transport;
}

Nrf52Transport::Nrf52Transport() : state_(STOPPED), timerPurpose_(0), arduinoPin_(0), gpioPin_(0), transmitPending_(false), receiveHandler_(nullptr), transmitHandler_(nullptr), context_(nullptr), randomState_(1), busErrors_(0), collisions_(0) {
    memset(&receiveFrame_, 0, sizeof(receiveFrame_));
    memset(&transmitFrame_, 0, sizeof(transmitFrame_));
}

bool Nrf52Transport::begin(uint8_t pin, TransportReceiveHandler receiveHandler, TransportTransmitHandler transmitHandler, void *context) {
    arduinoPin_ = pin;
    receiveHandler_ = receiveHandler;
    transmitHandler_ = transmitHandler;
    context_ = context;
    state_ = IDLE;
    return true;
}

void Nrf52Transport::end() { state_ = STOPPED; }

bool Nrf52Transport::send(const Frame &frame) {
    if (state_ == STOPPED || transmitPending_) return false;
    transmitFrame_ = frame;
    transmitPending_ = true;
    return true;
}

uint64_t Nrf52Transport::deviceIdentifier() const { return 0x1122334455667788ULL; }
uint32_t Nrf52Transport::busErrors() const { return busErrors_; }
uint32_t Nrf52Transport::collisions() const { return collisions_; }
void Nrf52Transport::handleLineFalling() {}
void Nrf52Transport::handleTimer() {}
void Nrf52Transport::injectFrame(const Frame &frame) { if (receiveHandler_ != nullptr) receiveHandler_(frame, context_); }

bool Nrf52Transport::takeSentFrame(Frame &frame) {
    if (!transmitPending_) return false;
    frame = transmitFrame_;
    return true;
}

void Nrf52Transport::completeTransmit() {
    if (!transmitPending_) return;
    transmitPending_ = false;
    if (transmitHandler_ != nullptr) transmitHandler_(context_);
}

}

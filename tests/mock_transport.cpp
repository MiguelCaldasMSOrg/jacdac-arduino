#include "../src/JacdacTransport.h"

#include <string.h>

namespace jacdac {

NrfTransport &NrfTransport::instance() {
    static NrfTransport transport;
    return transport;
}

NrfTransport::NrfTransport() : state_(STOPPED), timerPurpose_(0), arduinoPin_(0), gpioPin_(0), transmitPending_(false), receiveHandler_(nullptr), transmitHandler_(nullptr), context_(nullptr), randomState_(1), busErrors_(0), collisions_(0) {
    memset(&receiveFrame_, 0, sizeof(receiveFrame_));
    memset(&transmitFrame_, 0, sizeof(transmitFrame_));
    memset(&diagnostics_, 0, sizeof(diagnostics_));
}

bool NrfTransport::begin(uint8_t pin, TransportReceiveHandler receiveHandler, TransportTransmitHandler transmitHandler, void *context) {
    if (state_ != STOPPED) return false;
    arduinoPin_ = pin;
    receiveHandler_ = receiveHandler;
    transmitHandler_ = transmitHandler;
    context_ = context;
    state_ = IDLE;
    return true;
}

void NrfTransport::end() { state_ = STOPPED; }

bool NrfTransport::send(const Frame &frame) {
    if (state_ == STOPPED || transmitPending_) return false;
    transmitFrame_ = frame;
    transmitPending_ = true;
    return true;
}

uint64_t NrfTransport::deviceIdentifier() const { return 0x1122334455667788ULL; }
uint32_t NrfTransport::busErrors() const { return busErrors_; }
uint32_t NrfTransport::collisions() const { return collisions_; }
const TransportDiagnostics &NrfTransport::diagnostics() const { return diagnostics_; }
void NrfTransport::handleLineFalling() {}
void NrfTransport::handleTimer() {}
void NrfTransport::injectFrame(const Frame &frame) { if (receiveHandler_ != nullptr) receiveHandler_(frame, context_); }

bool NrfTransport::takeSentFrame(Frame &frame) {
    if (!transmitPending_) return false;
    frame = transmitFrame_;
    return true;
}

void NrfTransport::completeTransmit() {
    if (!transmitPending_) return;
    transmitPending_ = false;
    if (transmitHandler_ != nullptr) transmitHandler_(context_);
}

}

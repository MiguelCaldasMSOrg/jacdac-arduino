#include "../src/JacdacProtocol.h"

#include <assert.h>
#include <string.h>

using namespace jacdac;

static void testKnownCrc() {
    const char input[] = "123456789";
    assert(crc16(input, strlen(input)) == 0x29b1);
}

static void testFrameRoundTrip() {
    Frame frame;
    const uint64_t deviceId = 0x8877665544332211ULL;
    const uint8_t announce[] = {0x00, 0x01, 0x2a, 0x00, 0x63, 0xa2, 0x73, 0x14};
    const uint8_t reading[] = {0x01};
    resetFrame(frame, deviceId, 0);
    assert(appendPacket(frame, SERVICE_INDEX_CONTROL, CMD_ANNOUNCE, announce, sizeof(announce)));
    assert(appendPacket(frame, 1, CMD_GET_REGISTER | 0x181, reading, sizeof(reading)));
    finalizeFrame(frame);
    assert(validateFrame(frame, frameSize(frame)));

    PacketView packet;
    size_t offset = 0;
    assert(packetAt(frame, offset, packet));
    assert(packet.deviceIdentifier == deviceId);
    assert(packet.serviceIndex == 0);
    assert(packet.serviceCommand == CMD_ANNOUNCE);
    assert(packet.dataSize == sizeof(announce));
    assert(packetAt(frame, offset, packet));
    assert(packet.serviceIndex == 1);
    assert(packet.isRegisterGet());
    assert(packet.registerCode() == 0x181);
    assert(packet.data[0] == 1);
    assert(!packetAt(frame, offset, packet));

    frame.data[4] ^= 0x80;
    assert(!validateFrame(frame, frameSize(frame)));
}

static void testBounds() {
    Frame frame;
    uint8_t payload[SERIAL_PAYLOAD_SIZE] = {};
    resetFrame(frame, 1, FRAME_FLAG_COMMAND);
    assert(!appendPacket(frame, 1, 1, nullptr, 1));
    assert(appendPacket(frame, 1, 1, payload, sizeof(payload)));
    assert(!appendPacket(frame, 1, 2));
}

static void testMalformedPacket() {
    Frame frame;
    resetFrame(frame, 1, 0);
    frame.size = 4;
    frame.data[0] = 1;
    finalizeFrame(frame);
    assert(validateFrame(frame, frameSize(frame)));

    PacketView packet;
    size_t offset = 0;
    assert(!packetAt(frame, offset, packet));
    assert(offset == frame.size);
}

int main() {
    testKnownCrc();
    testFrameRoundTrip();
    testBounds();
    testMalformedPacket();
    return 0;
}
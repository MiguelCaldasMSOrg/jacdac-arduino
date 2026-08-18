#include "JacdacProtocol.h"

#include <string.h>

namespace jacdac {

static size_t align4(size_t value) {
    return (value + 3) & ~static_cast<size_t>(3);
}

uint16_t crc16(const void *data, size_t size) {
    const uint8_t *bytes = static_cast<const uint8_t *>(data);
    uint16_t crc = 0xffff;
    while (size-- != 0) {
        uint8_t value = static_cast<uint8_t>((crc >> 8) ^ *bytes++);
        value ^= value >> 4;
        crc = static_cast<uint16_t>((crc << 8) ^ (value << 12) ^ (value << 5) ^ value);
    }
    return crc;
}

size_t frameSize(const Frame &frame) {
    return static_cast<size_t>(frame.size) + FRAME_HEADER_SIZE;
}

bool validateFrame(const Frame &frame, size_t receivedSize) {
    const size_t declaredSize = frameSize(frame);
    if ((frame.flags & FRAME_FLAG_VNEXT) != 0 || frame.size < 4 || declaredSize > sizeof(Frame) || receivedSize < declaredSize) {
        return false;
    }
    return crc16(reinterpret_cast<const uint8_t *>(&frame) + 2, declaredSize - 2) == frame.crc;
}

void resetFrame(Frame &frame, uint64_t deviceIdentifier, uint8_t flags) {
    memset(&frame, 0, sizeof(frame));
    frame.deviceIdentifier = deviceIdentifier;
    frame.flags = flags;
}

bool appendPacket(Frame &frame, uint8_t serviceIndex, uint16_t serviceCommand, const void *data, uint8_t dataSize) {
    if (dataSize != 0 && data == nullptr) {
        return false;
    }
    const size_t packetSize = static_cast<size_t>(dataSize) + 4;
    const size_t paddedSize = align4(packetSize);
    if (static_cast<size_t>(frame.size) + paddedSize > sizeof(frame.data)) {
        return false;
    }
    uint8_t *destination = frame.data + frame.size;
    destination[0] = dataSize;
    destination[1] = serviceIndex;
    destination[2] = static_cast<uint8_t>(serviceCommand);
    destination[3] = static_cast<uint8_t>(serviceCommand >> 8);
    if (dataSize != 0 && data != nullptr) {
        memcpy(destination + 4, data, dataSize);
    }
    if (paddedSize > packetSize) {
        memset(destination + packetSize, 0, paddedSize - packetSize);
    }
    frame.size = static_cast<uint8_t>(frame.size + paddedSize);
    return true;
}

void finalizeFrame(Frame &frame) {
    frame.crc = crc16(reinterpret_cast<const uint8_t *>(&frame) + 2, frameSize(frame) - 2);
}

bool packetAt(const Frame &frame, size_t &offset, PacketView &packet) {
    if (offset + 4 > frame.size) {
        return false;
    }
    const uint8_t *source = frame.data + offset;
    const size_t packetSize = static_cast<size_t>(source[0]) + 4;
    if (offset + packetSize > frame.size) {
        offset = frame.size;
        return false;
    }
    packet.deviceIdentifier = frame.deviceIdentifier;
    packet.data = source + 4;
    packet.serviceCommand = static_cast<uint16_t>(source[2] | (source[3] << 8));
    packet.serviceIndex = source[1] & SERVICE_INDEX_MASK;
    packet.dataSize = source[0];
    packet.flags = frame.flags;
    offset += align4(packetSize);
    return true;
}

static_assert(sizeof(Frame) == MAX_FRAME_SIZE, "Jacdac frame layout must match the wire format");
static_assert(sizeof(PacketHeader) == SERIAL_HEADER_SIZE, "Jacdac packet header must match the wire format");

} // namespace jacdac
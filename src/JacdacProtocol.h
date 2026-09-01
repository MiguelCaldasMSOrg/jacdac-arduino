#pragma once

#include <stddef.h>
#include <stdint.h>
#include "JacdacConfig.h"

namespace jacdac {

constexpr size_t SERIAL_PAYLOAD_SIZE = JACDAC_FRAME_DATA_SIZE - 4;
constexpr size_t SERIAL_HEADER_SIZE = 16;
constexpr size_t FRAME_HEADER_SIZE = 12;
constexpr size_t FRAME_DATA_SIZE = JACDAC_FRAME_DATA_SIZE;
constexpr size_t MAX_FRAME_SIZE = FRAME_HEADER_SIZE + FRAME_DATA_SIZE;
constexpr uint8_t FRAME_FLAG_COMMAND = 0x01;
constexpr uint8_t FRAME_FLAG_ACK_REQUESTED = 0x02;
constexpr uint8_t FRAME_FLAG_IDENTIFIER_IS_SERVICE_CLASS = 0x04;
constexpr uint8_t FRAME_FLAG_VNEXT = 0x80;
constexpr uint8_t SERVICE_INDEX_MASK = 0x3f;
constexpr uint8_t SERVICE_INDEX_CONTROL = 0x00;
constexpr uint8_t SERVICE_INDEX_MAX_REGULAR = 0x3a;
constexpr uint8_t SERVICE_INDEX_BROADCAST = 0x3d;
constexpr uint8_t SERVICE_INDEX_PIPE = 0x3e;
constexpr uint8_t SERVICE_INDEX_CRC_ACK = 0x3f;
constexpr uint16_t CMD_ANNOUNCE = 0x0000;
constexpr uint16_t CMD_EVENT = 0x0001;
constexpr uint16_t CMD_GET_REGISTER = 0x1000;
constexpr uint16_t CMD_SET_REGISTER = 0x2000;
constexpr uint16_t CMD_CALIBRATE = 0x0002;
constexpr uint16_t CMD_COMMAND_NOT_IMPLEMENTED = 0x0003;
constexpr uint16_t REGISTER_CODE_MASK = 0x0fff;

#pragma pack(push, 1)
struct Frame {
    uint16_t crc;
    uint8_t size;
    uint8_t flags;
    uint64_t deviceIdentifier;
    uint8_t data[FRAME_DATA_SIZE];
};

struct PacketHeader {
    uint16_t crc;
    uint8_t frameSize;
    uint8_t flags;
    uint64_t deviceIdentifier;
    uint8_t serviceSize;
    uint8_t serviceIndex;
    uint16_t serviceCommand;
};
#pragma pack(pop)

struct PacketView {
    uint64_t deviceIdentifier;
    const uint8_t *data;
    uint16_t serviceCommand;
    uint8_t serviceIndex;
    uint8_t dataSize;
    uint8_t flags;

    bool isCommand() const { return (flags & FRAME_FLAG_COMMAND) != 0; }
    bool isReport() const { return !isCommand(); }
    bool isRegisterGet() const { return (serviceCommand & 0xf000) == CMD_GET_REGISTER; }
    bool isEvent() const { return isReport() && (serviceCommand & 0x8000) != 0; }
    uint16_t registerCode() const { return serviceCommand & REGISTER_CODE_MASK; }
    uint16_t eventCode() const { return serviceCommand & 0x00ff; }
    uint8_t eventCounter() const { return static_cast<uint8_t>((serviceCommand >> 8) & 0x7f); }
};

uint16_t crc16(const void *data, size_t size);
size_t frameSize(const Frame &frame);
bool validateFrame(const Frame &frame, size_t receivedSize);
void resetFrame(Frame &frame, uint64_t deviceIdentifier, uint8_t flags);
bool appendPacket(Frame &frame, uint8_t serviceIndex, uint16_t serviceCommand, const void *data = nullptr, uint8_t dataSize = 0);
void finalizeFrame(Frame &frame);
bool packetAt(const Frame &frame, size_t &offset, PacketView &packet);

} // namespace jacdac
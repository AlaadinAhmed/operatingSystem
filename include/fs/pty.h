#pragma once
#include "utils/ring_buffer.h"
#include <cstdint>
constexpr size_t PTY_BUFFER_SIZE = 4096;
struct PtyPair {
    int master_id;
    int slave_id;
    RingBuffer<uint8_t, PTY_BUFFER_SIZE> master_to_slave;
    RingBuffer<uint8_t, PTY_BUFFER_SIZE> slave_to_master;
};

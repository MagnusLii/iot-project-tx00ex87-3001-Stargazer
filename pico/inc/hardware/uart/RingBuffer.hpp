#pragma once

#include <vector>
#include <cstdint>

/**
 * @class RingBuffer
 * @brief Class implementing a ring buffer for UART communication.
 */
class RingBuffer {
public:
    explicit RingBuffer(int size);
    bool empty() const;
    bool full() const;
    bool put(uint8_t data);
    uint8_t get();
private:
    uint32_t head;
    uint32_t tail;
    std::vector<uint8_t> buffer;
};


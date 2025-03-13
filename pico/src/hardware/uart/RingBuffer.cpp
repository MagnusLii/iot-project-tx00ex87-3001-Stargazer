/**
 * @file RingBuffer.cpp
 * @brief Implementation of a ring buffer (circular buffer) for byte storage.
 */

#include "RingBuffer.hpp"

/**
 * @brief Constructs a RingBuffer object with a specified size.
 *
 * @param size The size of the buffer.
 */
RingBuffer::RingBuffer(int size) : head{0}, tail{0}, buffer(size) {}

/**
 * @brief Checks if the ring buffer is empty.
 *
 * @return true if the buffer is empty, false otherwise.
 */
bool RingBuffer::empty() const { return head == tail; }

/**
 * @brief Checks if the ring buffer is full.
 *
 * @return true if the buffer is full, false otherwise.
 */
bool RingBuffer::full() const { return (head + 1) % buffer.size() == tail; }

/**
 * @brief Adds a byte to the ring buffer.
 *
 * @param data The byte to store in the buffer.
 * @return true if the byte was successfully stored, false if the buffer was full.
 */
bool RingBuffer::put(uint8_t data) {
    // calculate new head (position where to store the value)
    uint32_t nh = (head + 1) % buffer.size();
    // return false if buffer would be full
    if (nh == tail) return false;

    buffer[head] = data;
    head = nh;
    return true;
}

/**
 * @brief Retrieves a byte from the ring buffer.
 *
 * @return The byte retrieved from the buffer.
 *         If the buffer is empty, the behavior is undefined.
 */
uint8_t RingBuffer::get() {
    uint8_t value = buffer[tail];
    if (head != tail) { tail = (tail + 1) % buffer.size(); }
    return value;
}

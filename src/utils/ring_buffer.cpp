#include "utils/ring_buffer.h"
template <typename T, size_t Capacity> bool RingBuffer<T, Capacity>::push(const T &item) {
    size_t next_head = (head + 1) % Capacity;
    if (next_head == tail) {
        return false;
    }
    buffer[head] = item;
    head = next_head;
    return true;
}
template <typename T, size_t Capacity> bool RingBuffer<T, Capacity>::pop(T &item) {
    if (head == tail) {
        return false;
    }
    item = buffer[tail];
    tail = (tail + 1) % Capacity;
    return true;
}

template <typename T, size_t Capacity> bool RingBuffer<T, Capacity>::isEmpty() { return head == tail; }
template <typename T, size_t Capacity> bool RingBuffer<T, Capacity>::isFull() {
    return ((head + 1) % Capacity) == tail;
}

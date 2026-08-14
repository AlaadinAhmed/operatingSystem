#pragma once
#include <cstddef>
template <typename T, size_t Capacity> class RingBuffer {
  private:
    T buffer[Capacity];
    size_t head = 0;
    size_t tail = 0;

  public:
    bool push(const T &item);
    bool pop(T &item);
    bool isEmpty();
    bool isFull();
};

#ifndef SCOPED_BUFFER_H
#define SCOPED_BUFFER_H

#include <cstddef>

template <typename T>
class ScopedBuffer {
  public:
    explicit ScopedBuffer(size_t size) 
        : buffer(new T[size]), size(size) {}

    ~ScopedBuffer() { delete[] buffer; }

    T* get_pointer() { return buffer; }
    size_t get_size() const { return size; }

  private:
    T* buffer;
    size_t size;
};

#endif // SCOPED_BUFFER_H

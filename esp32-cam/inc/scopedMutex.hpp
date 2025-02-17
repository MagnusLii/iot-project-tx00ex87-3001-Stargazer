#ifndef SCOPEDMUTEX_HPP
#define SCOPEDMUTEX_HPP

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// Automatically unlocks the semaphore when the object goes out of scope
// Takes a semaphore and locks it in the constructor, unlocks it in the destructor
class ScopedMutex {
  public:
    ScopedMutex(SemaphoreHandle_t &m) : mutex(m) { xSemaphoreTake(mutex, portMAX_DELAY); }
    ~ScopedMutex() { xSemaphoreGive(mutex); }

  private:
    SemaphoreHandle_t &mutex;
};

#endif // SCOPEDMUTEX_HPP
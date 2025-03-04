#ifndef SCOPEDMUTEX_HPP
#define SCOPEDMUTEX_HPP

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "debug.hpp"

// Automatically unlocks the semaphore when the object goes out of scope
// Takes a semaphore and locks it in the constructor, unlocks it in the destructor
class ScopedMutex {
  public:
    ScopedMutex(SemaphoreHandle_t &m) : mutex(m) {
        DEBUG("Taking mutex");
        xSemaphoreTake(mutex, portMAX_DELAY);
    }
    ~ScopedMutex() {
        DEBUG("Giving mutex");
        xSemaphoreGive(mutex);
    }

  private:
    SemaphoreHandle_t &mutex;
};

#endif // SCOPEDMUTEX_HPP
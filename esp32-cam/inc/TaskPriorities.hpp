#ifndef TASKPRIORITIES_HPP
#define TASKPRIORITIES_HPP

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

enum TaskPriorities {
    IDLE = tskIDLE_PRIORITY,
    LOW = tskIDLE_PRIORITY + 1,
    MEDIUM = tskIDLE_PRIORITY + 2,
    HIGH = tskIDLE_PRIORITY + 3,
    ABSOLUTE = tskIDLE_PRIORITY + 4
};

#endif
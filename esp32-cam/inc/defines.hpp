#ifndef BUFFER_SIZE
#define BUFFER_SIZE 512
#endif

#ifndef RETRIES
#define RETRIES 3
#endif

#ifndef RESPONSE_WAIT_TIME
#define RESPONSE_WAIT_TIME 5000
#endif

#ifndef NUM_OF_RETRIES_FOR_PICO_MESSAGE
#define NUM_OF_RETRIES_FOR_PICO_MESSAGE 3
#endif

#ifndef PICO_RESPONSE_WAIT_TIME
#define PICO_RESPONSE_WAIT_TIME 10000
#endif

#ifndef GET_REQUEST_TIMER_PERIOD
#define GET_REQUEST_TIMER_PERIOD 60000
#endif

#ifndef TASK_WATCHDOG_TIMEOUT
#define TASK_WATCHDOG_TIMEOUT 60000 // HTTP requests can take a while if the connection faces issues
#endif

#ifndef RETRY_DELAY_MS
#define RETRY_DELAY_MS 100
#endif

#ifndef ENQUEUE_REQUEST_RETRIES
#define ENQUEUE_REQUEST_RETRIES 3
#endif

#ifndef QUEUE_SIZE
#define QUEUE_SIZE 10
#endif
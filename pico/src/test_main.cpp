#include "pico/stdlib.h" // IWYU pragma: keep

#include "hardware/clock.hpp"

#include <memory>
#include <pico/time.h>
#include <pico/types.h>

#include "debug.hpp"

int main() {
    stdio_init_all();
    sleep_ms(5000);
    DEBUG("Start\r\n");
    auto clock = std::make_shared<Clock>();
    std::string timestamp = "1738686917";
    clock->update(timestamp);

    datetime_t alarm = clock->get_datetime();
    alarm.sec += 15;
    clock->add_alarm(alarm);

    for (;;) {
        if (clock->is_synced()) {
            datetime_t now = clock->get_datetime();
            DEBUG(now.year, unsigned(now.month), unsigned(now.day), unsigned(now.hour), unsigned(now.min),
                  unsigned(now.sec));
            absolute_time_t abs_time = get_absolute_time();
            // Sleep until the alarm rings or 30 seconds have passed
            while (!clock->is_alarm_ringing() && absolute_time_diff_us(abs_time, get_absolute_time()) < 30000000) {
                DEBUG(absolute_time_diff_us(abs_time, get_absolute_time()));
                sleep_ms(1000);
            }
            clock->clear_alarm();
        }
    }

    return 0;
}

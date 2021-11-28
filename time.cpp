#include <stdint.h>
#include <time.h>

uint64_t get_ts_ms()
{
        struct timespec ts { 0, 0 };

        clock_gettime(CLOCK_REALTIME, &ts);

        return uint64_t(ts.tv_sec) * uint64_t(1000) + uint64_t(ts.tv_nsec / 1000000);
}

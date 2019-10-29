//
// Created by ncl on 29/10/19.
//

#include <ctime>
#include "raft_enclave_u.h"

int64_t get_time_milliseconds() {
    timespec tp{};
    clock_gettime(CLOCK_REALTIME, &tp);

    return tp.tv_sec * 1000 + tp.tv_nsec / 1000000;
}

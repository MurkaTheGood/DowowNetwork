#include "Utils.hpp"

#include <unistd.h>
#include <sys/select.h>
#include <sys/timerfd.h>
#include <time.h>

// zero timespan
const timespec zero_time { 0, 0 };

bool DowowNetwork::Utils::SelectWrite(int fd, time_t seconds) {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd, &fds);

    // no timeout - immidiate check
    if (!seconds)
        return pselect(fd + 1, NULL, &fds, NULL, &zero_time, 0) > 0;
    else {
        timespec timeout { 0, seconds };
        return pselect(fd + 1, NULL, &fds, NULL, &timeout, 0) > 0;
    }
}

bool DowowNetwork::Utils::SelectRead(int fd, time_t seconds) {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd, &fds);

    if (!seconds)
        return pselect(fd + 1, &fds, NULL, NULL, &zero_time, 0) > 0;
    else {
        timespec timeout { 0, seconds };
        return pselect(fd + 1, &fds, NULL, NULL, &timeout, 0) > 0;
    }
}

void DowowNetwork::Utils::WriteToEventFd(int fd, uint64_t num) {
    write(fd, &num, sizeof(num));
}

void DowowNetwork::Utils::SetTimerFdTimeout(int fd, time_t seconds) {
    itimerspec new_timer {
        { 0, 0 },               // interval
        { seconds, 0 }          // expiration
    };
    timerfd_settime(fd, 0, &new_timer, 0);
}

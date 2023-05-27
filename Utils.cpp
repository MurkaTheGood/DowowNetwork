#include "Utils.hpp"

#include <unistd.h>
#include <sys/poll.h>
#include <sys/timerfd.h>
#include <time.h>

// zero timespan
const timespec zero_time { 0, 0 };

bool DowowNetwork::Utils::SelectWrite(int fd, int seconds) {
    pollfd pollfds { fd, POLLOUT, 0 };
    poll(&pollfds, 1, seconds * 1000);

    return pollfds.revents & POLLOUT;
}

bool DowowNetwork::Utils::SelectRead(int fd, int seconds) {
    pollfd pollfds { fd, POLLIN, 0 };
    poll(&pollfds, 1, seconds * 1000);

    return pollfds.revents & POLLIN;
}

void DowowNetwork::Utils::WriteEventFd(int fd, uint64_t num) {
    write(fd, &num, sizeof(num));
}

uint64_t DowowNetwork::Utils::ReadEventFd(int fd, int timeout) {
    if (SelectRead(fd, timeout)) {
        uint64_t result;
        read(fd, &result, sizeof(result));
        return result;
    }
    return 0;
}

void DowowNetwork::Utils::SetTimerFdTimeout(int fd, time_t seconds) {
    itimerspec new_timer {
        { 0, 0 },               // interval
        { seconds, 0 }          // expiration
    };
    timerfd_settime(fd, 0, &new_timer, 0);
}


#include "Utils.hpp"

#include <sys/select.h>
#include <time.h>

// zero timespan
const timespec zero_time { 0, 0 };

bool DowowNetwork::Utils::SelectWrite(int fd) {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd, &fds);

    return pselect(fd + 1, NULL, &fds, NULL, &zero_time, 0) > 0;
}

bool DowowNetwork::Utils::SelectRead(int fd) {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd, &fds);

    return pselect(fd + 1, &fds, NULL, NULL, &zero_time, 0) > 0;
}

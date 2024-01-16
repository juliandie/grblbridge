#include <signal.h>
#include <poll.h>

#include <grblbridge.h>

int grbl_pollin(int sd, int timeout) {
    struct pollfd fds = {
        .fd = sd,
        .events = POLLIN,
    };

    return poll(&fds, 1, timeout);
}
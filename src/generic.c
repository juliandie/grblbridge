#include <signal.h>
#include <poll.h>
#include <unistd.h>

#include <grblbridge.h>

int grbl_pollin(int sd, int timeout) {
    struct pollfd fds = {
        .fd = sd,
        .events = POLLIN,
    };

    return poll(&fds, 1, timeout);
}

int grbl_write(int sd, pthread_mutex_t *lock, const char *buf, size_t count) {
    int ret;

    /** Catch EBADF from write */
    if(sd == -1) {
        return 0;
    }

    pthread_mutex_lock(lock);
    ret = write(sd, buf, count);
    pthread_mutex_unlock(lock);
    return ret;
}
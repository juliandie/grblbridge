#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <errno.h>

#include <lib_tty.h>
#include <lib_log.h>

int tty_open(const char *ttyif) {
    int fd;

    fd = open(ttyif, O_RDWR);
    if(fd < 0) {
        return -1;
    }

    return fd;
}

int tty_select(int fd, int timeout) {
    struct timeval tv;
    fd_set fds;
    int ret;

    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    tv.tv_sec = timeout / 1000;
    tv.tv_usec = timeout * 1000;

    ret = select(fd + 1, &fds, NULL, NULL, &tv);
    if(ret < 0) {
        return -1;
    }

    if(ret == 0) {
        errno = ETIMEDOUT;
        return 0;
    }

    return ret;
}

int tty_read(int fd, char *buf, size_t size) {
    const char *p;
    size_t len;
    int ret;

    p = buf;
    len = size;
    while(len) {
        ret = read(fd, buf, len);
        if(ret < 0)
            return ret;

        if(ret == 0)
            goto done;

        p += ret;
        len -= ret;
    }
done:
    return size - len;
}

int tty_write(int fd, const char *buf, size_t size) {
    const char *p;
    size_t len;
    int ret;

    p = buf;
    len = size;
    while(len) {
        ret = write(fd, buf, len);
        if(ret < 0)
            return ret;

        p += ret;
        len -= ret;
    }

    return size - len;
}

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <grblbridge.h>

static void grbl_dev2rem_foward(struct grbl_bridge *grbl) {
    char buf[GRBL_MTU_SIZE];
    int ret;

    for(;;) {
        pthread_testcancel();

        ret = grbl_pollin(grbl->ttyfd, 1000);
        if(ret < 0)
            return;

        if(ret == 0)
            continue;

        ret = read(grbl->ttyfd, buf, sizeof(buf));
        if(ret < 0)
            return;

        if(ret == 0)
            return;

        if(grbl->verbose) {
            pthread_mutex_lock(&grbl->lock);
            printf("D2R{%.4dB} %s\n", ret, buf);
            pthread_mutex_unlock(&grbl->lock);
        }
#if 0
        /**
         * I don't remember, why I was resting für "\r\n".
         * Due to grbl this message isn't complete yet.
         * But this would break the message.
         */
        if(strncmp(&buf[ret - 2], "\r\n", 2))
            continue;
#endif
        ret = grbl_write(grbl->grbl_sd_peer, &grbl->lock, buf, (size_t)ret);
        if(ret < 0) {
            fprintf(stderr, "failed to write (d2r): %s", strerror(errno));
        }

        ret = grbl_write(grbl->mon_sd_peer, &grbl->lock, buf, (size_t)ret);
        if(ret < 0) {
            fprintf(stderr, "failed to write (mon): %s", strerror(errno));
        }
    }
    return;
}

static int grbl_prepare_serial(struct grbl_bridge *grbl) {
    struct termios tty;

    grbl->ttyfd = open(grbl->ttyname, O_RDWR);
    if(grbl->ttyfd < 0) {
        if(grbl->verbose) {
            fprintf(stderr, "failed to open terminal %s\n", grbl->ttyname);
        }
        return -1;
    }

    if(tcgetattr(grbl->ttyfd, &tty) < 0) {
        fprintf(stderr, "failed to get terminal attributes\n");
        return -1;
    }

    tty.c_cflag &= ~PARENB; // Parity even
    tty.c_cflag &= ~CSTOPB; // 1 stop bit
    tty.c_cflag &= ~CSIZE; // clear size bits
    tty.c_cflag |= CS8; // 8 bits per byte
    tty.c_cflag &= ~CRTSCTS; // disable rts/cts
    tty.c_cflag |= CREAD | CLOCAL; // turn on READ & ignore ctrl lines (CLOCAL = 1)
    tty.c_lflag &= ~ICANON; // non-canonical
    tty.c_lflag &= ~ECHO; // disable echo
    tty.c_lflag &= ~ISIG; // disable signals
    tty.c_iflag &= ~(IXON | IXOFF | IXANY); // turn off s/w flow ctrl
    // Disable any special handling of received bytes
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);
    tty.c_oflag &= ~OPOST; // Prevent special interpretation of output bytes (e.g. newline chars)
    tty.c_oflag &= ~ONLCR; // Prevent conversion of newline to carriage return/line feed
    // tty.c_oflag &= ~OXTABS; // Prevent conversion of tabs to spaces (NOT PRESENT IN LINUX)
    // tty.c_oflag &= ~ONOEOT; // Prevent removal of C-d chars (0x004) in output (NOT PRESENT IN LINUX)

    tty.c_cc[VTIME] = 0;    // Wait for up to 1s (10 deciseconds), returning as soon as any data is received.
    tty.c_cc[VMIN] = 0;

    // Set in/out baud rate to be 115200
    cfsetspeed(&tty, B115200);

    if(tcsetattr(grbl->ttyfd, TCSANOW, &tty) < 0) {
        fprintf(stderr, "failed to set terminal attributes\n");
        return -1;
    }

    return 0;
}

void *grbl_serial_thread(void *arg) {
    struct grbl_bridge *grbl = (struct grbl_bridge *)arg;
    int ret;

    pthread_mutex_lock(&grbl->lock);
    grbl->ttyfd = -1;
    pthread_mutex_unlock(&grbl->lock);

    if(grbl == NULL) {
        pthread_exit(NULL);
    }

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);

    for(;;) {
        pthread_testcancel();

        if(grbl->ttyfd < 0) {
            pthread_mutex_lock(&grbl->lock);
            ret = grbl_prepare_serial(grbl);
            pthread_mutex_unlock(&grbl->lock);
            if(ret < 0) {
                sleep(1);
                continue;
            }
        }

        grbl_dev2rem_foward(grbl);
        pthread_mutex_lock(&grbl->lock);
        close(grbl->ttyfd);
        grbl->ttyfd = -1;
        pthread_mutex_unlock(&grbl->lock);
    }

    pthread_exit(NULL);
}
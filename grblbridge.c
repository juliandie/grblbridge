#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <termios.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <poll.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netdb.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netpacket/packet.h>
#include <linux/types.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <errno.h>

#define MTU_SIZE (1500u)

#define GRBL_PORT "23" // default grbl port (telnet)

/** make any variable volatile that is access by different threads */
struct grbl_bridge {
    /** generic */
    int verbose;
    pthread_mutex_t lock;
    pthread_t l2r; /**< laser to remote */
    pthread_t r2l; /**< remote to laser */
    pthread_t a2m; /**< any to monitor */

    /** serial */
    volatile int ttyfd;
    char *ttyif;

    /** network */
    char *grbl_port;
    int grbl_srv; /**< server sock descriptor */
    volatile int grbl_cli; /**< remote sock descriptor */

    /** monitor */
    char *mon_port;
    int mon_srv;
    volatile int mon_cli;
};

/**
 * Disable stdin echo and icanon
 * echo shall be disabled, to avoid cluttering
 * icanon is disabled, to detect single key-strokes
 */
static void stdin_echo(int enable) {
    struct termios tty;
    tcgetattr(STDIN_FILENO, &tty);
    if(enable) {
        tty.c_lflag |= (ICANON | ECHO);
    }
    else {
        tty.c_lflag &= ~(ICANON | ECHO);
    }
    tcsetattr(STDIN_FILENO, TCSANOW, &tty);
}

static int grbl_pollin(int sd, int timeout) {
    struct pollfd fds = {
        .fd = sd,
        .events = POLLIN,
    };

    return poll(&fds, 1, timeout);
}

static void grbl_monitor(struct grbl_bridge *grbl,
                         const char *buf, size_t count) {
    if(grbl->mon_cli != -1) {
        if(send(grbl->mon_cli, buf, count, 0) < 0) {
            fprintf(stderr, "write: %s\n", strerror(errno));
        }
    }
    return;
}

/** read and discard any incoming messages */
static void grbl_a2m_read(struct grbl_bridge *grbl) {
    char buf[MTU_SIZE];
    int ret;

    for(;;) {
        pthread_testcancel();

        ret = grbl_pollin(grbl->mon_cli, 100);
        if(ret < 0) // err (maybe closed/detached)
            return;

        if(ret == 0) // timeout, no data
            continue;

        ret = read(grbl->mon_cli, buf, sizeof(buf));
        if(ret < 0) // err
            return;

        if(ret == 0) // closed by remote
            return;

        printf("Message discarded (%d): %s\n", ret, buf);
    }
    return;
}

static void *grbl_a2m_handle(void *arg) {
    struct grbl_bridge *grbl = (struct grbl_bridge *)arg;
    struct sockaddr_in peer;
    socklen_t addrlen;
    int ret;

    if(grbl == NULL) {
        pthread_exit(NULL);
    }

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);

    ret = listen(grbl->mon_srv, 1); // accept only 1 conection at once
    if(ret < 0) {
        fprintf(stderr, "listen: %s\n", strerror(errno));
        pthread_exit(NULL);
    }

    printf("Monitoring... 0.0.0.0:%s\n", grbl->mon_port);
    for(;;) {
        pthread_testcancel();

        ret = grbl_pollin(grbl->mon_srv, 1000);
        if(ret < 0) {
            fprintf(stderr, "poll: %s\n", strerror(errno));
            pthread_exit(NULL);
        }

        if(ret == 0)
            continue;

        pthread_mutex_lock(&grbl->lock);
        addrlen = sizeof(struct sockaddr);
        grbl->mon_cli = accept(grbl->mon_srv,
                               (struct sockaddr *)&peer, &addrlen);
        pthread_mutex_unlock(&grbl->lock);

        if(grbl->mon_cli < 0) {
            fprintf(stderr, "accept: %s\n", strerror(errno));
            pthread_exit(NULL);
        }

        printf("Monitor connected: %s:%d\n",
               inet_ntoa(peer.sin_addr), htons(peer.sin_port));
        grbl_a2m_read(grbl);

        pthread_mutex_lock(&grbl->lock);
        close(grbl->mon_cli);
        grbl->mon_cli = -1;
        pthread_mutex_unlock(&grbl->lock);
    }

    fprintf(stderr, "finish r2l\n");
    pthread_exit(NULL);
}

static void grbl_r2l_forward(struct grbl_bridge *grbl) {
    char buf[MTU_SIZE];
    int ret;

    for(;;) {
        pthread_testcancel();

        ret = grbl_pollin(grbl->grbl_cli, 100);
        if(ret < 0) // err (maybe closed/detached)
            return;

        if(ret == 0) // timeout, no data
            continue;

        ret = read(grbl->grbl_cli, buf, sizeof(buf));
        if(ret < 0) // err
            return;

        if(ret == 0) // closed by remote
            return;

        if(grbl->verbose) {
            pthread_mutex_lock(&grbl->lock);
            printf("R2L{%.4dB} %s\n", ret, buf);
            pthread_mutex_unlock(&grbl->lock);
        }

        if(grbl->ttyfd == -1)
            continue; // discard message

        pthread_mutex_lock(&grbl->lock);
        ret = write(grbl->ttyfd, buf, ret);
        if(ret < 0) {
            //fprintf(stderr, "write (tty): %s\n", strerror(errno));
        }
        grbl_monitor(grbl, buf, ret);
        pthread_mutex_unlock(&grbl->lock);
    }
    return;
}

static void *grbl_r2l_handle(void *arg) {
    struct grbl_bridge *grbl = (struct grbl_bridge *)arg;
    struct sockaddr_in peer;
    socklen_t addrlen;
    int ret;

    if(grbl == NULL) {
        pthread_exit(NULL);
    }

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);

    ret = listen(grbl->grbl_srv, 1); // accept only 1 conection at once
    if(ret < 0) {
        fprintf(stderr, "listen: %s\n", strerror(errno));
        pthread_exit(NULL);
    }

    printf("Listening... 0.0.0.0:%s\n", grbl->grbl_port);
    for(;;) {
        pthread_testcancel();

        ret = grbl_pollin(grbl->grbl_srv, 1000);
        if(ret < 0) {
            fprintf(stderr, "poll: %s\n", strerror(errno));
            pthread_exit(NULL);
        }

        if(ret == 0)
            continue;

        pthread_mutex_lock(&grbl->lock);
        addrlen = sizeof(struct sockaddr);
        grbl->grbl_cli = accept(grbl->grbl_srv,
                                (struct sockaddr *)&peer, &addrlen);
        pthread_mutex_unlock(&grbl->lock);

        if(grbl->grbl_cli < 0) {
            fprintf(stderr, "accept: %s\n", strerror(errno));
            pthread_exit(NULL);
        }

        grbl_r2l_forward(grbl);

        pthread_mutex_lock(&grbl->lock);
        close(grbl->grbl_cli);
        grbl->grbl_cli = -1;
        pthread_mutex_unlock(&grbl->lock);
    }

    fprintf(stderr, "finish r2l\n");
    pthread_exit(NULL);
}

static int grbl_prepare_tty(struct grbl_bridge *grbl) {
    struct termios tty;

    grbl->ttyfd = open(grbl->ttyif, O_RDWR);
    if(grbl->ttyfd < 0) {
        fprintf(stderr, "failed to open terminal\n");
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

static void grbl_l2r_foward(struct grbl_bridge *grbl) {
    char buf[MTU_SIZE];
    int ret;

    for(;;) {
        pthread_testcancel();

        ret = grbl_pollin(grbl->ttyfd, 100);
        if(ret < 0)
            return;

        if(ret == 0)
            continue;

        ret = read(grbl->ttyfd, buf, sizeof(buf));
        if(ret < 0)
            return;

        if(ret == 0) // something went very wrong
            return;

        if(grbl->verbose) {
            pthread_mutex_lock(&grbl->lock);
            printf("L2R{%.4dB} %s\n", ret, buf);
            pthread_mutex_unlock(&grbl->lock);
        }

        if(strncmp(&buf[ret - 2], "\r\n", 2)) // message not complete
            continue;

        pthread_mutex_lock(&grbl->lock);
        ret = write(grbl->grbl_cli, buf, ret);
        if(ret < 0) {
            fprintf(stderr, "write (tcp): %s", strerror(errno));
        }
        grbl_monitor(grbl, buf, ret);
        pthread_mutex_unlock(&grbl->lock);
    }
    return;
}

static void *grbl_l2r_handle(void *arg) {
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

        /**
         * wait for laser/serial connection
         * laser might in standby or disconnected
         */
        if(grbl->ttyfd < 0) {
            pthread_mutex_lock(&grbl->lock);
            ret = grbl_prepare_tty(grbl);
            pthread_mutex_unlock(&grbl->lock);
            if(ret < 0) {
                sleep(1);
                continue;
            }
        }

        grbl_l2r_foward(grbl);

        pthread_mutex_lock(&grbl->lock);
        close(grbl->ttyfd);
        grbl->ttyfd = -1;
        pthread_mutex_unlock(&grbl->lock);
    }

    fprintf(stderr, "finish l2r\n");
    pthread_exit(NULL);
}

static int grbl_inject(struct grbl_bridge *grbl) {
    char buf[32], c;
    int ret = 0;

    memset(buf, 0, sizeof(buf));
    printf("Inject to [r]emote, [l]aser or got [b]ack\n");
    c = (char)getchar();

    switch(c) {
    case 'r':
    case 'l':
        printf("Enter your code and confirm with [enter]: ");
        stdin_echo(1);
        fgets(buf, sizeof(buf), stdin);
        stdin_echo(0);
        if(c == 'r') {
            pthread_mutex_lock(&grbl->lock);
            ret = write(grbl->grbl_cli, buf, strlen(buf));
            if(ret < 0)
                fprintf(stderr, "write (tcp): %s", strerror(errno));
            pthread_mutex_unlock(&grbl->lock);
        }
        else {
            pthread_mutex_lock(&grbl->lock);
            /** write to ttyfd might fail, since there might be no laser */
            ret = write(grbl->ttyfd, buf, strlen(buf));
            if(ret < 0)
                fprintf(stderr, "write (tty): %s", strerror(errno));
            pthread_mutex_unlock(&grbl->lock);
        }
        break;
    case 'b': printf("falling back to main menu\n"); break;
    default: printf("unknown key, falling back to main menu\n"); break;
    }

    return ret;
}

static void print_help(const char *name) {
    printf("usage: %s [-hv] [-p port] <serial-port>\n"
           "default port: telnet (23)\n",
           name);
    return;
}

static int grbl_prepare_tcp(const char *port) {
    struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
        .ai_flags = AI_PASSIVE,
        .ai_protocol = 0,
    };
    struct addrinfo *res, *rp;
    int ret = 0, fd = 0;

    ret = getaddrinfo(NULL, port, &hints, &res);
    if(ret != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret));
        return -1;
    }

    for(rp = res; rp != NULL; rp = rp->ai_next) {
        fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if(fd == -1)
            continue;

        if(bind(fd, rp->ai_addr, rp->ai_addrlen) == 0)
            break; /* Success */

        close(fd);
        fd = -1;
    }

    freeaddrinfo(res);

    return fd;
}

static int grbl_prepare_thread(struct grbl_bridge *grbl) {
    if(pthread_mutex_init(&grbl->lock, NULL) < 0) {
        fprintf(stderr, "failed to init mutex\n");
        return -1;
    }

    if(grbl->mon_port) {
        if(pthread_create(&grbl->a2m, NULL,
           &grbl_a2m_handle, (void *)grbl) < 0) {
            fprintf(stderr, "failed to create thread a2m\n");
            return -1;
        }
    }

    if(pthread_create(&grbl->r2l, NULL,
       &grbl_r2l_handle, (void *)grbl) < 0) {
        fprintf(stderr, "failed to create thread r2l\n");
        return -1;
    }

    if(pthread_create(&grbl->l2r, NULL,
       &grbl_l2r_handle, (void *)grbl) < 0) {
        fprintf(stderr, "failed to create thread l2r\n");
        return -1;
    }

    return 0;
}

int main(int argc, char **argv) {
    struct grbl_bridge grbl;
    int ret = 0;

    memset(&grbl, 0, sizeof(struct grbl_bridge));
    grbl.grbl_port = GRBL_PORT;
    grbl.ttyfd = -1;
    grbl.grbl_cli = -1;
    grbl.mon_cli = -1;

    for(;;) {
        int c = getopt(argc, argv, "hvp:m:");

        if(c == -1)
            break;

        switch(c) {
        case 'v': grbl.verbose = 1; break;
        case 'p': grbl.grbl_port = optarg; break;
        case 'm': grbl.mon_port = optarg; break;
        case 'h':  print_help(argv[0]); return -1;
        default: break;
        }
    }

    if(optind >= argc) {
        print_help(argv[0]);
        return -1;
    }

    grbl.ttyif = strdup(argv[optind]);
    /**
     * We don't prepare tty yet, since the laser might be off at the moment.
     * The tty is opened and maintained in the l2r thread.
     */

    grbl.grbl_srv = grbl_prepare_tcp(grbl.grbl_port);
    if(ret < 0) {
        goto err_free;
    }

    if(grbl.mon_port) {
        grbl.mon_srv = grbl_prepare_tcp(grbl.mon_port);
        if(ret < 0) {
            goto err_unprepare_grbl;
        }
    }

    ret = grbl_prepare_thread(&grbl);
    if(ret < 0) {
        goto err_unprepare_mon;
    }
    stdin_echo(0);

    for(;;) {
        int c = getchar();

        switch(c) {
        case 'i':
            grbl_inject(&grbl);
            break;
        case 'v':
            grbl.verbose = 1 - grbl.verbose;
            printf("verbose %s\n", (grbl.verbose) ? "enabled" : "disabled");
            break;
        case 'x':
            goto done;
        }

        usleep(1000);
    }

done:
    printf("exit...\n");
    /** Enable stdin buffer and echo */
    stdin_echo(1);

    pthread_cancel(grbl.l2r);
    pthread_join(grbl.l2r, NULL);
    pthread_cancel(grbl.r2l);
    pthread_join(grbl.r2l, NULL);
    pthread_cancel(grbl.a2m);
    pthread_join(grbl.a2m, NULL);
    pthread_mutex_destroy(&grbl.lock);
    if(grbl.ttyfd != -1) {
        close(grbl.ttyfd);
    }
err_unprepare_mon:
    if(grbl.mon_cli != -1) {
        close(grbl.mon_cli);
    }
err_unprepare_grbl:
    if(grbl.grbl_cli != -1) {
        close(grbl.grbl_cli);
    }
    close(grbl.grbl_srv);
err_free:
    free(grbl.ttyif);
    return ret;
}

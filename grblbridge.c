#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <termios.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>

#include <tcp_sock.h>
#include <lib_tty.h>
#include <lib_log.h>

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / (sizeof(a[0])))
#endif

#define BUFFER_SIZE 0x1000

#define SERVERPORT (23) // grbl port (telnet)

struct grbl_bridge_s {
    /** generic */
    int verbose;
    pthread_mutex_t lock;

    /** serial */
    int ttyfd;
    char *ttyif;

    /** network */
    struct tcp_sock_s srv;
    struct tcp_sock_s cli;

};

static int run = 1;

static void stdin_echo(int enable) {
    struct termios tty;
    tcgetattr(STDIN_FILENO, &tty);
    if(enable)
        tty.c_lflag |= (ICANON | ECHO);
    else
        tty.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &tty);
}

static void sig_reset(int signum) {
    signal(signum, SIG_DFL);
}

static int sig_register(int signum, void (*handler)(int)) {
    struct sigaction act = {
        .sa_flags = SA_RESTART,
        .sa_handler = handler,
    };
    sigemptyset(&act.sa_mask);

    return sigaction(signum, &act, NULL);
}

static void sig_handler(int signum) {
    run = 0;
    sig_reset(signum);
}

static int grbl_open_tty(const char *ttyif) {
    struct termios tty;
    int fd;

    fd = open(ttyif, O_RDWR);
    if(fd < 0) {
        LIB_LOG_ERR("failed to open serial port");
        return -1;
    }

    if(tcgetattr(fd, &tty) < 0) {
        LIB_LOG_ERR("failed to get terminal attributes");
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

    if(tcsetattr(fd, TCSANOW, &tty) < 0) {
        LIB_LOG_ERR("failed to set terminal attributes");
        return -1;
    }

    return fd;
}

static void *r2l(void *arg) {
    struct grbl_bridge_s *grbl = (struct grbl_bridge_s *)arg;
    char buf[BUFFER_SIZE];
    int ret;

    if(grbl == NULL)
        run = 0;

    LIB_LOG_INFO("Listening to... 0.0.0.0:%u", SERVERPORT);
    while(run) {
        ret = tcp_select(&grbl->srv, 1000);
        if(ret < 0) {
            run = 0;
            continue;
        }

        if(ret == 0)
            continue;

        pthread_mutex_lock(&grbl->lock);
        ret = tcp_accept(&grbl->srv, &grbl->cli);
        if(ret < 0) {
            run = 0;
            continue;
        }
        pthread_mutex_unlock(&grbl->lock);

        while(run) {
            ret = tcp_select(&grbl->cli, 1000);
            if(ret < 0)
                goto close;

            if(ret == 0)
                continue;

            ret = tcp_recv(&grbl->cli, buf, sizeof(buf));
            if(ret < 0)
                goto close;

            if(ret == 0)
                goto close;

            if(grbl->verbose) {
                pthread_mutex_lock(&grbl->lock);
                lib_hexdump(buf, ret, "R2L");
                pthread_mutex_unlock(&grbl->lock);
            }

            pthread_mutex_lock(&grbl->lock);
            tty_write(grbl->ttyfd, buf, ret);
            pthread_mutex_unlock(&grbl->lock);
        }
close:
        pthread_mutex_lock(&grbl->lock);
        tcp_close(&grbl->cli);
        pthread_mutex_unlock(&grbl->lock);
    }

    pthread_exit(NULL);
}

static void *l2r(void *arg) {
    struct grbl_bridge_s *grbl = (struct grbl_bridge_s *)arg;
    char buf[BUFFER_SIZE];
    int ret;

    grbl->ttyfd = grbl_open_tty(grbl->ttyif);

    if(grbl == NULL)
        run = 0;

    while(run) {
        // wait for laser/serial connection
        // laser might in standby or disconnected
        if(grbl->ttyfd < 0) {
            pthread_mutex_lock(&grbl->lock);
            grbl->ttyfd = grbl_open_tty(grbl->ttyif);
            pthread_mutex_unlock(&grbl->lock);
            if(grbl->ttyfd < 0) {
                sleep(1);
                continue;
            }
        }

        while(run) {
            ret = tty_select(grbl->ttyfd, 1000);
            if(ret < 0)
                goto close;

            if(ret == 0)
                continue;

            ret = tty_read(grbl->ttyfd, buf, sizeof(buf));
            if(ret < 0)
                goto close;

            if(grbl->verbose) {
                pthread_mutex_lock(&grbl->lock);
                lib_hexdump(buf, ret, "L2R");
                pthread_mutex_unlock(&grbl->lock);
            }

            if(!strncmp(&buf[ret - 2], "\r\n", 2)) {
                pthread_mutex_lock(&grbl->lock);
                tcp_send(&grbl->cli, buf, ret);
                pthread_mutex_unlock(&grbl->lock);
            }
        }
close:
        pthread_mutex_lock(&grbl->lock);
        close(grbl->ttyfd);
        grbl->ttyfd = -1;
        pthread_mutex_unlock(&grbl->lock);
    }

    pthread_exit(NULL);
}

static int grbl_inject(struct grbl_bridge_s *grbl) {
    char buf[32];
    int c;

    memset(buf, 0, sizeof(buf));
    printf("Inject to [r]emote, [l]aser or got [b]ack\n");
    c = getchar();

    switch(c) {
    case 'r':
    case 'l':
        printf("Enter your code and confirm with [enter]: ");
        stdin_echo(1);
        fgets(buf, sizeof(buf), stdin);
        stdin_echo(0);
        if(c == 'r') {
            lib_hexdump(buf, sizeof(buf), "u2R");
            pthread_mutex_lock(&grbl->lock);
            tcp_send(&grbl->cli, buf, strlen(buf));
            pthread_mutex_unlock(&grbl->lock);
        }
        else {
            lib_hexdump(buf, sizeof(buf), "u2L");
            pthread_mutex_lock(&grbl->lock);
            tty_write(grbl->ttyfd, buf, strlen(buf));
            pthread_mutex_unlock(&grbl->lock);
        }
        break;
    case 'b': printf("falling back to main menu\n"); break;
    default: printf("unknown key, falling back to main menu\n"); break;
    }

    return 0;
}

static int print_help(const char *name, int ret) {
    printf("usage: %s [-hv] [-p port] <serial-port>\n"
           "default port: telnet (23)\n",
           name);
    return ret;
}

int main(int argc, char **argv) {
    pthread_t r2l_thread, l2r_thread;
    struct grbl_bridge_s grbl;
    uint16_t port = SERVERPORT;
    int ret;

    memset(&grbl, 0, sizeof(struct grbl_bridge_s));

    for(;;) {
        int c = getopt(argc, argv, "hvp:");

        if(c == -1)
            break;

        switch(c) {
        case 'v': grbl.verbose = 1; break;
        case 'p': port = (uint16_t)strtol(optarg, NULL, 0);
        case 'h': return print_help(argv[0], 0);
        default: break;
        }
    }

    if(optind >= argc)
        return print_help(argv[0], -1);

    grbl.ttyif = strdup(argv[optind]);

    if(sig_register(SIGTERM, &sig_handler) != 0) {
        LIB_LOG_ERR("failed to register SIGTERM handler");
    }

    ret = pthread_mutex_init(&grbl.lock, NULL);
    if(ret < 0) {
        LIB_LOG_ERR("failed to init mutex");
        goto err;
    }

    if(tcp_open(&grbl.srv, port)) {
        LIB_LOG_ERR("failed to open tcp socket");
        goto err_mutex_destroy;
    }

    if(pthread_create(&r2l_thread, NULL, &r2l, (void *)&grbl) < 0)
        goto err_close;

    if(pthread_create(&l2r_thread, NULL, &l2r, (void *)&grbl) < 0)
        goto err_join;

    // Disable tty buffer and echo
    stdin_echo(0);

    while(run) {
        int c = getchar();
        switch(c) {
        case 'i': grbl_inject(&grbl); break;
        case 'v': grbl.verbose = 1 - grbl.verbose; break;
        case 'x': run = 0; break;
        }
        usleep(1000);
    }

    // Enable tty buffer and echo
    stdin_echo(1);

    pthread_join(l2r_thread, NULL);
err_join:
    if(run)
        run = 0;
    pthread_join(r2l_thread, NULL);
err_close:
    tcp_close(&grbl.srv);
err_mutex_destroy:
    pthread_mutex_destroy(&grbl.lock);
err:
    free(grbl.ttyif);
    return ret;
}

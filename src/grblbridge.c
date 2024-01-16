#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <netdb.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netpacket/packet.h>
#include <linux/types.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>

#include <grblbridge.h>

#define GRBL_PORT "23" // default grbl port (telnet)

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

static int grbl_inject(struct grbl_bridge *grbl) {
    char buf[32], c;
    int ret = 0;

    memset(buf, 0, sizeof(buf));
    printf("Inject to [r]emote, [d]evice or got [b]ack\n");
    c = (char)getchar();

    switch(c) {
    case 'r':
        printf("Enter your message for remote and confirm with [enter]:\n");
        stdin_echo(1);
        if(fgets(buf, sizeof(buf), stdin) == NULL) {
            return -1;
        }
        stdin_echo(0);

        ret = grbl_write(grbl->grbl_sd_peer, &grbl->lock, buf, strlen(buf));
        if(ret < 0) {
            fprintf(stderr, "failed to write (d2r): %s", strerror(errno));
            return -1;
        }

        ret = grbl_write(grbl->mon_sd_peer, &grbl->lock, buf, strlen(buf));
        if(ret < 0) {
            fprintf(stderr, "failed to write (mon): %s", strerror(errno));
            return -1;
        }
        break;
    case 'd':
        printf("Enter your message for device and confirm with [enter]:\n");
        stdin_echo(1);
        if(fgets(buf, sizeof(buf), stdin) == NULL) {
            return -1;
        }
        stdin_echo(0);

        ret = grbl_write(grbl->ttyfd, &grbl->lock, buf, strlen(buf));
        if(ret < 0) {
            fprintf(stderr, "failed to write (d2r): %s", strerror(errno));
            return -1;
        }

        ret = grbl_write(grbl->mon_sd_peer, &grbl->lock, buf, strlen(buf));
        if(ret < 0) {
            fprintf(stderr, "failed to write (mon): %s", strerror(errno));
            return -1;
        }
        break;
    case 'b':
        printf("falling back to main menu\n");
        break;
    default:
        printf("unknown key, falling back to main menu\n");
        break;
    }

    return ret;
}

static int grbl_prepare_tcp(const char *port) {
    struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
        .ai_flags = AI_PASSIVE,
        .ai_protocol = 0,
    };
    struct addrinfo *res;
    int ret = 0, fd = 0;

    if(!port) {
        return -1;
    }

    ret = getaddrinfo(NULL, port, &hints, &res);
    if(ret != 0) {
        fprintf(stderr, "getaddrinfo failed: %s\n", gai_strerror(ret));
        return -1;
    }

    for(struct addrinfo *rp = res; rp != NULL; rp = rp->ai_next) {
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

    if(pthread_create(&grbl->serial, NULL,
       &grbl_serial_thread, (void *)grbl) < 0) {
        fprintf(stderr, "failed to create serial thread\n");
        return -1;
    }

    if(pthread_create(&grbl->tcp, NULL,
       &grbl_tcp_thread, (void *)grbl) < 0) {
        fprintf(stderr, "failed to create tcp thread\n");
        return -1;
    }

    if(pthread_create(&grbl->monitor, NULL,
       &grbl_mon_thread, (void *)grbl) < 0) {
        fprintf(stderr, "failed to create monitor thread\n");
        return -1;
    }

    return 0;
}

static void print_help(const char *name) {
    printf("usage: %s [-hv] [-p port] -d <serial-port>\n"
           "default port: telnet (23)\n",
           name);
    return;
}

static void grbl_init(struct grbl_bridge *grbl) {
    memset(grbl, 0, sizeof(struct grbl_bridge));
    grbl->ttyfd = -1;
    grbl->grbl_sd = -1;
    grbl->grbl_sd_peer = -1;
    grbl->mon_sd = -1;
    grbl->mon_sd_peer = -1;
}

int main(int argc, char **argv) {
    struct grbl_bridge grbl;
    int ret = 0;

    grbl_init(&grbl);
    for(;;) {
        int c = getopt(argc, argv, "hvp:m:d:");

        if(c == -1)
            break;

        switch(c) {
        case 'v': grbl.verbose = 1; break;
        case 'd': grbl.ttyname = strdup(optarg); break;
        case 'p': grbl.grbl_port_str = optarg; break;
        case 'm': grbl.mon_port_str = optarg; break;
        case 'h':  print_help(argv[0]); return -1;
        default: break;
        }
    }

    /**
     * Only test, if ttyname is given, but the device might be off for now.
     * Therefore the device-node might not be available yet.
     * The tty is opened and maintained in the device2remote thread.
     * In case the user passed in a wrong device-node for ttyname,
     * it's posible that ttyname won't be opened at all.
     */
    if(!grbl.ttyname) {
        print_help(argv[0]);
        return -1;
    }

    if(grbl.grbl_port_str != NULL) {
        grbl.grbl_sd = grbl_prepare_tcp(grbl.grbl_port_str);
    }
    else {
        grbl.grbl_sd = grbl_prepare_tcp(GRBL_PORT);
    }

    if(grbl.grbl_sd < 0) {
        goto err_free;
    }

    grbl.mon_sd = grbl_prepare_tcp(grbl.mon_port_str);
    if(grbl.mon_sd < 0 && grbl.mon_port_str == NULL) {
        goto err_unprepare_grbl;
    }

    /** TODO drop root-priviledges if required */

    ret = grbl_prepare_thread(&grbl);
    if(ret < 0) {
        goto err_unprepare_mon;
    }

    stdin_echo(0);
    for(;;) {
        int c = getchar();

        switch(c) {
        case 'h':
            printf("v    Change verbosity\n"
                   "i    Inject command\n"
                   "q    Quit\n"
                   "x    Exit\n");
            break;
        case 'i':
            grbl_inject(&grbl);
            break;
        case 'v':
            grbl.verbose = 1 - grbl.verbose;
            printf("verbose %s\n", (grbl.verbose) ? "enabled" : "disabled");
            break;
        case 'q':
            goto done;
            break;
        case 'x':
            goto done;
            break;
        }

        usleep(1000);
    }

done:
    printf("exit...\n");
    /** Enable stdin buffer and echo */
    stdin_echo(1);

    pthread_cancel(grbl.serial);
    pthread_join(grbl.serial, NULL);
    pthread_cancel(grbl.tcp);
    pthread_join(grbl.tcp, NULL);
    pthread_cancel(grbl.monitor);
    pthread_join(grbl.monitor, NULL);
    pthread_mutex_destroy(&grbl.lock);
    if(grbl.ttyfd != -1) {
        close(grbl.ttyfd);
    }
err_unprepare_mon:
    if(grbl.mon_sd_peer != -1) {
        close(grbl.mon_sd_peer);
    }
    if(grbl.mon_sd != -1) {
        close(grbl.mon_sd);
    }
err_unprepare_grbl:
    if(grbl.grbl_sd_peer != -1) {
        close(grbl.grbl_sd_peer);
    }
    if(grbl.grbl_sd != -1) {
        close(grbl.grbl_sd);
    }
err_free:
    free(grbl.ttyname);
    return ret;
}

#include <tcp_sock.h>

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

#include <lib_log.h>
//#include <lib_buffer.h>

static int tcp_set_blocking(int sd, int enable) {
    int flags;

    if(sd < 0)
        return -1;

    flags = fcntl(sd, F_GETFL, 0);
    if(flags == -1)
        return -1;

    flags = enable ? (flags & ~O_NONBLOCK) : (flags | O_NONBLOCK);
    return fcntl(sd, F_SETFL, flags);
}

static int srv_setsockopt(int fd, int lvl, int name, int val, size_t len) {
    return setsockopt(fd, lvl, name, &val, len);
}

int tcp_open(struct tcp_sock_s *tcp, uint16_t port) {
    struct sockaddr_in sa;
    int ret;
    socklen_t addrlen = sizeof(struct sockaddr_in);

    memset(tcp, 0, sizeof(struct tcp_sock_s));

    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);
    sa.sin_addr.s_addr = INADDR_ANY;

    tcp->port = port;
    tcp->sd = socket(AF_INET, SOCK_STREAM, 0);
    if(tcp->sd < 0) {
        LIB_LOG_ERR("socket failed on (%s:%d)",
                    inet_ntoa(sa.sin_addr), port);
        return -1;
    }

    if(tcp_set_blocking(tcp->sd, 0) < 0)
        LIB_LOG_ERR("disable blocking failed");

    if(srv_setsockopt(tcp->sd, SOL_SOCKET, SO_REUSEADDR, 1, sizeof(int)) < 0)
        LIB_LOG_ERR("disable blocking failed");

    ret = bind(tcp->sd, (struct sockaddr *)&sa, sizeof(struct sockaddr));
    if(ret < 0) {
        LIB_LOG_ERR("bind failed on %s:%d", inet_ntoa(sa.sin_addr), port);
        goto err;
    }

    if(!port) { // port == 0 auto-assigns the next available port
        ret = getsockname(tcp->sd, (struct sockaddr *)&sa, &addrlen);
        if(ret < 0) {
            LIB_LOG_ERR("getsockname failed on (%s:%d)",
                        inet_ntoa(sa.sin_addr), port);
            goto err;
        }
        tcp->port = ntohs(sa.sin_port);
        LIB_LOG_INFO("bound random port %d", tcp->port);
    }

    listen(tcp->sd, 3);

    return 0;

err:
    tcp_close(tcp);
    return -1;
}

int tcp_accept(struct tcp_sock_s *srv, struct tcp_sock_s *cli) {
    struct sockaddr_in6 ip6;
    struct sockaddr_in *ip4 = (struct sockaddr_in *)&ip6;
    struct sockaddr *addr = (struct sockaddr *)&ip6;
    char ipstr[32];
    socklen_t addrlen;

    addrlen = sizeof(struct sockaddr_in6);

    cli->sd = accept(srv->sd, addr, &addrlen);
    if(cli->sd < 0) {
        LIB_LOG_ERR("accpet failed");
        return -1;
    }

    if(tcp_set_blocking(cli->sd, 0) < 0)
        LIB_LOG_ERR("disable blocking failed");


    switch(addr->sa_family) {
    case AF_INET:
        cli->port = ntohs(ip4->sin_port);
        if(!inet_ntop(AF_INET, &ip4->sin_addr, ipstr, sizeof(ipstr)))
            LIB_LOG_ERR("inet_ntop failed");

        LIB_LOG_DBG("ip4 %s:%d", ipstr, ntohs(ip4->sin_port));
        break;
    case AF_INET6:
        cli->port = ntohs(ip6.sin6_port);
        if(!inet_ntop(AF_INET6, &ip6.sin6_addr, ipstr, sizeof(ipstr)))
            LIB_LOG_ERR("inet_ntop failed");

        LIB_LOG_DBG("ip6 %s:%d (%d, %d)",
                    ipstr, ntohs(ip6.sin6_port),
                    ip6.sin6_flowinfo,
                    ip6.sin6_scope_id);
        break;
    default: /* Should never get here */
        LIB_LOG_WARN("unknown address family (%d)", addr->sa_family);
        break;
    }
    return 0;
}

int tcp_connect(struct tcp_sock_s *tcp, char *host, uint16_t port) {
    struct hostent *hostent;
    struct sockaddr_in sa;
    int ret;

    memset(tcp, 0, sizeof(struct tcp_sock_s));

    if(isdigit(host[0])) {
        sa.sin_addr.s_addr = inet_addr(host);
    }
    else {
        hostent = gethostbyname(host);
        if(hostent) {
            memcpy((char *)&sa.sin_addr, (char *)hostent->h_addr, sizeof(sa.sin_addr));
        }
        else {
            LIB_LOG_WARN("failed to determine host");
            errno = EINVAL;
            return -1;
        }
    }

    sa.sin_port = htons(port);
    sa.sin_family = AF_INET;
    tcp->sd = socket(AF_INET, SOCK_STREAM, 0);
    if(tcp->sd < 0) {
        LIB_LOG_ERR("failed to open socket");
        return -1;
    }

    ret = connect(tcp->sd, (struct sockaddr *)&sa, sizeof(struct sockaddr_in));
    if(ret < 0) {
        LIB_LOG_ERR("failed to conenct");
        tcp_close(tcp);
        return -1;
    }

    tcp->port = port;

    return 0;
}

void tcp_close(struct tcp_sock_s *tcp) {
    if(tcp->sd >= 0)
        close(tcp->sd);

    memset(tcp, 0, sizeof(struct tcp_sock_s));
    tcp->sd = -1;
}

int tcp_select(struct tcp_sock_s *tcp, int timeout) {
    struct timeval tv;
    fd_set fds;
    int ret;

    FD_ZERO(&fds);
    FD_SET(tcp->sd, &fds);
    /* Timeout. */
    tv.tv_sec = timeout / 1000;
    tv.tv_usec = timeout * 1000;

    ret = select(tcp->sd + 1, &fds, NULL, NULL, &tv);
    if(ret < 0) {
        return -1;
    }

    if(ret == 0) {
        // isn't this set by select?
        errno = ETIMEDOUT;
        return 0;
    }

    return ret;
}

int tcp_poll(struct tcp_sock_s *tcp, int timeout) {
    struct pollfd fds;
    int ret;

    fds.fd = tcp->sd;
    fds.events = (POLLIN | POLLPRI);
    ret = poll(&fds, 1, timeout);

    if(ret < 0) { // error
        return -1;
    }

    if(ret == 0) {
        errno = ETIMEDOUT;
        return 0;
    }

    return ret;
}

int tcp_recv(struct tcp_sock_s *tcp, char *buf, size_t size) {
    int ret;

    if(!tcp || !buf || !size)
        return -EINVAL;

    ret = read(tcp->sd, buf, size);

    if(ret < 0) {
        if(errno == EWOULDBLOCK)
            return 0;
        return -1;
    }

    if(ret == 0) {
        // Connection reset by peer
        // Replace with ECONNRESET
        errno = ENOTCONN;
        return -1;
    }

    return ret;
}

int tcp_send(struct tcp_sock_s *tcp, const char *buf, size_t size) {
    const char *p = 0;
    size_t len = 0;
    int ret;

    if(!tcp || !buf || !size)
        return -EINVAL;

    p = buf;
    len = size;
    do {
        ret = write(tcp->sd, p, size);

        if(ret <= 0) {
            return -1;
        }

        p += ret;
        len -= ret;
    } while(len);

    return size;
}

int tcp_flush(struct tcp_sock_s *tcp) {
    int ret, queue;

    if(!tcp || tcp->sd == -1)
        return 0;

    for(;;) {
        ret = ioctl(tcp->sd, TIOCOUTQ, &queue);
        if(ret != 0) {
            return -1;
        }

        if(!queue) {
            return 0;
        }

        usleep(1);
    }

    return 0;
}

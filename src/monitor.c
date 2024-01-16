#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <grblbridge.h>

void grbl_mon_put(struct grbl_bridge *grbl, const char *buf, size_t count) {
    if(grbl->mon_sd_peer != -1) {
        if(send(grbl->mon_sd_peer, buf, count, 0) < 0) {
            fprintf(stderr, "failed to write (mon): %s\n", strerror(errno));
        }
    }
}

/** read and discard any incoming messages */
static void grbl_mon_read(struct grbl_bridge *grbl) {
    char buf[GRBL_MTU_SIZE];
    int ret;

    for(;;) {
        pthread_testcancel();

        ret = grbl_pollin(grbl->mon_sd_peer, 100);
        if(ret < 0) // err (maybe closed/detached)
            return;

        if(ret == 0) // timeout, no data
            continue;

        ret = read(grbl->mon_sd_peer, buf, sizeof(buf));
        if(ret < 0) // err
            return;

        if(ret == 0) // closed by remote
            return;

        //printf("Message discarded (%d): %s\n", ret, buf);
    }
    return;
}

void *grbl_mon_handle(void *arg) {
    struct grbl_bridge *grbl = (struct grbl_bridge *)arg;
    struct sockaddr_in lo, peer;
    socklen_t addrlen;
    int ret;

    if(grbl == NULL) {
        pthread_exit(NULL);
    }

    if(grbl->mon_sd == -1) {
        pthread_exit(NULL);
    }

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);

    ret = listen(grbl->mon_sd, 3);
    if(ret < 0) {
        fprintf(stderr, "listen: %s\n", strerror(errno));
        pthread_exit(NULL);
    }

    addrlen = sizeof(struct sockaddr);
    ret = getsockname(grbl->mon_sd, (struct sockaddr *)&lo, &addrlen);
    if(ret < 0) {
        fprintf(stderr, "getsockname: %s\n", strerror(errno));
        pthread_exit(NULL);
    }

    printf("Monitoring... 0.0.0.0:%u\n", ntohs(lo.sin_port));
    for(;;) {
        pthread_testcancel();

        ret = grbl_pollin(grbl->mon_sd, 1000);
        if(ret < 0) {
            fprintf(stderr, "poll: %s\n", strerror(errno));
            pthread_exit(NULL);
        }

        if(ret == 0)
            continue;

        pthread_mutex_lock(&grbl->lock);
        addrlen = sizeof(struct sockaddr);
        grbl->mon_sd_peer = accept(grbl->mon_sd,
                               (struct sockaddr *)&peer, &addrlen);
        pthread_mutex_unlock(&grbl->lock);

        if(grbl->mon_sd_peer < 0) {
            fprintf(stderr, "accept: %s\n", strerror(errno));
            pthread_exit(NULL);
        }

        printf("Monitor connected: %s:%d\n",
               inet_ntoa(peer.sin_addr), htons(peer.sin_port));
        grbl_mon_read(grbl);

        pthread_mutex_lock(&grbl->lock);
        close(grbl->mon_sd_peer);
        grbl->mon_sd_peer = -1;
        pthread_mutex_unlock(&grbl->lock);
    }

    fprintf(stderr, "finish r2l\n");
    pthread_exit(NULL);
}
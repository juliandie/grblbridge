#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <grblbridge.h>

static void grbl_rem2dev_forward(struct grbl_bridge *grbl) {
    char buf[GRBL_MTU_SIZE];
    int ret;

    for(;;) {
        pthread_testcancel();

        ret = grbl_pollin(grbl->grbl_sd_peer, 100);
        if(ret < 0) // err (maybe closed/detached)
            return;

        if(ret == 0) // timeout, no data
            continue;

        ret = read(grbl->grbl_sd_peer, buf, sizeof(buf));
        if(ret < 0) // err
            return;

        if(ret == 0) // closed by remote
            return;

        if(grbl->verbose) {
            pthread_mutex_lock(&grbl->lock);
            printf("R2D{%.4dB} %s\n", ret, buf);
            pthread_mutex_unlock(&grbl->lock);
        }

        if(grbl->ttyfd == -1)
            continue; // discard message

        pthread_mutex_lock(&grbl->lock);
        ret = write(grbl->ttyfd, buf, ret);
        if(ret < 0) {
            fprintf(stderr, "failed to write (r2d): %s", strerror(errno));
        }
        grbl_mon_put(grbl, buf, ret);
        pthread_mutex_unlock(&grbl->lock);
    }
    return;
}

void *grbl_rem2dev_handle(void *arg) {
    struct grbl_bridge *grbl = (struct grbl_bridge *)arg;
    struct sockaddr_in lo, peer;
    socklen_t addrlen;
    int ret;

    if(grbl == NULL) {
        pthread_exit(NULL);
    }

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);

    ret = listen(grbl->grbl_sd, 3);
    if(ret < 0) {
        fprintf(stderr, "listen: %s\n", strerror(errno));
        pthread_exit(NULL);
    }

    addrlen = sizeof(struct sockaddr);
    ret = getsockname(grbl->grbl_sd, (struct sockaddr *)&lo, &addrlen);
    if(ret < 0) {
        fprintf(stderr, "getsockname: %s\n", strerror(errno));
        pthread_exit(NULL);
    }

    printf("Listening... 0.0.0.0:%u\n", ntohs(lo.sin_port));
    for(;;) {
        pthread_testcancel();

        ret = grbl_pollin(grbl->grbl_sd, 1000);
        if(ret < 0) {
            fprintf(stderr, "poll: %s\n", strerror(errno));
            pthread_exit(NULL);
        }

        if(ret == 0)
            continue;

        pthread_mutex_lock(&grbl->lock);
        addrlen = sizeof(struct sockaddr);
        grbl->grbl_sd_peer = accept(grbl->grbl_sd,
                                (struct sockaddr *)&peer, &addrlen);
        pthread_mutex_unlock(&grbl->lock);

        if(grbl->grbl_sd_peer < 0) {
            fprintf(stderr, "accept: %s\n", strerror(errno));
            pthread_exit(NULL);
        }

        grbl_rem2dev_forward(grbl);

        pthread_mutex_lock(&grbl->lock);
        close(grbl->grbl_sd_peer);
        grbl->grbl_sd_peer = -1;
        pthread_mutex_unlock(&grbl->lock);
    }

    fprintf(stderr, "finish r2l\n");
    pthread_exit(NULL);
}
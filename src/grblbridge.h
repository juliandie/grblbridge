#ifndef GRBLBRIDGE_H_
#define GRBLBRIDGE_H_

#include <pthread.h>

#define GRBL_MTU_SIZE (1500u)

/** make any variable volatile that is access by different threads */
struct grbl_bridge {
    /** generic */
    int verbose;

    /** thread */
    pthread_mutex_t lock;
    pthread_t dev2remote;
    pthread_t remote2dev;
    pthread_t monitor;

    /** serial */
    char *ttyname;
    volatile int ttyfd;

    /** network */
    char *grbl_port_str;
    int grbl_sd;
    volatile int grbl_sd_peer;

    /** monitor */
    char *mon_port_str;
    int mon_sd;
    volatile int mon_sd_peer;
};

/** generic.c */
int grbl_pollin(int sd, int timeout);

/** dev2remote.c */
void *grbl_dev2rem_handle(void *arg);

/** remote2dev.c */
void *grbl_rem2dev_handle(void *arg);

/** monitor.c */
void grbl_mon_put(struct grbl_bridge *grbl, const char *buf, size_t count);
void *grbl_mon_handle(void *arg);

#endif /* GRBLBRIDGE_H_ */
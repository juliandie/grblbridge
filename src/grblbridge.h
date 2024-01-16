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
    pthread_t serial;
    pthread_t tcp;
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
int grbl_write(int sd, pthread_mutex_t *lock, const char *buf, size_t count);

/** serial.c */
void *grbl_serial_thread(void *arg);

/** tcp.c */
void *grbl_tcp_thread(void *arg);

/** monitor.c */
void *grbl_mon_thread(void *arg);

#endif /* GRBLBRIDGE_H_ */
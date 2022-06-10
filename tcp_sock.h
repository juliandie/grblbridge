
#ifndef TCP_SOCK_H_
#define TCP_SOCK_H_

#include <stdint.h>
#include <arpa/inet.h>
//#include <lib_netif.h>

#if 0
struct sockaddr { // 16B
    sa_family_t	sa_family;	/* address family, AF_xxx	*/
    char		sa_data[14];	/* 14 bytes of protocol address	*/
};

struct sockaddr_in { // 16B
    __kernel_sa_family_t	sin_family;	/* Address family		*/
    __be16		sin_port;	/* Port number			*/
    struct in_addr {
        __be32	s_addr;
    } sin_addr;	/* Internet address		*/

    /* Pad to size of `struct sockaddr'. */
    unsigned char		__pad[__SOCK_SIZE__ - sizeof(short int) -
        sizeof(unsigned short int) - sizeof(struct in_addr)];
};

struct sockaddr_in6 { // 28B
    unsigned short int	sin6_family;    /* AF_INET6 */
    __be16			sin6_port;      /* Transport layer port # */
    __be32			sin6_flowinfo;  /* IPv6 flow information */
    struct in6_addr {
        union {
            __u8		u6_addr8[16];
#if __UAPI_DEF_IN6_ADDR_ALT
            __be16		u6_addr16[8];
            __be32		u6_addr32[4];
#endif
        } in6_u;
#define s6_addr		in6_u.u6_addr8
#if __UAPI_DEF_IN6_ADDR_ALT
#define s6_addr16	in6_u.u6_addr16
#define s6_addr32	in6_u.u6_addr32
#endif
    } sin6_addr;      /* IPv6 address */
    __u32			sin6_scope_id;  /* scope id (new in RFC2553) */
};

#define AF_INET 2
#define AF_INET6 10
#endif

struct tcp_sock_s;

struct tcp_sock_s {
    int sd;

    /** In case the socket was opened with a 0 port
     * the assigned port number can be read back
     */
    uint16_t port;
};

/** Server */
int tcp_open(struct tcp_sock_s *tcp, uint16_t port);
int tcp_accept(struct tcp_sock_s *srv, struct tcp_sock_s *cli);

/** Client */
int tcp_connect(struct tcp_sock_s *tcp, char *host, uint16_t port);

/** Generic */
void tcp_close(struct tcp_sock_s *tcp);

int tcp_select(struct tcp_sock_s *tcp, int timeout); // timeout seconds
int tcp_poll(struct tcp_sock_s *tcp, int timeout); // timeout milliseconds
int tcp_recv(struct tcp_sock_s *tcp, char *buf, size_t size);
int tcp_send(struct tcp_sock_s *tcp, const char *buf, size_t size);
int tcp_flush(struct tcp_sock_s *tcp);

#endif

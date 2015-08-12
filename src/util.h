#ifndef UTIL_H
#define	UTIL_H

#define TIMEVAL_SET(tv, t) \
    do {                                             \
        tv.tv_sec  = (long) t;                       \
        tv.tv_usec = (long) ((t - tv.tv_sec) * 1e6); \
    } while (0)

#define TIMEVAL_TO_DOUBLE(tv) (tv.tv_sec + tv.tv_usec * 1e-6)

static inline char *sock_addr(struct sockaddr *addr) {
    struct sockaddr_in addr_in = *(struct sockaddr_in *) addr;
    char *ip = emalloc(20);
    uv_ip4_name(&addr_in, ip, 20);
    return ip;
}
    
static inline int sock_port(struct sockaddr *addr) {
    struct sockaddr_in addr_in = *(struct sockaddr_in *) addr;
    return ntohs(addr_in.sin_port);
}

#define COPY_C_STR(c_str, str, str_len) \
    memcpy(c_str, str, str_len); \
    c_str[str_len] = '\0'

#define MAKE_C_STR(c_str, str, str_len) \
    c_str = emalloc(str_len + 1); \
    COPY_C_STR(c_str, str, str_len)

#endif	/* UTIL_H */

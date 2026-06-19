#ifndef LWIPOPTS_H
#define LWIPOPTS_H

/* ---- OS integration ---- */
#define NO_SYS                  0
#define LWIP_TCPIP_CORE_LOCKING 1

/* ---- Memory ---- */
#define MEM_ALIGNMENT        4
#define MEM_SIZE             (8 * 1024)
#define MEMP_NUM_PBUF        16
#define MEMP_NUM_TCP_SEG     16
#define MEMP_NUM_SYS_TIMEOUT 8

/* ---- Buffers ---- */
#define PBUF_POOL_SIZE    8
#define PBUF_POOL_BUFSIZE 512

/* ---- Protocols ---- */
#define LWIP_ARP  1
#define LWIP_IPV4 1
#define LWIP_ICMP 1
#define LWIP_UDP  1
#define LWIP_TCP  1
#define LWIP_DNS  0
#define LWIP_DHCP 0

/* ---- Loopback ---- */
#define LWIP_HAVE_LOOPIF        1
#define LWIP_LOOPBACK_MAX_PBUFS 8
#define LWIP_NETIF_LOOPBACK     1

/* ---- Stats / Debug ---- */
#define LWIP_STATS 0
#define LWIP_DEBUG 0

/* ---- Callbacks ---- */
#define LWIP_NETIF_STATUS_CALLBACK 1
#define LWIP_NETIF_LINK_CALLBACK   1

/* ---- Socket / Netconn API ---- */
#define LWIP_NETCONN        1
#define LWIP_SOCKET         1
#define LWIP_COMPAT_SOCKETS 1

/* ---- FreeRTOS sys_arch ---- */
#define LWIP_FREERTOS_THREAD_STACKSIZE_IS_STACKWORDS 1
#define TCPIP_THREAD_NAME                            "tcpip"
#define TCPIP_THREAD_STACKSIZE                       512
#define TCPIP_THREAD_PRIO                            3
#define TCPIP_MBOX_SIZE                              16
#define DEFAULT_UDP_RECVMBOX_SIZE                    8
#define DEFAULT_TCP_RECVMBOX_SIZE                    8
#define DEFAULT_ACCEPTMBOX_SIZE                      8
#define DEFAULT_THREAD_STACKSIZE                     256

#endif /* LWIPOPTS_H */

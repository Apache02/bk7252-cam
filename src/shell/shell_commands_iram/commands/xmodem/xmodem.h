#ifndef XMODEM_H
#define XMODEM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define XMODEM_ERR_TIMEOUT          (-1)
#define XMODEM_ERR_CRC              (-2)
#define XMODEM_ERR_SEQ              (-3)
#define XMODEM_ERR_ABORT            (-4)
#define XMODEM_ERR_OVERFLOW         (-5)
#define XMODEM_ERR_NAK              (-6)

#define XMODEM_SOH                  0x01
#define XMODEM_STX                  0x02
#define XMODEM_EOT                  0x04
#define XMODEM_ACK                  0x06
#define XMODEM_NAK                  0x15
#define XMODEM_CAN                  0x18
#define XMODEM_CCHAR                'C'
#define XMODEM_SUB                  0x1A


#ifndef XMODEM_TIMEOUT_PACKET_MS
#define XMODEM_TIMEOUT_PACKET_MS   3000
#endif

#ifndef XMODEM_TIMEOUT_BYTE_MS
#define XMODEM_TIMEOUT_BYTE_MS     1000
#endif

#ifndef XMODEM_MAX_RETRIES
#define XMODEM_MAX_RETRIES         10
#endif

#ifndef XMODEM_MAX_1K_INIT_TRIES
#define XMODEM_MAX_1K_INIT_TRIES   3
#endif


extern int xmodem_getc(int timeout_ms);

extern void xmodem_putc(uint8_t c);

int32_t xmodem_receive(uint8_t *buf, uint32_t max_len);

#ifdef __cplusplus
}
#endif

#endif // XMODEM_H
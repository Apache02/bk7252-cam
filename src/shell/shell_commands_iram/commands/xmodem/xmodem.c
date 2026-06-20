#include <stdint.h>
#include <string.h>
#include "utils/crc16.h"
#include "xmodem.h"

#define BLOCK_SIZE_SOH 128
#define BLOCK_SIZE_STX 1024

static void xmodem_abort(void) {
    for (int i = 0; i < 3; i++) xmodem_putc(XMODEM_CAN);
    for (int i = 0; i < 3; i++) xmodem_putc(0x08); /* BS */
}

static void xmodem_flush(void) { while (xmodem_getc(50) >= 0); }

static int xmodem_recv_exact(uint8_t *dst, uint32_t len, int timeout_ms) {
    for (uint32_t i = 0; i < len; i++) {
        int c = xmodem_getc(timeout_ms);
        if (c < 0) return XMODEM_ERR_TIMEOUT;
        dst[i] = (uint8_t)c;
    }
    return 0;
}

static int xmodem_recv_block(uint8_t hdr, uint8_t *data_buf, uint8_t *out_blknum) {
    uint32_t bsz = (hdr == XMODEM_STX) ? BLOCK_SIZE_STX : BLOCK_SIZE_SOH;

    int bn = xmodem_getc(XMODEM_TIMEOUT_BYTE_MS);
    int bi = xmodem_getc(XMODEM_TIMEOUT_BYTE_MS);
    if (bn < 0 || bi < 0) return XMODEM_ERR_TIMEOUT;
    if (((uint8_t)bn ^ (uint8_t)bi) != 0xFFu) return XMODEM_ERR_SEQ;
    *out_blknum = (uint8_t)bn;

    if (xmodem_recv_exact(data_buf, bsz, XMODEM_TIMEOUT_BYTE_MS) < 0) return XMODEM_ERR_TIMEOUT;

    int ch = xmodem_getc(XMODEM_TIMEOUT_BYTE_MS);
    int cl = xmodem_getc(XMODEM_TIMEOUT_BYTE_MS);
    if (ch < 0 || cl < 0) return XMODEM_ERR_TIMEOUT;

    uint16_t recv_crc = ((uint16_t)(uint8_t)ch << 8) | (uint8_t)cl;
    if (recv_crc != crc16(data_buf, bsz)) return XMODEM_ERR_CRC;

    return (int)bsz;
}

int32_t xmodem_receive(uint8_t *buf, uint32_t max_len) {
    uint8_t  block_buf[BLOCK_SIZE_STX];
    uint32_t received  = 0;
    uint8_t  expected  = 1;
    int      nak_count = 0;
    int      can_count = 0;

    for (int try = 0;; try++) {
        if (try >= XMODEM_MAX_1K_INIT_TRIES + 10) return XMODEM_ERR_TIMEOUT;

        xmodem_putc(XMODEM_CCHAR);

        int c = xmodem_getc(XMODEM_TIMEOUT_PACKET_MS);

        if (c == XMODEM_SOH || c == XMODEM_STX) {
            uint8_t blk_num;
            int     bsz = xmodem_recv_block((uint8_t)c, block_buf, &blk_num);

            if (bsz < 0) {
                xmodem_flush();
                xmodem_putc(XMODEM_NAK);
                if (++nak_count >= XMODEM_MAX_RETRIES) {
                    xmodem_abort();
                    return XMODEM_ERR_NAK;
                }
                continue;
            }

            if (blk_num != 1u) {
                xmodem_abort();
                return XMODEM_ERR_SEQ;
            }

            if ((uint32_t)bsz > max_len) {
                xmodem_abort();
                return XMODEM_ERR_OVERFLOW;
            }

            memcpy(buf, block_buf, (size_t)bsz);
            received  = (uint32_t)bsz;
            expected  = 2u;
            nak_count = 0;
            xmodem_putc(XMODEM_ACK);
            break;
        }

        if (c == XMODEM_CAN) {
            if (++can_count >= 2) return XMODEM_ERR_ABORT;
        } else {
            can_count = 0;
        }
    }

    for (;;) {
        int c = xmodem_getc(XMODEM_TIMEOUT_PACKET_MS);

        if (c == XMODEM_EOT) {
            xmodem_putc(XMODEM_ACK);
            return (int32_t)received;
        }

        if (c == XMODEM_CAN) {
            if (++can_count >= 2) return XMODEM_ERR_ABORT;
            continue;
        }
        can_count = 0;

        if (c != XMODEM_SOH && c != XMODEM_STX) {
            xmodem_flush();
            xmodem_putc(XMODEM_NAK);
            if (++nak_count >= XMODEM_MAX_RETRIES) {
                xmodem_abort();
                return (c < 0) ? XMODEM_ERR_TIMEOUT : XMODEM_ERR_NAK;
            }
            continue;
        }

        uint8_t blk_num;
        int     bsz = xmodem_recv_block((uint8_t)c, block_buf, &blk_num);

        if (bsz < 0) {
            xmodem_flush();
            xmodem_putc(XMODEM_NAK);
            if (++nak_count >= XMODEM_MAX_RETRIES) {
                xmodem_abort();
                return (bsz == XMODEM_ERR_TIMEOUT) ? XMODEM_ERR_TIMEOUT : XMODEM_ERR_NAK;
            }
            continue;
        }

        if (blk_num == (uint8_t)(expected - 1u)) {
            xmodem_putc(XMODEM_ACK);
            continue;
        }

        if (blk_num != expected) {
            xmodem_abort();
            return XMODEM_ERR_SEQ;
        }

        if (received + (uint32_t)bsz > max_len) {
            xmodem_abort();
            return XMODEM_ERR_OVERFLOW;
        }

        memcpy(buf + received, block_buf, (size_t)bsz);
        received += (uint32_t)bsz;
        expected++;
        nak_count = 0;
        xmodem_putc(XMODEM_ACK);
    }
}

#ifndef HARDWARE_FLASH_BYPASS_H
#define HARDWARE_FLASH_BYPASS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Send tx_len bytes then receive rx_len bytes in a single CS# transaction
// via the SPI peripheral (flash_spi_mux=1 window).
// tx_len must not exceed 16.
// Caller must be executing from IRAM — BK7252 RAM (0x00400000) is non-executable.
bool flash_bypass_xfer(const uint8_t *tx, uint8_t tx_len,
                       uint8_t *rx, uint8_t rx_len);

// Read 16-byte Unique ID into uid[].
bool flash_uid(uint8_t uid[16]);

#ifdef __cplusplus
}
#endif

#endif // HARDWARE_FLASH_BYPASS_H

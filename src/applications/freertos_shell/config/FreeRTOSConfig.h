#ifndef FREERTOS_CONFIG_H

#include "../../../platform/freertos/config/FreeRTOSConfig.h"

// redefine heap size
#define configMAIN_HEAP_SIZE                        ((256 - 56) * 1024)
#define configADDITIONAL_HEAP_SIZE                  (256 * 1024)
#undef configTOTAL_HEAP_SIZE
#define configTOTAL_HEAP_SIZE                       (configMAIN_HEAP_SIZE + configADDITIONAL_HEAP_SIZE)

#endif /* FREERTOS_CONFIG_H */

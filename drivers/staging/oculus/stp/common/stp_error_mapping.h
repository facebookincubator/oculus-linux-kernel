#ifndef STP_ERROR_MAPPING_H
#define STP_ERROR_MAPPING_H

#include <stp/common/stp_common.h>

int spi_stp_dev_map_errors(int stp_error);
#define STP_ERR_VAL(e) spi_stp_dev_map_errors(e)

#define STP_IS_ERR(e) unlikely((e) != 0)

#endif

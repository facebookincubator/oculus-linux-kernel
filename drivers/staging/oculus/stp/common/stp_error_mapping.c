#include <asm-generic/errno.h>

#include <common/stp_device_logging.h>
#include <common/stp_error_mapping.h>

int spi_stp_dev_map_errors(int stp_error)
{
	int ret = 0;

	switch (stp_error) {
	case STP_SUCCESS:
		// case STP_ERROR_NONE:
		ret = 0;
		break;
	case STP_ERROR:
	case STP_ERROR_DEVICE_NOT_CONNECTED:
		ret = -EAGAIN;
		break;
	case STP_ERROR_CONTROLLER_NOT_CONNECTED:
	case STP_ERROR_NOT_SYNCED:
	case STP_ERROR_ALREADY_OPEN:
	case STP_ERROR_ALREADY_CLOSED:
		ret = -EBADF;
		break;
	case STP_ERROR_INVALID_SESSION:
		ret = -EPIPE;
		break;
	case STP_ERROR_INVALID_PARAMETERS:
	case STP_ERROR_STP_NOT_INITIALIZED:
	case STP_ERROR_INVALID_COMMAND:
	case STP_ERROR_SOC_SUSPENDED:
		ret = -EBADF;
		break;
	case STP_ERROR_IO_INTERRUPT:
		ret = -EPIPE;
		break;
	case STP_ERROR_NEXT_ERROR:
		ret = -EBADF;
		break;
	default:
		break;
	}

#if defined(STP_DEBUG_ALL_ERR_CODES) && (STP_DEBUG_ALL_ERR_CODES == 1)
	if (ret != 0)
		STP_DRV_LOG_ERR_RATE_LIMIT("Error found: `%d` converted to `%d`",
				stp_error, ret);
#endif

	return ret;
}

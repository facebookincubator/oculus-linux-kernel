// SPDX-License-Identifier: GPL-2.0
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/spi/spi.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/types.h>

#include <common/stp_device_logging.h>
#include <common/stp_error_mapping.h>
#include <device/stp_device.h>
#include <driver/stp_gpio.h>
#include <stp/common/stp_common_public.h>
#include "stp_emulation.h"
#ifdef STP_EMULATION

int stp_emulation_create_channels(struct stp_channel_data* channel_data) {
  int rval = 0;
  // Channel configurations matching meta-neo-spi-stp.dtsi
  struct {
    uint32_t tx_buffer_size;
    uint32_t rx_buffer_size;
    uint32_t priority;
  } channel_configs[STP_TOTAL_NUM_CHANNELS] = {/* Channel 0-7 */
                                               {4096, 4096, 30},
                                               {4096, 4096, 30},
                                               {4096, 4096, 30},
                                               {4096, 4096, 30},
                                               {4096, 4096, 30},
                                               {4096, 4096, 30},
                                               {4096, 4096, 30},
                                               {4096, 4096, 30},
                                               /* Channel 8-15 */
                                               {4096, 4096, 20},
                                               {4096, 4096, 31},
                                               {4096, 4096, 1},
                                               {4096, 4096, 0},
                                               {4096, 4096, 0},
                                               {4096, 4096, 1},
                                               {4096, 4096, 31},
                                               {4096, 4096, 31},
                                               /* Channel 16-23 */
                                               {4096, 4096, 31},
                                               {4096, 4096, 31},
                                               {4096, 4096, 2},
                                               {4096, 4096, 31},
                                               {4096, 4096, 31},
                                               {4096, 4096, 31},
                                               {4096, 4096, 31},
                                               {4096, 4096, 31},
                                               /* Channel 24-31 */
                                               {4096, 4096, 31},
                                               {4096, 4096, 0},
                                               {4096, 4096, 0},
                                               {4096, 4096, 0},
                                               {4096, 4096, 1},
                                               {4096, 4096, 0},
                                               {8192, 8192, 0},
                                               {8192, 8192, 30}};

  // Create 32 channels with configurations from dtsi
  for (int i = 0; i < STP_TOTAL_NUM_CHANNELS; i++) {
    channel_data->channel = i;
    channel_data->tx_len_bytes = channel_configs[i].tx_buffer_size;
    channel_data->rx_len_bytes = channel_configs[i].rx_buffer_size;
    channel_data->priority = channel_configs[i].priority;

    rval = stp_create_channel(channel_data);
    if (rval != 0) {
      STP_DRV_LOG_ERR("create channel failed for mock channel %d", i);
      break;
    }
  }
  return rval;
}

int stp_emulation_init_gpio(struct device* const dev, struct stp_gpio_data* const data) {
  struct gpio_desc* desc;

  if (!data || !dev) {
    STP_DRV_LOG_ERR("bad gpio input");
    return -EINVAL;
  }

  desc = devm_gpiod_get(dev, "mcu-request-transaction", GPIOD_IN);
  if (IS_ERR(desc)) {
    STP_DRV_LOG_ERR("failed to get gpio mcu-request-transaction: %ld", PTR_ERR(desc));
    return PTR_ERR(desc);
  }
  data->device_request_transaction = desc_to_gpio(desc);
  devm_gpiod_put(dev, desc);

  desc = devm_gpiod_get(dev, "soc-has-data", GPIOD_OUT_HIGH);
  if (IS_ERR(desc)) {
    STP_DRV_LOG_ERR("failed to get gpio soc-has-data: %ld", PTR_ERR(desc));
    return PTR_ERR(desc);
  }
  data->controller_has_data = desc_to_gpio(desc);
  devm_gpiod_put(dev, desc);

  if (stp_gpio_set_direction(data)) {
    STP_DRV_LOG_ERR("failed to set gpio direction");
    return -EINVAL;
  }

  return 0;
}
#endif // STP_EMULATION

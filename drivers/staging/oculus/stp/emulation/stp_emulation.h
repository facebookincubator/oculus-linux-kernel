#ifndef STP_EMULATION_H
#define STP_EMULATION_H
#if defined(CONFIG_X86) || defined(CONFIG_X86_64)
#define STP_EMULATION

int stp_emulation_create_channels(struct stp_channel_data* channel_data);
int stp_emulation_init_gpio(struct device* const dev, struct stp_gpio_data* const data);
#endif /* CONFIG_X86 || CONFIG_X86_64 */
#endif /* STP_EMULATION_H */

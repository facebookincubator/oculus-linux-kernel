#ifndef __MAX17332_CACHE_H_
#define __MAX17332_CACHE_H_


struct max17332_cache_config {
    /* Addresses below define the registers that are cached */
    u8 min_reg_addr;
    u8 max_reg_addr;
    unsigned int cache_expiry_seconds;
    const u8 *reg_skip_cache_expiry_list; /* list of registers for which cache expiry does not apply - example: we always want to return a capacity value to prevent unexpected behavior when read fails */
    unsigned int num_reg_skip_cache_expiry_list;
};

struct max17332_cache_item {
    u16 cache_val;
    unsigned long last_update;
    struct mutex lock; /* Multiple threads will be writing to cache when reading FG registers. This prevents race conditions. */
};

/*
- The purpose of the cache is to allow provide the user space with previosly read register values
in case of any regmap_read failures (which currently results in value of 0 read by userspace)
- The cache will contain two arrays: one for regular registers and one for NVM registers.
- cache size = max register address  - min register address + 1
- register index = register address - min register address
*/
struct max17332_cache {
    const struct max17332_cache_config *config;

    struct max17332_cache_item** reg_cache;
};

struct max17332_cache* max17332_cache_init(const struct max17332_cache_config *config);
int max17332_cache_read(struct max17332_cache *cache, u8 reg, u16 *value);
int max17332_cache_write(struct max17332_cache *cache, u8 reg, u16 value);
int max17332_cache_exit(struct max17332_cache *cache);


#endif //__MAX17332_CACHE_H_

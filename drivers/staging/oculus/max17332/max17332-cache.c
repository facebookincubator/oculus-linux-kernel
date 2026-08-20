#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/jiffies.h>

#include "max17332-cache.h"

bool is_valid_reg_addr(const struct max17332_cache_config *config, u8 reg)
{
    return reg >= config->min_reg_addr && reg <= config->max_reg_addr;
}

size_t get_index(const struct max17332_cache_config *config, u8 reg)
{
    return reg - config->min_reg_addr;
}

bool is_reg_in_skip_cache_expiry_list(const struct max17332_cache_config *config, u8 reg)
{
    int i;

    for (i = 0; i < config->num_reg_skip_cache_expiry_list; i++) {
        if (*(config->reg_skip_cache_expiry_list + i) == reg)
            return true;
    }

    return false;
}

struct max17332_cache* max17332_cache_init(const struct max17332_cache_config *config)
{
    unsigned int reg_cache_size;
    int ret = -EINVAL;
    struct max17332_cache *cache;

    if (!config || config->min_reg_addr > config->max_reg_addr)
        goto err;

    cache = kzalloc(sizeof(*cache), GFP_KERNEL);
    if (!cache) {
        ret = -ENOMEM;
        goto err;
    }

    cache->config = config;

    reg_cache_size = config->max_reg_addr - config->min_reg_addr + 1;

    cache->reg_cache = kcalloc(reg_cache_size, sizeof(struct max17332_cache_item *), GFP_KERNEL);

    if (!cache->reg_cache) {
        ret = -ENOMEM;
        goto err_reg_cache;
    }

    return cache;

err_reg_cache:
    kfree(cache);
err:
    return ERR_PTR(ret);
}

int max17332_cache_read(struct max17332_cache *cache, u8 reg, u16 *value)
{
    u8 index;
    struct max17332_cache_item *cache_val_ptr;
    unsigned long cache_expiry_threshold;
    int ret = 0;

    if (!is_valid_reg_addr(cache->config, reg))
        return -EINVAL;

    index = get_index(cache->config, reg);

    cache_val_ptr = cache->reg_cache[index];

    if (cache_val_ptr == 0) { /* cache value was never written to */
        pr_debug("%s: cache val for register 0x%02x does not exist\n", __func__, reg);
        *value = 0;
        return -ENODATA;
    }

    mutex_lock(&cache_val_ptr->lock);
    if (is_reg_in_skip_cache_expiry_list(cache->config, reg)) {
        *value = cache_val_ptr->cache_val;
    } else {
        cache_expiry_threshold = cache->config->cache_expiry_seconds * HZ;
        if (time_is_after_jiffies(cache_val_ptr->last_update + cache_expiry_threshold)) {
            *value = cache_val_ptr->cache_val;
        } else {
            pr_debug("%s: cache val for register 0x%02x expired\n", __func__, reg);
            *value = 0;
            ret = -ENODATA;
        }
    }
    mutex_unlock(&cache_val_ptr->lock);

    return ret;
}

int max17332_cache_write(struct max17332_cache *cache, u8 reg, u16 value)
{
    u8 index;
    struct max17332_cache_item *cache_val_ptr;

    if (!is_valid_reg_addr(cache->config, reg))
        return -EINVAL;

    index = get_index(cache->config, reg);

    cache_val_ptr = cache->reg_cache[index];

    if (cache_val_ptr == 0) { /* register was never cached - allocate memory for it */
        cache_val_ptr = kzalloc(sizeof(struct max17332_cache_item), GFP_KERNEL);
        if (!cache_val_ptr)
            return -ENOMEM;
        cache_val_ptr->cache_val = value;
        cache_val_ptr->last_update = jiffies;
        mutex_init(&cache_val_ptr->lock);
        cache->reg_cache[index] = cache_val_ptr;

        return 0;
    }

    mutex_lock(&cache_val_ptr->lock);
    cache_val_ptr->cache_val = value;
    cache_val_ptr->last_update = jiffies;
    mutex_unlock(&cache_val_ptr->lock);

    return 0;
}

int max17332_cache_exit(struct max17332_cache *cache)
{
    int i;
    const struct max17332_cache_config* config = cache->config;
    size_t reg_cache_size = config->max_reg_addr - config->min_reg_addr + 1;
    struct max17332_cache_item *cache_val_ptr;

    for (i = 0; i < reg_cache_size; i++) {
        cache_val_ptr = cache->reg_cache[i];
        if (cache_val_ptr) {
            mutex_destroy(&cache_val_ptr->lock);
            kfree(cache_val_ptr);
        }
    }

    kfree(cache->reg_cache);
    cache->reg_cache = NULL;
    kfree(cache);
    cache = NULL;

    return 0;
}

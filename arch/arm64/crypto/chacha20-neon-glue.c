/*
 * ChaCha20 256-bit cipher algorithm, RFC7539, arm64 NEON functions
 *
 * Copyright (C) 2016 - 2017 Linaro, Ltd. <ard.biesheuvel@linaro.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Based on:
 * ChaCha20 256-bit cipher algorithm, RFC7539, SIMD glue code
 *
 * Copyright (C) 2015 Martin Willi
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <crypto/algapi.h>
#include <crypto/chacha.h>
#include <crypto/internal/skcipher.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include <asm/hwcap.h>
#include <asm/neon.h>
#include <asm/simd.h>

asmlinkage void chacha20_block_xor_neon(u32 *state, u8 *dst, const u8 *src);
asmlinkage void chacha20_4block_xor_neon(u32 *state, u8 *dst, const u8 *src);

static void chacha20_doneon(u32 *state, u8 *dst, const u8 *src,
			    unsigned int bytes)
{
	u8 buf[CHACHA_BLOCK_SIZE];

	while (bytes >= CHACHA_BLOCK_SIZE * 4) {
		kernel_neon_begin();
		chacha20_4block_xor_neon(state, dst, src);
		kernel_neon_end();
		bytes -= CHACHA_BLOCK_SIZE * 4;
		src += CHACHA_BLOCK_SIZE * 4;
		dst += CHACHA_BLOCK_SIZE * 4;
		state[12] += 4;
	}

	if (!bytes)
		return;

	kernel_neon_begin();
	while (bytes >= CHACHA_BLOCK_SIZE) {
		chacha20_block_xor_neon(state, dst, src);
		bytes -= CHACHA_BLOCK_SIZE;
		src += CHACHA_BLOCK_SIZE;
		dst += CHACHA_BLOCK_SIZE;
		state[12]++;
	}
	if (bytes) {
		memcpy(buf, src, bytes);
		chacha20_block_xor_neon(state, buf, buf);
		memcpy(dst, buf, bytes);
	}
	kernel_neon_end();
}

static int chacha20_neon(struct skcipher_request *req)
{
	struct crypto_skcipher *tfm = crypto_skcipher_reqtfm(req);
	struct chacha_ctx *ctx = crypto_skcipher_ctx(tfm);
	struct skcipher_walk walk;
	u32 state[16];
	int err;

	if (!may_use_simd() || req->cryptlen <= CHACHA_BLOCK_SIZE)
		return crypto_chacha_crypt(req);

	err = skcipher_walk_virt(&walk, req, false);

	crypto_chacha_init(state, ctx, walk.iv);

	while (walk.nbytes > 0) {
		unsigned int nbytes = walk.nbytes;

		if (nbytes < walk.total)
			nbytes = round_down(nbytes, walk.stride);

		chacha20_doneon(state, walk.dst.virt.addr, walk.src.virt.addr,
				nbytes);
		err = skcipher_walk_done(&walk, walk.nbytes - nbytes);
	}

	return err;
}

static struct skcipher_alg alg = {
	.base.cra_name		= "chacha20",
	.base.cra_driver_name	= "chacha20-neon",
	.base.cra_priority	= 300,
	.base.cra_blocksize	= 1,
	.base.cra_ctxsize	= sizeof(struct chacha_ctx),
	.base.cra_module	= THIS_MODULE,

	.min_keysize		= CHACHA_KEY_SIZE,
	.max_keysize		= CHACHA_KEY_SIZE,
	.ivsize			= CHACHA_IV_SIZE,
	.chunksize		= CHACHA_BLOCK_SIZE,
	.walksize		= 4 * CHACHA_BLOCK_SIZE,
	.setkey			= crypto_chacha20_setkey,
	.encrypt		= chacha20_neon,
	.decrypt		= chacha20_neon,
};

static int __init chacha20_simd_mod_init(void)
{
	if (!(elf_hwcap & HWCAP_ASIMD))
		return -ENODEV;

	return crypto_register_skcipher(&alg);
}

static void __exit chacha20_simd_mod_fini(void)
{
	crypto_unregister_skcipher(&alg);
}

module_init(chacha20_simd_mod_init);
module_exit(chacha20_simd_mod_fini);

MODULE_AUTHOR("Ard Biesheuvel <ard.biesheuvel@linaro.org>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS_CRYPTO("chacha20");

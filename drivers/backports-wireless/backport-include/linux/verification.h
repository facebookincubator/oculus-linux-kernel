#ifndef __BP_VERIFICATION_H
#define __BP_VERIFICATION_H
#include <linux/version.h>
#if LINUX_VERSION_IS_GEQ(4,7,0) && !defined(CPTCFG_BPAUTO_BUILD_SYSTEM_DATA_VERIFICATION)
#include_next <linux/verification.h>
#else
#include <linux/key.h>

enum key_being_used_for {
	VERIFYING_MODULE_SIGNATURE,
	VERIFYING_FIRMWARE_SIGNATURE,
	VERIFYING_KEXEC_PE_SIGNATURE,
	VERIFYING_KEY_SIGNATURE,
	VERIFYING_KEY_SELF_SIGNATURE,
	VERIFYING_UNSPECIFIED_SIGNATURE,
	NR__KEY_BEING_USED_FOR
};

extern int verify_pkcs7_signature(const void *data, size_t len,
				  const void *raw_pkcs7, size_t pkcs7_len,
				  struct key *trusted_keys,
				  enum key_being_used_for usage,
				  int (*view_content)(void *ctx,
						      const void *data, size_t len,
						      size_t asn1hdrlen),
				  void *ctx);
#endif /* LINUX_VERSION_IS_GEQ(4,7,0) && !defined(CPTCFG_BPAUTO_BUILD_SYSTEM_DATA_VERIFICATION) */
#endif /* __BP_VERIFICATION_H */

/* 
 * DES & Triple DES EDE Cipher Algorithms.
 */

#ifndef __CRYPTO_DES_H
#define __CRYPTO_DES_H

#define DES_KEY_SIZE		8
#define DES_EXPKEY_WORDS	32
#define DES_BLOCK_SIZE		8

#define DES3_EDE_KEY_SIZE	(3 * DES_KEY_SIZE)
#define DES3_EDE_EXPKEY_WORDS	(3 * DES_EXPKEY_WORDS)
#define DES3_EDE_BLOCK_SIZE	DES_BLOCK_SIZE

/**
 * crypto_des_verify_key - Check whether a DES key is weak
 * @tfm: the crypto algo
 * @key: the key buffer
 *
 * Returns -EINVAL if the key is weak and the crypto TFM does not permit weak
 * keys. Otherwise, 0 is returned.
 *
 * It is the job of the caller to ensure that the size of the key equals
 * DES_KEY_SIZE.
 */
int verify_skcipher_des_key(struct crypto_tfm *tfm, const u8 *key);

/*
 * RFC2451:
 *
 *   For DES-EDE3, there is no known need to reject weak or
 *   complementation keys.  Any weakness is obviated by the use of
 *   multiple keys.
 *
 *   However, if the first two or last two independent 64-bit keys are
 *   equal (k1 == k2 or k2 == k3), then the DES3 operation is simply the
 *   same as DES.  Implementers MUST reject keys that exhibit this
 *   property.
 *
 */
int verify_skcipher_des3_key(struct crypto_tfm *tfm, const u8 *key);

static inline int verify_aead_des_key(struct crypto_tfm *tfm, const u8 *key,
				      int keylen)
{
	if (keylen != DES_KEY_SIZE)
		return -EINVAL;

	return verify_skcipher_des_key(tfm, key);
}

static inline int verify_aead_des3_key(struct crypto_tfm *tfm, const u8 *key,
				       int keylen)
{
	if (keylen != DES3_EDE_KEY_SIZE)
		return -EINVAL;

	return verify_skcipher_des3_key(tfm, key);
}

extern unsigned long des_ekey(u32 *pe, const u8 *k);

extern int __des3_ede_setkey(u32 *expkey, u32 *flags, const u8 *key,
			     unsigned int keylen);

#endif /* __CRYPTO_DES_H */

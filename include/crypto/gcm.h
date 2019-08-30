#ifndef _CRYPTO_GCM_H
#define _CRYPTO_GCM_H

#define GCM_AES_IV_SIZE 12
#define GCM_RFC4106_IV_SIZE 8
#define GCM_RFC4543_IV_SIZE 8

/*
 * validate authentication tag for GCM
 */
static inline int crypto_gcm_check_authsize(unsigned int authsize)
{
	switch (authsize) {
	case 4:
	case 8:
	case 12:
	case 13:
	case 14:
	case 15:
	case 16:
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

#endif

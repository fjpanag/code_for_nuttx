/*******************************************************************************
 *
 *	eXtended Tiny Encryption Algorithm.
 *
 *	File:	xtea.h
 *  Author:	Fotis Panagiotopoulos
 *  Date:	08/06/2021
 *
 *
 ******************************************************************************/

#ifndef XTEA_H
#define XTEA_H

#include <stdint.h>

/* Number of XTEA rounds. */
#define XTEA_NUMBER_OF_ROUNDS 16

/* XTEA context. */
typedef struct {
	uint32_t key[4];
} XTEA_t;


/*
 *	Initializes the XTEA context.128-bit key for encryption/decryption.
 *
 *	Parameters:
 *		key			128-bit (4 x 32-bit) key for encryption/decryption.
 */
void XTEA_init(XTEA_t * xtea, const uint32_t key[4]);

/*
 *	Encrypts 64-bit data with an 128-bit key.
 *
 *	Parameters:
 *		data		2 x 32-bit data.
 */
void XTEA_encrypt(XTEA_t * xtea, uint32_t data[2]);

/*
 *	Decrypt 64-bit data with an 128-bit key.
 *
 *	Parameters:
 *		data		2 x 32-bit data.
 */
void XTEA_decrypt(XTEA_t * xtea, uint32_t data[2]);


#endif


/*******************************************************************************
 *
 *	eXtended Tiny Encryption Algorithm.
 *
 *	File:	xtea.c
 *  Author:	Fotis Panagiotopoulos
 *  Date:	08/06/2021
 *
 *
 ******************************************************************************/

#include "xtea.h"
#include <stdint.h>

#define XTEA_DELTA            0x9E3779B9


void XTEA_init(XTEA_t * xtea, const uint32_t key[4])
{
	for (int i = 0; i < 4; i++)
		xtea->key[i] = key[i];
}

void XTEA_encrypt(XTEA_t * xtea, uint32_t data[2])
{
	uint32_t d0 = data[0];
	uint32_t d1 = data[1];
	uint32_t sum = 0;

	for (int i = XTEA_NUMBER_OF_ROUNDS; i != 0; i--)
	{
		d0 += (((d1 << 4) ^ (d1 >> 5)) + d1) ^ (sum + xtea->key[sum & 3]);
		sum += XTEA_DELTA;
		d1 += (((d0 << 4) ^ (d0 >> 5)) + d0) ^ (sum + xtea->key[(sum >> 11) & 3]);
	}

	data[0] = d0;
	data[1] = d1;
}

void XTEA_decrypt(XTEA_t * xtea, uint32_t data[2])
{
	uint32_t d0 = data[0];
	uint32_t d1 = data[1];
	uint32_t sum = XTEA_DELTA * XTEA_NUMBER_OF_ROUNDS;

	for (int i = XTEA_NUMBER_OF_ROUNDS; i != 0; i--)
	{
		d1 -= (((d0 << 4) ^ (d0 >> 5)) + d0) ^ (sum + xtea->key[(sum >> 11) & 3]);
		sum -= XTEA_DELTA;
		d0 -= (((d1 << 4) ^ (d1 >> 5)) + d1) ^ (sum + xtea->key[sum & 3]);
	}

	data[0] = d0;
	data[1] = d1;
}


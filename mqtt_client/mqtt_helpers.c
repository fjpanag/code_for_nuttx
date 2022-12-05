/*******************************************************************************
 *
 *	MQTT helpers.
 *
 *	File:	mqtt_helpers.c
 *  Author:	Fotis Panagiotopoulos
 *  Date:	03/06/2022
 *
 *
 ******************************************************************************/

#include "mqtt_helpers.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>


int MQTT_encodeSize(uint8_t * buf, size_t length)
{
	int idx = 0;

	do {
		char d = length % 128;
		length /= 128;

		if (length > 0)
			d |= 0x80;

		buf[idx++] = d;
	} while (length > 0);

	return idx;
}

int MQTT_decodeSize(uint8_t * buf, size_t * value)
{
	*value = 0;

	unsigned char c;
	int multiplier = 1;
	int len = 0;

	do {
		if (++len > 4)
			goto exit;

		c = *buf++;

		*value += (c & 0x7F) * multiplier;
		multiplier *= 128;
	} while ((c & 0x80) != 0);

exit:
	return len;
}

char MQTT_readChar(uint8_t ** pptr)
{
	char c = **pptr;
	(*pptr)++;
	return c;
}

void MQTT_writeChar(uint8_t ** pptr, char c)
{
	**pptr = c;
	(*pptr)++;
}

int MQTT_readInt(uint8_t ** pptr)
{
	unsigned char * ptr = *pptr;
	int len = 256 * (*ptr) + (*(ptr + 1));
	*pptr += 2;
	return len;
}

void MQTT_writeInt(uint8_t ** pptr, int i)
{
	**pptr = (unsigned char)(i / 256);
	(*pptr)++;
	**pptr = (unsigned char)(i % 256);
	(*pptr)++;
}

int MQTT_readString(char ** string, uint8_t ** pptr, const uint8_t * end)
{
	if (end - (*pptr) <= 1)
		return 0;

	int len = MQTT_readInt(pptr);

	if (&(*pptr)[len] > end)
		return 0;

	*string = malloc(len + 1);
	if ((*string) == NULL)
		return 0;

	memcpy(*string, *pptr, len);
	(*string)[len] = '\0';

	*pptr += len;

	return len;
}

void MQTT_writeString(uint8_t ** pptr, const char * string)
{
	if (!string)
	{
		MQTT_writeInt(pptr, 0);
		return;
	}

	int len = (int)strlen(string);

	MQTT_writeInt(pptr, len);
	memcpy(*pptr, string, len);
	*pptr += len;
}


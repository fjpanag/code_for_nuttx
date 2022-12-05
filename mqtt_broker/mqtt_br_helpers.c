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

#include "mqtt_br_helpers.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <nuttx/config.h>
#include <sys/types.h>

#ifdef CONFIG_MQTT_BROKER

static int utf8_validate(char * string, size_t len);


int MQTT_br_encodeSize(uint8_t * buf, int length)
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

int MQTT_br_decodeSize(uint8_t * buf, int * value)
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

char MQTT_br_readChar(uint8_t ** pptr)
{
	char c = **pptr;
	(*pptr)++;
	return c;
}

void MQTT_br_writeChar(uint8_t ** pptr, char c)
{
	**pptr = c;
	(*pptr)++;
}

int MQTT_br_readInt(uint8_t ** pptr)
{
	unsigned char * ptr = *pptr;
	int len = 256 * (*ptr) + (*(ptr + 1));
	*pptr += 2;
	return len;
}

void MQTT_br_writeInt(uint8_t ** pptr, int i)
{
	**pptr = (unsigned char)(i / 256);
	(*pptr)++;
	**pptr = (unsigned char)(i % 256);
	(*pptr)++;
}

size_t MQTT_br_readString(char ** string, uint8_t ** pptr, const uint8_t * end)
{
	if (end - (*pptr) <= 1)
		return 0;

	size_t len = MQTT_br_readInt(pptr);

	if (len == 0)
		return 0;

	if (&(*pptr)[len] > end)
		return 0;

	*string = malloc(len + 1);
	if ((*string) == NULL)
		return 0;

	memcpy(*string, *pptr, len);
	(*string)[len] = '\0';

	*pptr += len;

	//Strings must must contain only valid UTF-8 characters.
	if (utf8_validate(*string, len) == 0)
	{
		free(*string);
		*string = NULL;
		return 0;
	}

	return len;
}

void MQTT_br_writeString(uint8_t ** pptr, const char * string)
{
	if (!string)
	{
		MQTT_br_writeInt(pptr, 0);
		return;
	}

	size_t len = strlen(string);

	MQTT_br_writeInt(pptr, (int)len);
	memcpy(*pptr, string, len);
	*pptr += len;
}


int utf8_validate(char * string, size_t len)
{
	int cnt = 0;

	for (unsigned i = 0; i < len; i++)
	{
		uint8_t c = (uint8_t)string[i];

		if (c == 0)
			return 0;

		if (cnt == 0)
		{
			if ((c >> 5) == 0x06)
				cnt = 1;
			else if ((c >> 4) == 0x0E)
				cnt = 2;
			else if ((c >> 3) == 0x1E)
				cnt = 3;
			else if ((c >> 7) != 0x00)
				return 0;
		}
		else
		{
			if ((c >> 6) != 0x02)
				return 0;

			cnt--;
		}
	}

	return (cnt == 0);
}

#endif


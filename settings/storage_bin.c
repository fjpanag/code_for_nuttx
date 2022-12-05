/*******************************************************************************
 *
 *	Settings binary storage.
 *
 *	File:	storage_bin.c
 *  Author:	Fotis Panagiotopoulos
 *  Date:	07/05/2021
 *
 *
 ******************************************************************************/

#include "storage.h"
#include "settings.h"
#include <unistd.h>
#include <fcntl.h>
#include <nuttx/crc32.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <nuttx/config.h>
#include <sys/types.h>

#define VALID			0x600D
#define BUFFER_SIZE		256     //Note alignment for Flash writes!

static Setting_t * getSetting(char * key);

extern Setting_t map[CONFIG_SETTINGS_MAP_SIZE];


void load_bin(char * file)
{
	int fd = open(file, O_RDONLY);
	if (fd < 0)
		return;

	uint16_t valid = 0;
	read(fd, &valid, sizeof(uint16_t));

	if (valid != VALID)
		goto abort;

	uint16_t count = 0;
	read(fd, &count, sizeof(uint16_t));

	uint32_t calc_crc = 0;
	uint32_t exp_crc = 0;

	for (int i = 0; i < count; i++)
	{
		Setting_t setting;
		read(fd, &setting, sizeof(Setting_t));
		calc_crc = crc32part((uint8_t*)&setting, sizeof(Setting_t), calc_crc);
	}

	read(fd, &exp_crc, sizeof(uint32_t));

	if (calc_crc != exp_crc)
		goto abort;

	lseek(fd, (sizeof(uint16_t) * 2), SEEK_SET);  //Get after valid & size.

	for (int i = 0; i < count; i++)
	{
		Setting_t setting;
		read(fd, &setting, sizeof(Setting_t));

		Setting_t * slot = getSetting(setting.key);
		if (slot == NULL)
			continue;

		memcpy(slot, &setting, sizeof(Setting_t));
	}

abort:
	close(fd);
}

void save_bin(char * file)
{
	uint8_t * buffer = malloc(BUFFER_SIZE);
	if (buffer == NULL)
		return;

	int count = 0;
	for (int i = 0; i < CONFIG_SETTINGS_MAP_SIZE; i++)
	{
		if (map[i].type == SETTING_EMPTY)
			break;

		count++;
	}

	int fd = open(file, (O_RDWR | O_TRUNC), 0666);
	if (fd < 0)
		goto abort;

	memset(buffer, 0xFF, BUFFER_SIZE);
	*((uint16_t*)buffer) = VALID;
	*(((uint16_t*)buffer) + 1) = count;

	off_t offset = sizeof(uint16_t) * 2;  //Valid & count.
	size_t data_size = count * sizeof(Setting_t);
	size_t rem_data = data_size;
	uint32_t crc = crc32((uint8_t*)map, data_size);
	size_t rem_crc = sizeof(crc);

	while ((offset + rem_data + rem_crc) > 0)
	{
		size_t to_write = ((offset + rem_data) > BUFFER_SIZE) ? (size_t)(BUFFER_SIZE - offset) : rem_data;
		memcpy((buffer + offset), (((uint8_t*)map) + (data_size - rem_data)), to_write);

		for (size_t i = (to_write + offset); (i < BUFFER_SIZE) && (rem_crc > 0); i++, rem_crc--)
		{
			off_t crc_byte = (off_t)(sizeof(crc) - rem_crc);
			buffer[i] = (crc >> (8 * crc_byte)) & 0xFF;
		}

		write(fd, buffer, BUFFER_SIZE);

		rem_data -= to_write;
		offset = 0;
	}

	close(fd);

abort:
	free(buffer);
}


Setting_t * getSetting(char * key)
{
	for (int i = 0; i < CONFIG_SETTINGS_MAP_SIZE; i++)
	{
		Setting_t * setting = &map[i];

		if (strcmp(key, setting->key) == 0)
			return setting;

		if (setting->type == SETTING_EMPTY)
			return setting;
	}

	return NULL;
}


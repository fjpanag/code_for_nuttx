/*******************************************************************************
 *
 *	Settings text storage.
 *
 *	File:	storage_text.c
 *  Author:	Fotis Panagiotopoulos
 *  Date:	07/05/2021
 *
 *
 ******************************************************************************/

#include "storage.h"
#include "settings.h"
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <nuttx/config.h>
#include <sys/types.h>

#define BUFFER_SIZE		256

static Setting_t * getSetting(char * key);

extern Setting_t map[CONFIG_SETTINGS_MAP_SIZE];


void load_text(char * file)
{
	//Check that the file exists.
	if (access(file, F_OK) != 0)
	{
		//If not, try the backup file.
		char * backup_file = malloc(strlen(file) + 2);
		if (backup_file == NULL)
			return;

		strcpy(backup_file, file);
		strcat(backup_file, "~");

		if (access(backup_file, F_OK) == 0)
		{
			//There is a backup file.
			//Restore this as the new settings file.
			rename(backup_file, file);
		}

		free(backup_file);
	}

	FILE * f;
	f = fopen(file, "r");

	if (f == NULL)
		return;

	char * buffer = malloc(BUFFER_SIZE);
	if (buffer == NULL)
		goto abort;

	while (fgets(buffer, BUFFER_SIZE, f))
	{
		//Remove any line terminators.
		for (int i = ((int)strlen(buffer) - 1); i > 0; i--)
		{
			if (buffer[i] == ';' || buffer[i] == '\n' || buffer[i] == '\r')
				buffer[i] = '\0';
			else
				break;
		}

		//Separate the key / value pair.
		char * eq = strchr(buffer, '=');
		if (eq == NULL)
			continue;

		char * key = buffer;
		char * val = eq + 1;
		*eq = '\0';

		//Check if the key is valid.
		if (!isalpha(key[0]))
			continue;

		//Get the setting slot.
		Setting_t * setting = getSetting(key);
		if (setting == NULL)
			continue;

		//Parse the value.
		if (isalpha(val[0]))
		{
			if (strcasecmp(val, "true") == 0)
			{
				//It's a boolean.
				setting->type = SETTING_BOOL;
				setting->i = 1;
			}
			else if (strcasecmp(val, "false") == 0)
			{
				//It's a boolean.
				setting->type = SETTING_BOOL;
				setting->i = 0;
			}
			else
			{
				//It's a string.
				if (strlen(val) >= CONFIG_SETTINGS_VALUE_SIZE)
					continue;

				setting->type = SETTING_STRING;
				strncpy(setting->s, val, CONFIG_SETTINGS_VALUE_SIZE);
				setting->s[CONFIG_SETTINGS_VALUE_SIZE - 1] = '\0';
			}
		}
		else
		{
			if (strchr(val, '.') != NULL)
			{
				char * s = val;
				int i = 0;
				while (s[i]) s[i] == '.' ? i++ : *s++;
				if (i == 1)
				{
					//It's a float.
					double d = 0;
					if (sscanf(val, "%lf", &d) == 1)
					{
						setting->type = SETTING_FLOAT;
						setting->f = d;
					}
				}
				else if (i == 3)
				{
					//It's an IP address.
					setting->type = SETTING_IP_ADDR;
					inet_pton(AF_INET, val, &setting->ip);
				}
			}
			else
			{
				//It's an integer.
				int i = 0;
				if (sscanf(val, "%d", &i) == 1)
				{
					setting->type = SETTING_INT;
					setting->i = i;
				}
			}
		}

		//Handle parse errors.
		if (setting->type == SETTING_EMPTY)
			memset(setting->key, 0, CONFIG_SETTINGS_KEY_SIZE);
	}

	free(buffer);

abort:
	fclose(f);
}

void save_text(char * file)
{
	char * backup_file = malloc(strlen(file) + 2);
	if (backup_file == NULL)
		return;

	strcpy(backup_file, file);
	strcat(backup_file, "~");

	FILE * f;
	f = fopen(backup_file, "w");

	if (f == NULL)
		goto abort;

	for (int i = 0; i < CONFIG_SETTINGS_MAP_SIZE; i++)
	{
		if (map[i].type == SETTING_EMPTY)
			break;

		switch (map[i].type)
		{
			case SETTING_STRING:
				fprintf(f, "%s=%s\n", map[i].key, map[i].s);
				break;

			case SETTING_INT:
				fprintf(f, "%s=%d\n", map[i].key, map[i].i);
				break;

			case SETTING_BOOL:
				fprintf(f, "%s=%s\n", map[i].key, map[i].i ? "true" : "false");
				break;

			case SETTING_FLOAT:
				fprintf(f, "%s=%.06f\n", map[i].key, map[i].f);
				break;

			case SETTING_IP_ADDR:
			{
				char ip_str[INET_ADDRSTRLEN];
				inet_ntop(AF_INET, &map[i].ip, ip_str, sizeof(ip_str));
				fprintf(f, "%s=%s\n", map[i].key, ip_str);
				break;
			}
		}
	}

	fclose(f);

	remove(file);
	rename(backup_file, file);

abort:
	free(backup_file);
}


Setting_t * getSetting(char * key)
{
	if (strlen(key) >= CONFIG_SETTINGS_KEY_SIZE)
		return NULL;

	for (int i = 0; i < CONFIG_SETTINGS_MAP_SIZE; i++)
	{
		Setting_t * setting = &map[i];

		if (strcmp(key, setting->key) == 0)
			return setting;

		if (setting->type == SETTING_EMPTY)
		{
			strncpy(setting->key, key, CONFIG_SETTINGS_KEY_SIZE);
			setting->key[CONFIG_SETTINGS_KEY_SIZE - 1] = '\0';
			return setting;
		}
	}

	return NULL;
}


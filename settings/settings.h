/*******************************************************************************
 *
 *	Settings storage.
 *
 *	File:	settings.h
 *  Author:	Fotis Panagiotopoulos
 *  Date:	05/05/2021
 *
 *  The settings storage can be used to store and retrieve various configurable
 *  parameters. This storage is a RAM-based map (for fast access), but one or
 *  more files can be used too (so values can persist across system reboots).
 *
 *  Each setting is a key / value pair.
 *  The key is always a string, while there are numerous value types supported.
 *
 *  All strings, whether they represent a key or a value, must use a specific
 *  format. They must be ASCII, NULL-terminated strings, and they are always
 *  case-sensitive. They must obey to the following rules:
 *  * They cannot start with a number.
 *  * They cannot contain the characters '=', ';'.
 *  * They cannot contain the characters '\n', '\r'.
 *
 *  Since each setting has its own type, it is the user's responsibility to
 *  access the setting using the correct type. Some "casts" are possible (e.g.
 *  between bool and int), but generally reading with the wrong type will lead
 *  to failure.
 *
 *  There are also various types of files that can be used. Each file type
 *  is using a different data format to parse the stored data. Every type is
 *  expected to be best suited for a specific file system or medium type, or
 *  be better performing while used with external systems. In any case, all
 *  configured files are automatically synchronized whenever a value changes.
 *
 *  This is a thread-safe implementation. Different threads may access the
 *  settings simultaneously.
 *
 *
 ******************************************************************************/

#ifndef SETTINGS_H_
#define SETTINGS_H_

#include "storage.h"
#include <netinet/in.h>
#include <stdint.h>
#include <sys/types.h>

/* Setting */
typedef struct {
	char key[CONFIG_SETTINGS_KEY_SIZE];
	int type;
	union {
		int i;
		double f;
		char s[CONFIG_SETTINGS_VALUE_SIZE];
		struct in_addr ip;
	};
} Setting_t;

/* Settings types. */
enum {
	SETTING_EMPTY		= 0,
	SETTING_INT			= 1,
	SETTING_BOOL		= 2,
	SETTING_FLOAT		= 3,
	SETTING_STRING		= 4,
	SETTING_IP_ADDR		= 5
};


/*
 *	Initializes the settings storage.
 */
void Settings_init(void);

/*
 *	Sets a file to be used as a settings storage.
 *	All files are automatically loaded when they are set.
 *
 *	Except from the first file, if loading the file causes any changes
 *	to the settings, then the new map will be dumped to all files
 *	(effectively it syncs all storages).
 *
 *	Note! Storages are assigned a priority based on the order that they
 *	are added. The first storage gets the lowest priority, and the last
 *	gets the highest. If a setting is found to be present and set in
 *	multiple storages, the final value to be kept will be of that of the
 *	highest priority (i.e. the last added).
 *
 *	Parameters:
 *		file		The file name of the storage to use.
 *		type		The type of the storage.
 */
void Settings_setStorage(char * file, int type);

/*
 *	Synchronizes all settings storages.
 *
 *	All storages are loaded according to their priority (i.e. the order
 *	they were added), and the final values are re-written to all of them.
 */
void Settings_sync(void);

/*
 *	Registers a task to be notified on any change of the settings.
 *
 *	Whenever any value is changed, a signal will be sent to all
 *	registered threads. Signals are NOT sent when new settings are
 *	created or when the whole storage is cleared.
 *
 *	Parameters:
 *		signo		Signal number.
 */
void Settings_notify(uint8_t signo);

/*
 *	Gets the hash of the settings storage.
 *
 *	This hash represents the internal state of the settings map. A
 *	unique number is calculated based on the contents of the whole map.
 *
 *	This hash can be used to check the settings for any alterations, i.e.
 *	any setting that may had its value changed since last check.
 *
 *	Returns the hash of the storage.
 */
uint32_t Settings_hash(void);

/*
 *	Clears all settings.
 *	Data in all storages are purged.
 *
 *	Note that if the settings are cleared during the application run-time
 *	(i.e. not during initialization), every access to the settings storage
 *	will fail.
 *
 *	All settings must be created again. This can be done either by calling
 *	Settings_create() again, or by restarting the application.
 */
void Settings_clear(void);


/*
 *	Creates a new setting.
 *
 *	If the setting is found to exist in any of the storages, it will
 *	be loaded. Else, it will be created and the default value will be
 *	assigned.
 *
 *	Parameters:
 *		key			The key of the setting.
 *		type		The type of the setting.
 *		...			The default value of the setting.
 *
 *	Returns 1 if the setting was created successfully, or 0 otherwise.
 */
int Settings_create(char * key, int type, ...);

/*
 *	Gets the type of a setting.
 *
 *	Parameters:
 *		key			The key of the setting.
 *
 *	Returns the type of the setting, or SETTING_EMPTY if not found.
 */
int Settings_type(char * key);

/*
 *	Gets the value of a setting.
 *
 *	Warning! The setting MUST be read with the correct type argument.
 *	Trying to read a setting with the wrong type will result in a
 *	failure. It is the user's responsibility to keep track of each
 *	setting's type.
 *
 *	Parameters:
 *		key			The key of the setting.
 *		type		The type of the setting.
 *		...			Pointer to store the setting value.
 *
 *	Returns 1 if the setting was read successfully, or 0 otherwise.
 */
int Settings_get(char * key, int type, ...);

/*
 *	Sets the value of a setting.
 *
 *	Note! If the setting is of a different type than what it is
 *	specified with this function, it will be changed to the new
 *	type! Any subsequent gets MUST use the new type.
 *
 *	Parameters:
 *		key			The key of the setting.
 *		type		The type of the setting.
 *		...			The new value of the setting.
 *
 *	Returns 1 if the setting was written successfully, or 0 otherwise.
 */
int Settings_set(char * key, int type, ...);


/*
 *	Gets a copy of a setting at the specified
 *	position.
 *
 *	It can be used to iterate over the settings
 *	map, by using successive values of idx.
 *
 *	Parameters:
 *		idx			Iteration index.
 *		setting		Pointer to store the setting.
 *
 *	Returns 1 if a new setting was read, 0 otherwise.
 */
int Settings_iterate(int idx, Setting_t * setting);


#endif


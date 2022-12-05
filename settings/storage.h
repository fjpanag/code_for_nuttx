/*******************************************************************************
 *
 *	Settings storages.
 *
 *	File:	storage.h
 *  Author:	Fotis Panagiotopoulos
 *  Date:	07/05/2021
 *
 *
 ******************************************************************************/

#ifndef STORAGE_H_
#define STORAGE_H_

#include <nuttx/config.h>

/* Settings storage. */
typedef struct {
	char file[CONFIG_SETTINGS_MAX_FILENAME];

	void (*load_fn)(char * file);
	void (*save_fn)(char * file);
} Storage_t;

/* Storages types. */
enum {
	STORAGE_BINARY	= 0,
	STORAGE_TEXT	= 1
};


/* Text storage. */
void load_text(char * file);
void save_text(char * file);

/* Binary storage. */
void load_bin(char * file);
void save_bin(char * file);


#endif


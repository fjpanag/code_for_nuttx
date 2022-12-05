/*******************************************************************************
 *
 *	Geolocation service.
 *
 *	File:	geolocation.h
 *  Author:	Fotis Panagiotopoulos
 *  Date:	22/10/2023
 *
 *
 ******************************************************************************/

#ifndef GEOLOCATION_H_
#define GEOLOCATION_H_

#include <netinet/in.h>
#include <nuttx/config.h>

#ifdef CONFIG_GEOLOCATION

/* Geolocation data. */
typedef struct {
	struct in_addr ip;

	struct {
		char * continent;
		char * country;
		char * city;
	} location;

	struct {
		double latitude;
		double longitude;
	} coordinates;

	struct {
		int offset;
		int dst;
	} timezone;

} Geolocation_t;


/*
 *	Starts the Geolocation service.
 */
void Geolocation_start(void);

/*
 *	Gets the current geolocation data.
 *
 *	Parameters:
 *		geo_data		Pointer to the geolocation data
 *						structure to populate.
 */
void Geolocation_getData(Geolocation_t * geo_data);


#endif

#endif


/*******************************************************************************
 *
 *	Sun calculations.
 *
 *	File:	sun.h
 *  Author:	Fotis Panagiotopoulos
 *  Date:	27/10/2022
 *
 *
 ******************************************************************************/

#ifndef SUN_H_
#define SUN_H_

#include <time.h>

/* Sun data. */
typedef struct {
	time_t sunrise;
	time_t sunset;
	time_t day_duration;
	double sun_height;
	double max_radiation;
} Sun_t;


/*
 *	Calculates the sun data of the day.
 *
 *	Parameters:
 *		sun			Pointer to sun data struct to
 *					store the results.
 *		lat			The latitude to calculate for.
 *		lon			The longitude to calculate for.
 */
void Sun_getData(Sun_t * sun, double lat, double lon);


#endif


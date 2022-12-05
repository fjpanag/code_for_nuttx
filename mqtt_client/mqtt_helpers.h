/*******************************************************************************
 *
 *	MQTT helpers.
 *
 *	File:	mqtt_helpers.h
 *  Author:	Fotis Panagiotopoulos
 *  Date:	03/06/2022
 *
 *
 ******************************************************************************/

#ifndef MQTT_HELPERS_H_
#define MQTT_HELPERS_H_

#include <stdint.h>
#include <sys/types.h>


/*
 *	Encodes the message length according to the MQTT algorithm.
 *
 *	Parameters:
 *		buf			Buffer to store the result.
 *		length		The length of the buffer.
 *
 *	Returns the number of bytes written to the buffer.
 */
int MQTT_encodeSize(uint8_t * buf, size_t length);

/*
 *	Decodes the message length according to the MQTT algorithm.
 *
 *	Parameters:
 *		buf			Buffer containing the received message.
 *		value		The length of the message, as decoded.
 *
 *	Returns the number of bytes processed from the buffer.
 */
int MQTT_decodeSize(uint8_t * buf, size_t * value);

/*
 *	Reads one character from the input buffer.
 *
 *	The input buffer is automatically advanced.
 *
 *	Parameters:
 *		pptr		Pointer to the input buffer.
 *
 *	Returns the character read.
 */
char MQTT_readChar(uint8_t ** pptr);

/*
 *	Writes one character to the output buffer.
 *
 *	The output buffer is automatically advanced.
 *
 *	Parameters:
 *		pptr		Pointer to the output buffer.
 *		c			The character to write.
 */
void MQTT_writeChar(uint8_t ** pptr, char c);

/*
 *	Reads one integer (16-bit) from the input buffer.
 *
 *	The input buffer is automatically advanced.
 *
 *	Parameters:
 *		pptr		Pointer to the input buffer.
 *
 *	Returns the integer read.
 */
int MQTT_readInt(uint8_t ** pptr);

/*
 *	Writes one integer (16-bit) to the output buffer.
 *
 *	The output buffer is automatically advanced.
 *
 *	Parameters:
 *		pptr		Pointer to the output buffer.
 *		i			The integer to write.
 */
void MQTT_writeInt(uint8_t ** pptr, int i);

/*
 *	Reads an MQTT string from the input buffer.
 *
 *	The input buffer is automatically advanced.
 *
 *	Parameters:
 *		string		String to store the read data.
 *		pptr		Pointer to the input buffer.
 *		end			Pointer to the end of the data.
 *
 *	Returns the size of the read string.
 */
int MQTT_readString(char ** string, uint8_t ** pptr, const uint8_t * end);

/*
 *	Writes an MQTT string to the output buffer.
 *
 *	The output buffer is automatically advanced.
 *
 *	Parameters:
 *		pptr		Pointer to the output buffer.
 *		string		The string to write.
 */
void MQTT_writeString(uint8_t ** pptr, const char * string);


#endif


/*******************************************************************************
 *
 *	MQTT helpers.
 *
 *	File:	mqtt_br_helpers.h
 *  Author:	Fotis Panagiotopoulos
 *  Date:	09/09/2022
 *
 *
 ******************************************************************************/

#ifndef MQTT_BR_HELPERS_H_
#define MQTT_BR_HELPERS_H_

#include <stdint.h>
#include <nuttx/config.h>
#include <sys/types.h>

#ifdef CONFIG_MQTT_BROKER


/*
 *	Encodes the message length according to the MQTT algorithm.
 *
 *	Parameters:
 *		buf			Buffer to store the result.
 *		length		The length of the buffer.
 *
 *	Returns the number of bytes written to the buffer.
 */
int MQTT_br_encodeSize(uint8_t * buf, int length);

/*
 *	Decodes the message length according to the MQTT algorithm.
 *
 *	Parameters:
 *		buf			Buffer containing the received message.
 *		value		The length of the message, as decoded.
 *
 *	Returns the number of bytes processed from the buffer.
 */
int MQTT_br_decodeSize(uint8_t * buf, int * value);

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
char MQTT_br_readChar(uint8_t ** pptr);

/*
 *	Writes one character to the output buffer.
 *
 *	The output buffer is automatically advanced.
 *
 *	Parameters:
 *		pptr		Pointer to the output buffer.
 *		c			The character to write.
 */
void MQTT_br_writeChar(uint8_t ** pptr, char c);

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
int MQTT_br_readInt(uint8_t ** pptr);

/*
 *	Writes one integer (16-bit) to the output buffer.
 *
 *	The output buffer is automatically advanced.
 *
 *	Parameters:
 *		pptr		Pointer to the output buffer.
 *		i			The integer to write.
 */
void MQTT_br_writeInt(uint8_t ** pptr, int i);

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
size_t MQTT_br_readString(char ** string, uint8_t ** pptr, const uint8_t * end);

/*
 *	Writes an MQTT string to the output buffer.
 *
 *	The output buffer is automatically advanced.
 *
 *	Parameters:
 *		pptr		Pointer to the output buffer.
 *		string		The string to write.
 */
void MQTT_br_writeString(uint8_t ** pptr, const char * string);


#endif

#endif


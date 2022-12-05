/*******************************************************************************
 *
 *	JSON parser.
 *
 *	File:	json.h
 *  Author:	Fotis Panagiotopoulos
 *  Date:	22/04/2022
 *
 *  This is a lightweight JSON parser. This parser has been designed with deeply embedded
 *  systems in mind. Thus its design has been adapted to this environment.
 *  These kind of systems, have increasingly more processing power, while the memory
 *  continues to be the bottleneck. This parser provides this trade-off: it makes very
 *  minimal use of the memory (only a few bytes), with no use of the Heap, while it
 *  requires a bit more of CPU time.
 *
 *	To keep memory footprint small, and avoid the use of the heap, an internal scratchpad
 *  is used. Note that this scratchpad must be larger than the biggest string expected
 *  in the document. Also note that the contents of the scratchpad, (and thus of the
 *  strings returned) are valid only right after calling the relevant function. Other
 *  calls may change the contents of the scratchpad.
 *
 *
 ******************************************************************************/

#ifndef JSON_H_
#define JSON_H_

#include <limits.h>
#include <stddef.h>
#include <nuttx/config.h>

/* String scratchpad size. */
#ifndef CONFIG_JSON_SCRATCHPAD_SIZE
#define CONFIG_JSON_SCRATCHPAD_SIZE		0x64
#endif

/* JSON value types. */
typedef enum {
	JSON_OBJECT,
	JSON_STRING,
	JSON_INT,
	JSON_FLOAT,
	JSON_BOOL,
	JSON_ARRAY,
	JSON_NULL
} JSON_Type_t;

/* JSON parse error value. */
#define JSON_ERROR		INT_MAX

/* JSON key / value pair. */
typedef struct {
	struct {
		const char * open;
		const char * close;
	} key;

	struct {
		const char * open;
		const char * close;
	} value;

} JSON_Node_t;

/* JSON object. */
typedef struct {
	const char * start;
	const char * end;
	char scratchpad[CONFIG_JSON_SCRATCHPAD_SIZE];
} JSON_Object_t;


/*
 *	Opens and parses a JSON document.
 *
 *	Parameters:
 *		json		The JSON object.
 *		buffer		The buffer containing the JSON document.
 *		size		The size of the buffer.
 *
 *	Returns 1 if parsed successfully, 0 otherwise.
 */
int JSON_open(JSON_Object_t * json, const void * buffer, size_t size);

/*
 *	Gets the JSON node with the specified name.
 *
 *	Parameters:
 *		json		The JSON object.
 *		name		The name to search for.
 *		node		Pointer to store the found node.
 *
 *	Returns 1 if it succeeds, 0 otherwise.
 */
int JSON_get(JSON_Object_t * json, char * name, JSON_Node_t * node);

/*
 *	Gets the first node of the supplied JSON object.
 *
 *	Parameters:
 *		json		The JSON object.
 *		node		Pointer to store the found node to.
 *
 *	Returns 1 if it succeeds, 0 otherwise.
 */
int JSON_getFirst(JSON_Object_t * json, JSON_Node_t * node);

/*
 *	Gets the next node of the supplied one.
 *
 *	Parameters:
 *		json		The JSON object.
 *		current		The current node.
 *		next		Ppointer to store the found node.
 *
 *	Returns 1 if it succeeds, 0 otherwise.
 */
int JSON_getNext(JSON_Object_t * json, JSON_Node_t * current, JSON_Node_t * next);

/*
 *	Gets the name of the supplied node.
 *
 *	Parameters:
 *		json		The JSON object.
 *		node		The node to get the name of.
 *
 *	Returns the name of the node, or NULL in case of an error.
 */
char * JSON_getName(JSON_Object_t * json, JSON_Node_t * node);

/*
 *	Gets the type of the supplied node.
 *
 *	Parameters:
 *		json		The JSON object.
 *		node		The node to get the type of.
 *
 *	Returns the type of the node.
 */
JSON_Type_t JSON_getType(JSON_Object_t * json, JSON_Node_t * node);

/*
 *	Gets the value of the supplied object.
 *
 *	Note! The value will be returned as a string,
 *	regardless of its actual type.
 *
 *	Parameters:
 *		json		The JSON object.
 *		node		The node to get the value of.
 *
 *	Returns the value of the node.
 */
char * JSON_getValue(JSON_Object_t * json, JSON_Node_t * node);

/*
 *	Gets the object value of a node.
 *
 *	Parameters:
 *		json		The JSON object.
 *		node		The ndoe to get the value of.
 *		object		Pointer to store the found object.
 *
 *	Returns 1 if it succeeds, 0 if the value is not an object.
 */
int JSON_getObject(JSON_Object_t * json, JSON_Node_t * node, JSON_Object_t * object);

/*
 *	Performs a formatted read from the node's value.
 *
 *	Parameters:
 *		json		The JSON object.
 *		node		The node to read the value of.
 *		format, ...	Read formatter.
 *
 *	Returns the nomber of arguments parsed.
 */
int JSON_scanf(JSON_Object_t * json, JSON_Node_t * node, const char * format,  ...);

/*
 *	Gets the string value of a node.
 *
 *	Parameters:
 *		json		The JSON object.
 *		node		The node to get the value of.
 *
 *	Returns the string value, or NULL if the value is not a string.
 */
char * JSON_getString(JSON_Object_t * json, JSON_Node_t * node);

/*
 *	Gets the integer value of a node.
 *
 *	Parameters:
 *		json		The JSON object.
 *		node		The node to get the value of.
 *	Returns the integer value, or 0xFFFFFFFF if the value is not an integer.
 */
int JSON_getInt(JSON_Object_t * json, JSON_Node_t * node);

/*
 *	Gets the float value of a node.
 *
 *	Parameters:
 *		json		The JSON object.
 *		node		The node to get the value of.
 *
 *	Returns the float value, or 0xFFFFFFFF if the value is not a float.
 */
double JSON_getFloat(JSON_Object_t * json, JSON_Node_t * node);

/*
 *	Gets the boolean value of a node.
 *
 *	Parameters:
 *		json		The JSON object.
 *		node		The node to get the value of.
 *
 *	Returns the boolean value, or 0xFFFFFFFF if the value is not a boolean.
 */
int JSON_getBoolean(JSON_Object_t * json, JSON_Node_t * node);

/*
 *	Gets the array size of a node.
 *
 *	Parameters:
 *		json		The JSON object.
 *		node		The node to get the array size of.
 *
 *	Returns the array size, or -1 if the value is not an array.
 */
int JSON_array_getSize(JSON_Object_t * json, JSON_Node_t * node);

/*
 *	Returns the first element of an array.
 *
 *	Parameters:
 *		json		The JSON object.
 *		array		The array node.
 *		element		Pointer to store the array element.
 *
 *	Returns 1 if it succeeds, 0 otherwise.
 */
int JSON_array_first(JSON_Object_t * json, JSON_Node_t * array, JSON_Node_t * element);

/*
 *	Returns the element of an array at the specified position.
 *
 *	Parameters:
 *		json		The JSON object.
 *		array		The array node.
 *		pos			The position within the array.
 *		element		Pointer to store the array element.
 *
 *	Returns 1 if it succeeds, 0 otherwise.
 */
int JSON_array_at(JSON_Object_t * json, JSON_Node_t * array, int pos, JSON_Node_t * element);

/*
 *	Returns the next element of an array.
 *
 *	Parameters:
 *		json		The JSON object.
 *		array		The array node.
 *		current		The current node.
 *		next		Pointer to store the next array element.
 *
 *	Returns 1 if it succeeds, 0 otherwise.
 */
int JSON_array_next(JSON_Object_t * json, JSON_Node_t * array, JSON_Node_t * current, JSON_Node_t * next);


#endif


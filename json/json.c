/*******************************************************************************
 *
 *	JSON parser.
 *
 *	File:	json.c
 *  Author:	Fotis Panagiotopoulos
 *  Date:	22/04/2022
 *
 *
 ******************************************************************************/

#include "json.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include <limits.h>
#include <stddef.h>
#include <assert.h>
#include <sys/types.h>

static int parse_object(const char * start, const char * stop, const char ** open, const char ** close);
static int parse_node(const char * start, const char * stop, JSON_Node_t * node);
static int parse_array(const char * start, const char * stop, const char ** open, const char ** close);
static int parse_array_element(const char * start, const char * stop, const char ** open, const char ** close);
static int parse_string(const char * start, const char * stop, const char ** open, const char ** close);


int JSON_open(JSON_Object_t * json, const void * buffer, size_t size)
{
	DEBUGASSERT(json);
	DEBUGASSERT(buffer);

	return parse_object(buffer, ((char*)buffer) + size, &json->start, &json->end);
}

int JSON_get(JSON_Object_t * json, char * name, JSON_Node_t * node)
{
	if (!JSON_getFirst(json, node))
		goto error;

	do {
		JSON_getName(json, node);
		if (strcmp(name, json->scratchpad) == 0)
			return 1;

	} while (JSON_getNext(json, node, node));


error:
	memset(node, 0, sizeof(JSON_Node_t));

	return 0;
}

int JSON_getFirst(JSON_Object_t * json, JSON_Node_t * node)
{
	DEBUGASSERT(json && json->start && json->end);
	DEBUGASSERT(node);

	return parse_node(json->start, json->end, node);
}

int JSON_getNext(JSON_Object_t * json, JSON_Node_t * current, JSON_Node_t * next)
{
	DEBUGASSERT(json && json->start && json->end);
	DEBUGASSERT(current && current->key.open && current->value.open);
	DEBUGASSERT(next);

	if (current->value.close >= json->end)
		return 0;

	return parse_node(current->value.close, json->end, next);
}

char * JSON_getName(JSON_Object_t * json, JSON_Node_t * node)
{
	DEBUGASSERT(json && json->start && json->end);
	DEBUGASSERT(node && node->key.open && node->value.open);

	DEBUGASSERT(*node->key.open == '\"');
	DEBUGASSERT(*(node->key.close - 1) == '\"');

	int length = (node->key.close - 1) - (node->key.open + 1);

	if (length > CONFIG_JSON_SCRATCHPAD_SIZE - 1)
		length = CONFIG_JSON_SCRATCHPAD_SIZE - 1;

	//Get the node name.
	memcpy(json->scratchpad, node->key.open + 1, length);
	json->scratchpad[length] = '\0';

	return json->scratchpad;
}

JSON_Type_t JSON_getType(JSON_Object_t * json, JSON_Node_t * node)
{
	(void)json;

	if (*node->value.open == '\"')
	{
		return JSON_STRING;
	}
	else if (*node->value.open == '[')
	{
		return JSON_ARRAY;
	}
	else if (*node->value.open == '{')
	{
		return JSON_OBJECT;
	}
	else if ((memcmp(node->value.open, "true", 4) == 0) || (memcmp(node->value.open, "false", 5) == 0))
	{
		return JSON_BOOL;
	}
	else if ((memcmp(node->value.open, "null", 4) == 0) || (node->value.open == node->value.close))
	{
		return JSON_NULL;
	}
	else if (isdigit(*node->value.open) || (*node->value.open == '-'))
	{
		if (memchr(node->value.open, '.', (node->value.close - node->value.open)))
		{
			return JSON_FLOAT;
		}
		else
		{
			return JSON_INT;
		}
	}

	return JSON_NULL;
}

char * JSON_getValue(JSON_Object_t * json, JSON_Node_t * node)
{
	DEBUGASSERT(json && json->start && json->end);
	DEBUGASSERT(node && node->key.open && node->value.open);

	int length = node->value.close - node->value.open;

	if (length > CONFIG_JSON_SCRATCHPAD_SIZE - 1)
		length = CONFIG_JSON_SCRATCHPAD_SIZE - 1;

	//Get the node name.
	memcpy(json->scratchpad, node->value.open, length);
	json->scratchpad[length] = '\0';

	return json->scratchpad;
}

int JSON_getObject(JSON_Object_t * json, JSON_Node_t * node, JSON_Object_t * object)
{
	DEBUGASSERT(json && json->start && json->end);
	DEBUGASSERT(node && node->key.open && node->value.open);
	DEBUGASSERT(object);

	return parse_object(node->value.open, node->value.close, &object->start, &object->end);
}

int JSON_scanf(JSON_Object_t * json, JSON_Node_t * node, const char * format,  ...)
{
	va_list arguments;
	va_start(arguments, format);

	//Value is stored in the scratchpad.
	JSON_getValue(json, node);

	int items = vsscanf(json->scratchpad, format, arguments);

	va_end(arguments);

	return items;
}

char * JSON_getString(JSON_Object_t * json, JSON_Node_t * node)
{
	DEBUGASSERT(json && json->start && json->end);
	DEBUGASSERT(node && node->key.open && node->value.open);

	if ((*node->value.open != '\"') || (*(node->value.close - 1) != '\"'))
		return NULL;

	int length = (node->value.close - 1) - (node->value.open + 1);

	if (length > CONFIG_JSON_SCRATCHPAD_SIZE - 1)
		length = CONFIG_JSON_SCRATCHPAD_SIZE - 1;

	//Get the node name.
	memcpy(json->scratchpad, node->value.open + 1, length);
	json->scratchpad[length] = '\0';

	return json->scratchpad;
}

int JSON_getInt(JSON_Object_t * json, JSON_Node_t * node)
{
	DEBUGASSERT(json && json->start && json->end);
	DEBUGASSERT(node && node->key.open && node->value.open);

	int i = JSON_ERROR;
	if (sscanf(node->value.open, "%d", &i) != 1)
		return JSON_ERROR;

	return i;
}

double JSON_getFloat(JSON_Object_t * json, JSON_Node_t * node)
{
	DEBUGASSERT(json && json->start && json->end);
	DEBUGASSERT(node && node->key.open && node->value.open);

	double f = JSON_ERROR;
	if (sscanf(node->value.open, "%lf", &f) != 1)
		return JSON_ERROR;

	return f;
}

int JSON_getBoolean(JSON_Object_t * json, JSON_Node_t * node)
{
	DEBUGASSERT(json && json->start && json->end);
	DEBUGASSERT(node && node->key.open && node->value.open);

	if (memcmp(node->value.open, "true", 4) == 0)
		return 1;
	else if (memcmp(node->value.open, "false", 4) == 0)
		return 0;
	else
		return JSON_ERROR;
}

int JSON_array_getSize(JSON_Object_t * json, JSON_Node_t * node)
{
	DEBUGASSERT(json && json->start && json->end);
	DEBUGASSERT(node && node->key.open && node->value.open);

	if (*node->value.open != '[')
		return 0;

	int size = 0;

	int within_quotes = 0;
	int within_object = 0;
	int nesting = 0;

	const char * p = node->value.open + 1;
	while (p < node->value.close)
	{
		//Escaping.
		if (*p == '\\' && within_quotes)
		{
			p += 2;
			continue;
		}

		//Quotes.
		if (*p == '\"')
			within_quotes ^= 1;

		//Objects.
		if (*p == '{')
			within_object++;

		if (*p == '}')
			within_object = (within_object > 0) ? (within_object - 1) : 0;

		//Value boundaries.
		if (!within_quotes && !within_object)
		{
			if (*p == '[')
				nesting++;

			if (*p == ']')
			{
				if (nesting == 0)
				{
					break;
				}
				else
				{
					nesting--;
				}
			}

			if (*p == ',' && nesting == 0)
				size++;
		}

		p++;
	}

	p = node->value.open + 1;
	while (p <= node->value.close)
	{
		if ((*p == ',') || (*p == ']'))
			break;

		if ((*p != ' ') && (*p != '\t') && (*p != '\n') && (*p != '\r'))
		{
			size++;
			break;
		}

		p++;
	}

	return size;
}

int JSON_array_first(JSON_Object_t * json, JSON_Node_t * array, JSON_Node_t * element)
{
	DEBUGASSERT(json && json->start && json->end);
	DEBUGASSERT(array && array->key.open && array->value.open);
	DEBUGASSERT(element);

	element->key.open = array->key.open;
	element->key.close = array->key.close;

	const char * start = array->value.open;
	if (*start != '[')
		goto error;

	start++;

	if (!parse_array_element(start, array->value.close, &element->value.open, &element->value.close))
		goto error;

	return 1;

error:
	memset(element, 0, sizeof(JSON_Node_t));

	return 0;
}

int JSON_array_at(JSON_Object_t * json, JSON_Node_t * array, int pos, JSON_Node_t * element)
{
	DEBUGASSERT(json && json->start && json->end);
	DEBUGASSERT(array && array->key.open && array->value.open);
	DEBUGASSERT(element);

	element->key.open = array->key.open;
	element->key.close = array->key.close;

	if (*array->value.open != '[')
		goto error;

	int idx = 0;

	int within_quotes = 0;
	int within_object = 0;
	int nesting = 0;

	const char * p = array->value.open + 1;
	while (p < array->value.close)
	{
		//Select this value.
		if (idx == pos)
		{
			if (!parse_array_element(p, array->value.close, &element->value.open, &element->value.close))
				goto error;

			return 1;
		}

		//Escaping.
		if ((*p == '\\') && within_quotes)
		{
			p += 2;
			continue;
		}

		//Quotes.
		if (*p == '\"')
			within_quotes ^= 1;

		//Objects.
		if (*p == '{')
			within_object++;

		if (*p == '}')
			within_object = (within_object > 0) ? (within_object - 1) : 0;

		//Value boundaries.
		if (!within_quotes && !within_object)
		{
			if (*p == '[')
				nesting++;

			if (*p == ']')
			{
				if (nesting == 0)
				{
					goto error;
				}
				else
				{
					nesting--;
				}
			}

			if (*p == ',' && nesting == 0)
				idx++;
		}

		p++;
	}

error:
	memset(element, 0, sizeof(JSON_Node_t));

	return 0;
}

int JSON_array_next(JSON_Object_t * json, JSON_Node_t * array, JSON_Node_t * current, JSON_Node_t * next)
{
	DEBUGASSERT(json && json->start && json->end);
	DEBUGASSERT(array && array->key.open && array->value.open);
	DEBUGASSERT(current && current->key.open && current->value.open);
	DEBUGASSERT(next);

	next->key.open = array->key.open;
	next->key.close = array->key.close;

	const char * start = current->value.close;
	if (*start != ',')
		goto error;

	start++;

	if (!parse_array_element(start, array->value.close, &next->value.open, &next->value.close))
		goto error;

	return 1;

error:
	memset(next, 0, sizeof(JSON_Node_t));

	return 0;
}


int parse_object(const char * start, const char * stop, const char ** open, const char ** close)
{
	*open = memchr(start, '{', stop - start);
	if (!*open)
		goto error;

	const char * p = (*open) + 1;

	int within_quotes = 0;
	int nesting = 0;
	while (p < stop)
	{
		//Escaping.
		if (*p == '\\' && within_quotes)
		{
			p += 2;
			continue;
		}

		//Quotes.
		if (*p == '\"')
			within_quotes ^= 1;

		//Object boundaries.
		if (!within_quotes)
		{
			if (*p == '{')
				nesting++;

			if (*p == '}')
			{
				if (nesting == 0)
				{
					*close = p + 1;
					return 1;
				}
				else
				{
					nesting--;
				}
			}
		}

		p++;
	}

error:
	*open = NULL;
	*close = NULL;

	return 0;
}

int parse_node(const char * start, const char * stop, JSON_Node_t * node)
{
	//Prepare the node.
	memset(node, 0, sizeof(JSON_Node_t));

	//Parse the key.
	if (!parse_string(start, stop, &node->key.open, &node->key.close))
		goto error;

	//Parse the colon.
	char * colon = memchr(node->key.close, ':', stop - node->key.close);
	if (!colon)
		goto error;

	//Parse the value.
	char * value = colon + 1;

	while ((*value == ' ') || (*value == '\t') || (*value == '\n') || (*value == '\r'))
	{
		value++;

		if (value >= stop)
			goto error;
	}

	switch (*value)
	{
		//String
		case '\"':
		{
			if (!parse_string(value, stop, &node->value.open, &node->value.close))
				goto error;

			break;
		}

		//Array
		case '[':
		{
			if (!parse_array(value, stop, &node->value.open, &node->value.close))
				goto error;

			break;

		}

		//Object
		case '{':
		{
			if (!parse_object(value, stop, &node->value.open, &node->value.close))
				goto error;

			break;
		}

		//Everything else
		default:
		{
			node->value.open = value;
			node->value.close = memchr(value, ',', stop - value);

			if (!node->value.close)
				node->value.close = memchr(value, '}', (stop - value) + 1);

			if (!node->value.close)
				goto error;

			break;
		}
	}

	return 1;

error:
	memset(node, 0, sizeof(JSON_Node_t));

	return 0;
}

int parse_array(const char * start, const char * stop, const char ** open, const char ** close)
{
	*open = memchr(start, '[', stop - start);
	if (!*open)
		goto error;

	const char * p = (*open) + 1;

	int within_quotes = 0;
	int nesting = 0;
	while (p < stop)
	{
		//Escaping.
		if ((*p == '\\') && within_quotes)
		{
			p += 2;
			continue;
		}

		//Quotes.
		if (*p == '\"')
		{
			within_quotes ^= 1;
			p++;
			continue;
		}

		//Array boundaries.
		if (!within_quotes)
		{
			if (*p == '[')
			{
				nesting++;
			}
			else if (*p == ']')
			{
				if (nesting == 0)
				{
					*close = p + 1;
					return 1;
				}
				else
				{
					nesting--;
				}
			}
		}

		p++;
	}

error:
	*open = NULL;
	*close = NULL;

	return 0;
}

int parse_array_element(const char * start, const char * stop, const char ** open, const char ** close)
{
	const char * value = start;

	while ((*value == ' ') || (*value == '\t') || (*value == '\n') || (*value == '\r') || (*value == ','))
	{
		value++;

		if (value >= stop)
			goto error;
	}

	switch (*value)
	{
		//String
		case '\"':
		{
			if (!parse_string(value, stop, open, close))
				goto error;

			break;
		}

		//Array
		case '[':
		{
			if (!parse_array(value, stop, open, close))
				goto error;

			break;
		}

		//Object
		case '{':
		{
			if (!parse_object(value, stop, open, close))
				goto error;

			break;
		}

		//Everything else
		default:
		{
			*open = value;
			*close = memchr(value, ',', stop - value);

			if (!*close)
				*close = memchr(value, ']', (stop - value) + 1);

			if (!*close)
				goto error;

			break;
		}
	}

	return 1;

error:
	*open = NULL;
	*close = NULL;

	return 0;
}

int parse_string(const char * start, const char * stop, const char ** open, const char ** close)
{
	*open = memchr(start, '\"', stop - start);
	if (!*open)
		goto error;

	const char * p = *open + 1;
	while (p < stop)
	{
		if (*p == '\\')
		{
			p += 2;
			continue;
		}

		if (*p == '\"')
		{
			*close = p + 1;
			return 1;
		}

		p++;
	}

error:
	*open = NULL;
	*close = NULL;

	return 0;
}


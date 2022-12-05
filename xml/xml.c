/*******************************************************************************
 *
 *	XML parser.
 *
 *	File:	xml.c
 *  Author:	Fotis Panagiotopoulos
 *  Date:	22/04/2022
 *
 *
 ******************************************************************************/

#include "xml.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stddef.h>
#include <ctype.h>
#include <assert.h>
#include <sys/types.h>

#define XML_tag_isOpen(tag)		(*((tag)->start + 1) != '/')
#define XML_tag_isClosed(tag)	(*((tag)->start + 1) == '/')

static int parseProlog(XML_Doc_t * doc, const void * buffer, size_t size);
static int parseNode(XML_Node_t * node, const void * buffer, size_t size);
static int parseTag(XML_Tag_t * tag, const void * buffer, size_t size);
static int compareTags(XML_Tag_t * a, XML_Tag_t * b);


int XML_open(XML_Doc_t * doc, const void * buffer, size_t size)
{
	DEBUGASSERT(doc);
	DEBUGASSERT(buffer);

	doc->scratchpad[0] = '\0';

	//Parse the prolog/
	if (!parseProlog(doc, buffer, size))
		return 0;

	//The document always follows the prolog.
	unsigned prolog_offset = 0;
	if (doc->prolog.open.start)
		prolog_offset = doc->prolog.open.end - (char*)buffer;

	if (prolog_offset >= size)
		return 0;

	//Parse the root node.
	if (parseNode(&doc->root, ((char*)buffer) + prolog_offset, size - prolog_offset) == 0)
		return 0;

	return 1;
}

int XML_openPart(XML_Doc_t * doc, XML_Node_t * node)
{
	DEBUGASSERT(doc);
	DEBUGASSERT(node);
	DEBUGASSERT((node->open.start != NULL) && (node->open.end != NULL)
				&& (node->close.start != NULL) && (node->close.end != NULL));

	doc->scratchpad[0] = '\0';

	//This is a part of a document. There cannot be a prolog.
	memset(&doc->prolog, 0, sizeof(XML_Node_t));

	//Parse the root node.
	if (parseNode(&doc->root, node->open.start, ((node->close.end + 1) - node->open.start)) == 0)
		return 0;

	return 1;
}

char * XML_doc_getProlog(XML_Doc_t * doc)
{
	DEBUGASSERT(doc && XML_node_isValid(doc, &doc->root));

	if (doc->prolog.open.start)
	{
		const char * start = doc->prolog.open.start + 2;
		int length = doc->prolog.open.end - start - 1;

		DEBUGASSERT(length > 0);

		if (length > CONFIG_XML_SCRATCHPAD_SIZE - 1)
			length = CONFIG_XML_SCRATCHPAD_SIZE - 1;

		//Get the node name.
		memcpy(doc->scratchpad, start, length);
		doc->scratchpad[length] = '\0';
	}
	else
	{
		doc->scratchpad[0] = '\0';
	}

	return doc->scratchpad;
}

int XML_node_find(XML_Doc_t * doc, XML_Node_t * parent, const char * name, XML_Node_t * node)
{
	DEBUGASSERT(doc && XML_node_isValid(doc, &doc->root));
	DEBUGASSERT(name);
	DEBUGASSERT(node);

	//If no parent is specified, the root is considered the parent.
	if (parent == NULL)
		parent = &doc->root;

	//Check if it is self-closing.
	if (parent->open.start == parent->close.start)
		return 0;

	//Needed if parent and node are pointing to the same struct.
	const char * parent_end = parent->close.start;

	memcpy(node, parent, sizeof(XML_Node_t));

	//Search all tags of the document.
	while (parseNode(node, node->open.end, (parent_end - node->open.end)))
	{
		const char * start = node->open.start + 1;

		//Remove the attribute, if it exists.
		unsigned length = node->open.end - start;
		char * sp_p = memchr(start, ' ', length);

		if (sp_p)
			length = sp_p - start;

		//Check if it matches.
		if (strlen(name) == length)
		{
			if (memcmp(start, name, length) == 0)
				return 1;
		}
	}

	return 0;
}

int XML_node_getFirst(XML_Doc_t * doc, XML_Node_t * parent, XML_Node_t * node)
{
	DEBUGASSERT(doc && XML_node_isValid(doc, &doc->root));
	DEBUGASSERT(node);

	//If no parent is specified, the root is considered the parent.
	if (parent == NULL)
		parent = &doc->root;

	//Check if it is self-closing.
	if (parent->open.start == parent->close.start)
		return 0;

	//Get the first found node.
	return parseNode(node, parent->open.end, parent->close.start - parent->open.end);
}

int XML_node_getNext(XML_Doc_t * doc, XML_Node_t * parent, XML_Node_t * current, XML_Node_t * node)
{
	DEBUGASSERT(doc && XML_node_isValid(doc, &doc->root));
	DEBUGASSERT(node);

	//If no parent is specified, the root is considered the parent.
	if (parent == NULL)
		parent = &doc->root;

	//Check if it is self-closing.
	if (parent->open.start == parent->close.start)
		return 0;

	//The current node cannot be the parent of itself.
	DEBUGASSERT(parent != current);

	//If no current is specified, the first will be returned.
	if (current == NULL)
	{
		//Use node as temp storage.
		node->close.end = parent->open.end;
		current = node;
	}

	DEBUGASSERT(current->close.end <= parent->close.start);

	//Get the node directly after "current".
	return parseNode(node, current->close.end, parent->close.start - current->close.end);
}

int XML_node_getAt(XML_Doc_t * doc, XML_Node_t * parent, unsigned int pos, XML_Node_t * node)
{
	DEBUGASSERT(doc && XML_node_isValid(doc, &doc->root));
	DEBUGASSERT(node);

	//If no parent is specified, the root is considered the parent.
	if (parent == NULL)
		parent = &doc->root;

	//Check if it is self-closing.
	if (parent->open.start == parent->close.start)
		return 0;

	unsigned index = 0;
	XML_Node_t n;
	n.close.end = parent->open.end;

	//Scan all children, till the specified position.
	while (parseNode(&n, n.close.end, parent->close.start - n.close.end))
	{
		if (index == pos)
			break;

		index++;
	}

	memcpy(node, &n, sizeof(XML_Node_t));

	//Check if the node is valid. The above loop may have failed.
	return XML_node_isValid(doc, node);
}

int XML_node_getLast(XML_Doc_t * doc, XML_Node_t * parent, XML_Node_t * node)
{
	DEBUGASSERT(doc && XML_node_isValid(doc, &doc->root));
	DEBUGASSERT(node);

	//If no parent is specified, the root is considered the parent.
	if (parent == NULL)
		parent = &doc->root;

	//Check if it is self-closing.
	if (parent->open.start == parent->close.start)
		return 0;

	//Needed if parent and node are pointing to the same struct.
	const char * parent_end = parent->close.start;

	XML_Node_t n;
	n.close.end = parent->open.end;

	//Search for all nodes. Every new one found is considered last, till the next one is found.
	while (parseNode(&n, n.close.end, parent_end - n.close.end))
	{
		memcpy(node, &n, sizeof(XML_Node_t));
	}

	//Check if the node is valid. There may not be any children nodes on this parent.
	return XML_node_isValid(doc, node);
}

const char * XML_node_getName(XML_Doc_t * doc, XML_Node_t * node)
{
	DEBUGASSERT(doc && XML_node_isValid(doc, &doc->root));
	DEBUGASSERT(node && XML_node_isValid(doc, node));

	const char * start = (node->open.start + 1);

	//Remove the attribute, if it exists.
	int length = node->open.end - start;
	char * sp_p = memchr(start, ' ', length);

	if (sp_p)
		length = sp_p - start;

	DEBUGASSERT(length > 0);

	if (length > CONFIG_XML_SCRATCHPAD_SIZE - 1)
		length = CONFIG_XML_SCRATCHPAD_SIZE - 1;

	//Get the node name.
	memcpy(doc->scratchpad, start, length);
	doc->scratchpad[length] = '\0';

	return doc->scratchpad;
}

const char * XML_node_getAttributes(XML_Doc_t * doc, XML_Node_t * node)
{
	DEBUGASSERT(doc && XML_node_isValid(doc, &doc->root));
	DEBUGASSERT(node && XML_node_isValid(doc, node));

	const char * start = (node->open.start + 1);

	//Remove the attribute, if it exists.
	int length = node->open.end - start;
	char * sp_p = memchr(start, ' ', length);
	if (sp_p)
	{
		start = sp_p + 1;
		length = node->open.end - start;

		//If this a self-closing tag, it has to be trimmed accordingly.
		if (*(node->open.end - 1) == '/')
			length--;

		DEBUGASSERT(length > 0);

		if (length > CONFIG_XML_SCRATCHPAD_SIZE - 1)
			length = CONFIG_XML_SCRATCHPAD_SIZE - 1;

		//Get the attributes.
		memcpy(doc->scratchpad, start, length);
		doc->scratchpad[length] = '\0';
	}
	else
	{
		doc->scratchpad[0] = '\0';
	}

	return doc->scratchpad;
}

int XML_node_attributesScanf(XML_Doc_t * doc, XML_Node_t * node, const char * format,  ...)
{
	va_list arguments;
	va_start(arguments, format);

	//The attributes are stored to the scratchpad.
	XML_node_getAttributes(doc, node);

	int items = vsscanf(doc->scratchpad, format, arguments);

	va_end(arguments);

	return items;
}

char * XML_node_getContent(XML_Doc_t * doc, XML_Node_t * node)
{
	DEBUGASSERT(doc && XML_node_isValid(doc, &doc->root));
	DEBUGASSERT(node && XML_node_isValid(doc, node));

	//Check if it is self-closing.
	if (node->open.start == node->close.start)
	{
		doc->scratchpad[0] = '\0';
		return doc->scratchpad;
	}

	//If has children, then it does not have content.
	//Only text is considered content.
	XML_Node_t child;
	if (parseNode(&child, node->open.end, node->close.start - node->open.end))
	{
		doc->scratchpad[0] = '\0';
		return doc->scratchpad;
	}

	const char * start = (node->open.end + 1);
	int length = node->close.start - start;

	DEBUGASSERT(length >= 0);

	//Trim any leading formatting.
	while (((*start == '\n') || (*start == '\r') || (*start == ' ') || (*start == '\t')) && length)
	{
		start++;
		length--;
	}

	//Trim any trailing formatting.
	const char * end = start + length - 1;
	while (((*end == '\n') || (*end == '\r') || (*end == ' ') || (*end == '\t')) && length)
	{
		end--;
		length--;
	}

	if (length > CONFIG_XML_SCRATCHPAD_SIZE - 1)
		length = CONFIG_XML_SCRATCHPAD_SIZE - 1;

	//Get the content.
	memcpy(doc->scratchpad, start, length);
	doc->scratchpad[length] = '\0';

	return doc->scratchpad;
}

int XML_node_contentScanf(XML_Doc_t * doc, XML_Node_t * node, const char * format,  ...)
{
	va_list arguments;
	va_start(arguments, format);

	//The content is stored to the scratchpad.
	XML_node_getContent(doc, node);

	int items = vsscanf(doc->scratchpad, format, arguments);

	va_end(arguments);

	return items;
}

int XML_node_isValid(XML_Doc_t * doc, XML_Node_t * node)
{
	(void)doc;

	//To be valid, all tag pointer should be also valid.
	//Note this does NOT apply to the prolog node!
	return (node->open.start != NULL) && (node->open.end != NULL) && (node->close.start != NULL) && (node->close.end != NULL);
}

int XML_node_isEmpty(XML_Doc_t * doc, XML_Node_t * node)
{
	//An empty node has neither children, nor content.

	if (XML_node_hasChildren(doc, node))
		return 0;

	//Content is stored in the scratchpad.
	XML_node_getContent(doc, node);
	return (strlen(doc->scratchpad) == 0);
}

int XML_node_getParent(XML_Doc_t * doc, XML_Node_t * node, XML_Node_t * parent)
{
	DEBUGASSERT(doc && XML_node_isValid(doc, &doc->root));
	DEBUGASSERT(node && XML_node_isValid(doc, node));
	DEBUGASSERT(parent);

	XML_Node_t p;
	memset(&p, 0, sizeof(XML_Node_t));

	XML_Node_t n;
	n.open.end = doc->root.open.start;

	//Look for any node that encloses the provided one.
	while (parseNode(&n, n.open.end, (doc->root.close.end - n.open.end)))
	{
		if (n.open.end < node->open.start && n.close.start > node->close.end)
			memcpy(&p, &n, sizeof(XML_Node_t));

		if (n.open.start >= node->open.start)
			break;
	}

	memcpy(parent, &p, sizeof(XML_Node_t));

	//Ensure that the node is valid. The provided node maybe is the root.
	return XML_node_isValid(doc, parent);
}

int XML_node_hasSiblings(XML_Doc_t * doc, XML_Node_t * node)
{
	//The siblings are the children of the parent.

	XML_Node_t parent;
	if (XML_node_getParent(doc, node, &parent))
	{
		return XML_node_hasChildren(doc, &parent) - 1;
	}

	return 0;
}

int XML_node_hasChildren(XML_Doc_t * doc, XML_Node_t * node)
{
	DEBUGASSERT(doc && XML_node_isValid(doc, &doc->root));

	//Check if it is self-closing.
	if (node && (node->open.start == node->close.start))
		return 0;

	int children = 0;

	//If no parent is specified, the root is considered the parent.
	if (node == NULL)
		node = &doc->root;

	XML_Node_t child;
	child.close.end = node->open.end;

	//Just count all the children.
	while (parseNode(&child, child.close.end, node->close.start - child.close.end))
		children++;

	return children;
}


int parseProlog(XML_Doc_t * doc, const void * buffer, size_t size)
{
	doc->prolog.open.start = memchr(buffer, '<', size);
	if (!doc->prolog.open.start)
		goto parse_fail;

	if (*(doc->prolog.open.start + 1) != '?')
		goto no_prolog;

	doc->prolog.open.end = doc->prolog.open.start;

	do {
		doc->prolog.open.end = memchr(doc->prolog.open.end, '>', size - (doc->prolog.open.end - (char*)buffer));
		if (!doc->prolog.open.end)
			goto parse_fail;
	} while (*(doc->prolog.open.end - 1) != '?');

	doc->prolog.close.start = NULL;
	doc->prolog.close.end = NULL;
	return 1;

no_prolog:
	doc->prolog.open.start = NULL;
	doc->prolog.open.end = NULL;
	doc->prolog.close.start = NULL;
	doc->prolog.close.end = NULL;

	return 1;  //Parsing was successful, even without prolog, since there were not errors.

parse_fail:
	return 0;
}

int parseNode(XML_Node_t * node, const void * buffer, size_t size)
{
	node->open.start = buffer;
	node->open.end = buffer;

	//Find the first opening tag.
	do {
		if (!parseTag(&node->open, node->open.end, size - (node->open.end - (char*)buffer)))
			goto parse_fail;

	} while (!XML_tag_isOpen(&node->open));

	node->close.start = node->open.start;
	node->close.end = node->open.end;

	//Check if this is a self-closing tag.
	if (*(node->open.end - 1) == '/')
		return 1;

	//There may be multiple tags with the same name nested.
	int nesting = 0;
	while (parseTag(&node->close, node->close.end, size - (node->close.end - (char*)buffer)))
	{
		if (compareTags(&node->open, &node->close))
		{
			if (XML_tag_isClosed(&node->close))
			{
				if (nesting == 0)
					return 1;

				nesting--;
			}
			else
			{
				nesting++;
			}
		}
	}

parse_fail:
	node->open.start = NULL;
	node->open.end = NULL;
	node->close.start = NULL;
	node->close.end = NULL;

	return 0;
}

int parseTag(XML_Tag_t * tag, const void * buffer, size_t size)
{
	tag->start = memchr(buffer, '<', size);
	if (!tag->start)
		goto parse_fail;

	tag->end = tag->start;

	do {
		tag->end = memchr(tag->end + 1, '>', size - (tag->end - (char*)buffer));
		if (!tag->end)
			goto parse_fail;
	} while (*(tag->start + 1) == '!' && *(tag->end - 1) != '-');  //If this is a comment tag, find its match.

	//Ignore the comment.
	if (*(tag->start + 1) == '!')
		return parseTag(tag, tag->end, size - (tag->end - (char*)buffer));

	//Check the tag name.
	//It must exist, and start from a letter or '_'.
	const char * check_name = tag->start + 1;
	if (*check_name == '/')
		check_name++;

	if ((!isalpha(*check_name)) && (*check_name != '_'))
		goto parse_fail;

	return 1;

parse_fail:
	tag->start = NULL;
	tag->end = NULL;

	return 0;
}

int compareTags(XML_Tag_t * a, XML_Tag_t * b)
{
	const char * a_start = a->start + 1;
	if (*a_start == '/')
		a_start++;

	int a_length = a->end - a_start;
	char * sp_a_p = memchr(a_start, ' ', a_length);

	if (sp_a_p)
		a_length = sp_a_p - a_start;

	const char * b_start = b->start + 1;
	if (*b_start == '/')
		b_start++;

	int b_length = b->end - b_start;
	char * sp_b_p = memchr(b_start, ' ', b_length);

	if (sp_b_p)
		b_length = sp_b_p - b_start;

	if (a_length != b_length)
		return 0;

	if (memcmp(a_start, b_start, a_length) != 0)
		return 0;

	return 1;
}


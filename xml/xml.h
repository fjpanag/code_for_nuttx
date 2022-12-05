/*******************************************************************************
 *
 *	XML parser.
 *
 *	File:	xml.h
 *  Author:	Fotis Panagiotopoulos
 *  Date:	22/04/2022
 *
 *  This is a lightweight, non-validating XML parser. This parser has been designed with
 *  deeply embedded systems in mind. Thus its design has been adapted to this environment.
 *  These kind of systems, have increasingly more processing power, while the memory
 *  continues to be the bottleneck. This parser provides this trade-off: it makes very
 *  minimal use of the memory (only a few bytes), with no use of the Heap, while it
 *  requires a bit more of CPU time.
 *
 *  To keep memory footprint small, and avoid the use of the heap, an internal scratchpad
 *  is used. Note that this scratchpad must be larger than the biggest string expected
 *  in the document. Also note that the contents of the scratchpad, (and thus of the
 *  strings returned) are valid only right after calling the relevant function. Other
 *  calls may change the contents of the scratchpad.
 *
 *  Note that this is a minimal parser, and thus it may not adhere strictly to the
 *  XML standard. Specifically, there are the following limitations:
 *  	* The XML prolog is handled, but not parsed.
 *  	* Namespaces are not supported.
 *  	* Entity references are not handled. The application has to take care of them.
 *  	* Only Linux line-endings are supported. Also only leading and trailing
 *  		new-lines and spaces are truncated.
 *  	* The XML document is not checked whether it is well-formed. Only the necessary
 *  		rules for parsing are checked. This is especially true regarding the proper
 *  		nesting of the elements, and proper quoting of the attributes!
 *
 *
 ******************************************************************************/

#ifndef XML_H_
#define XML_H_

#include <stddef.h>
#include <nuttx/config.h>

/* String scratchpad size. */
#ifndef CONFIG_XML_SCRATCHPAD_SIZE
#define CONFIG_XML_SCRATCHPAD_SIZE		48
#endif

/* XML Tag. */
typedef struct {
	const char * start;
	const char * end;
} XML_Tag_t;

/* XML Node. */
typedef struct {
	XML_Tag_t open;
	XML_Tag_t close;
} XML_Node_t;

/* XML Document. */
typedef struct {
	XML_Node_t prolog;
	XML_Node_t root;
	char scratchpad[CONFIG_XML_SCRATCHPAD_SIZE];
} XML_Doc_t;


/*
 *	Opens an XML document.
 *	The document is tried to be parsed, and the root node is stored.
 *
 *	Parameters:
 *		doc			The document handle.
 *		buffer		The buffer containing the document.
 *		size		The size of the document.
 *
 *	Returns 1 if the document is opened and parsed successfully, 0 otherwise.
 */
int XML_open(XML_Doc_t * doc, const void * buffer, size_t size);

/*
 *	Opens a part of a larger XML document, as a new smaller document.
 *
 *	Parameters
 *		doc			The new document handle.
 *		node		The portion of the old file to open.
 *					The provided node will be the root node of
 *					the new document.
 *
 *	Returns 1 if the document is opened and parsed successfully, 0 otherwise.
 */
int XML_openPart(XML_Doc_t * doc, XML_Node_t * node);

/*
 *	Gets the prolog of an XML document.
 *
 *	Parameters:
 *		doc			The document handle.
 *
 *	Returns a string containing the prolog, or an empty string.
 */
char * XML_doc_getProlog(XML_Doc_t * doc);

/*
 *	Searches for a node of the given name.
 *
 *	Parameters:
 *		doc			The document handle.
 *		parent		The parent node. Only child nodes will be searched.
 *					If NULL, the root will be used as a parent.
 *		name		The name of the node.
 *		node		Pointer to the node struct, to store the result.
 *
 *	Returns 1 if found, 0 otherwise.
 */
int XML_node_find(XML_Doc_t * doc, XML_Node_t * parent, const char * name, XML_Node_t * node);

/*
 *	Gets the first child of the provided node.
 *
 *	Parameters:
 *		doc			The document handle.
 *		parent		The parent node.
 *		node		Pointer to the node struct, to store the result.
 *
 *	Returns 1 if found, 0 otherwise.
 */
int XML_node_getFirst(XML_Doc_t * doc, XML_Node_t * parent, XML_Node_t * node);

/*
 *	Gets the next sibling node.
 *
 *	Parameters:
 *		doc			The document handle.
 *		parent		The parent node. Only child nodes will be searched.
 *		current		The current node. A sibling of this node will be returned.
 *		node		Pointer to the node struct, to store the result.
 *
 *	Returns 1 if found, 0 otherwise.
 */
int XML_node_getNext(XML_Doc_t * doc, XML_Node_t * parent, XML_Node_t * current, XML_Node_t * node);

/*
 *	Gets the child at the specified position of the provided node.
 *
 *	Parameters:
 *		doc			The document handle.
 *		parent		The parent node.
 *		pos			The position (index) of the child.
 *		node		Pointer to the node struct, to store the result.
 *
 *	Returns 1 if found, 0 otherwise.
 */
int XML_node_getAt(XML_Doc_t * doc, XML_Node_t * parent, unsigned int pos, XML_Node_t * node);

/*
 *	Gets the last child of the provided node.
 *
 *	Parameters:
 *		doc			The document handle.
 *		parent		The parent node.
 *		node		Pointer to the node struct, to store the result.
 *
 *	Returns 1 if found, 0 otherwise.
 */
int XML_node_getLast(XML_Doc_t * doc, XML_Node_t * parent, XML_Node_t * node);

/*
 *	Gets the name of the provided node.
 *
 *	Parameters:
 *		doc			The document handle.
 *		node		The node to get the name.
 *
 *	Returns a string containing the node name, or an empty string.
 */
const char * XML_node_getName(XML_Doc_t * doc, XML_Node_t * node);

/**
 * Gets the attributes of the provided node.
 *
 *	Parameters:
 *		doc			The document handle.
 *		node		The node to get the attributes.
 *
 *	Returns a string containing the attributes, or an empty string.
 */
const char * XML_node_getAttributes(XML_Doc_t * doc, XML_Node_t * node);

/*
 *	Reads the formatted attributes of a node.
 *
 *	Parameters:
 *		doc			The document handle.
 *		node		The node to read the attributes from.
 *		format, ...	The format string and the arguments.
 *
 *	Returns the number of arguments parsed.
 */
int XML_node_attributesScanf(XML_Doc_t * doc, XML_Node_t * node, const char * format,  ...);

/*
 *	Gets the content of a node.
 *
 *	Note! If the node contains other nodes, then an empty string
 *	will be returned. Only text is considered actual content.
 *
 *	Parameters:
 *		doc			The document handle.
 *		node		The node to read the content.
 *
 *	Returns a string containing the node's content, or an empty string.
 */
char * XML_node_getContent(XML_Doc_t * doc, XML_Node_t * node);

/*
 *	Reads the formatted content of a node.
 *
 *	Parameters:
 *		doc			The document handle.
 *		node		The node to read the content from.
 *		format, ...	The format string and the arguments.
 *
 *	Returns the number of arguments parsed.
 */
int XML_node_contentScanf(XML_Doc_t * doc, XML_Node_t * node, const char * format,  ...);

/*
 *	Checks whether a node struct is valid.
 *
 *	Parameters:
 *		doc			The document handle.
 *		node		The node to check.
 *
 *	Returns 1 if valid, 0 otherwise.
 */
int XML_node_isValid(XML_Doc_t * doc, XML_Node_t * node);

/*
 *	Checks whether the provided node is an empty one.
 *	Empty is a node without content and without children.
 *
 *	Parameters:
 *		doc			The document handle.
 *		node		The node to check.
 *
 *	Returns 1 if the node is empty, 0 otherwise.
 */
int XML_node_isEmpty(XML_Doc_t * doc, XML_Node_t * node);

/*
 *	Gets the parent of a node.
 *
 *	Parameters:
 *		doc			The document handle.
 *		node		The node to find its parent.
 *		parent		Pointer to the node struct, to store the result.
 *
 *	Returns 1 if a parent is found, 0 otherwise.
 */
int XML_node_getParent(XML_Doc_t * doc, XML_Node_t * node, XML_Node_t * parent);

/*
 *	Checks whether the provided node has any siblings.
 *
 *	Parameters:
 *		doc			The document handle.
 *		node		The node to find its siblings.
 *
 *	Returns the number of siblings that the node has.
 */
int XML_node_hasSiblings(XML_Doc_t * doc, XML_Node_t * node);

/*
 *	Checks whether the provided node has any children.
 *
 *	Note! This function checks only for direct children,
 *	not for grand-children!
 *
 *	Parameters:
 *		doc			The document handle.
 *		node		The node to check.
 *
 *	Returns the number of children that the node has.
 */
int XML_node_hasChildren(XML_Doc_t * doc, XML_Node_t * node);


#endif


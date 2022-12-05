/*******************************************************************************
 *
 *	FTP client.
 *
 *	File:	ftpc.h
 *  Author:	Fotis Panagiotopoulos
 *  Date:	28/06/2022
 *
 *
 ******************************************************************************/

#ifndef FTPC_H_
#define FTPC_H_

#include <netinet/in.h>
#include <stdint.h>
#include <nuttx/config.h>
#include <sys/types.h>

/* FTP client return codes. */
typedef enum {
	FTPC_OK				= 0,    // OK response.
	FTPC_ERR_INTERNAL	= -1,   // Internal (client) error.
	FTPC_ERR_NETWORK	= -2,   // Network error.
	FTPC_ERR_HOSTNAME	= -3,   // Hostname could not be resolved.
	FTPC_ERR_CONNECT	= -4,   // Could not connect.
	FTPC_ERR_SERVER		= -5,   // Server error, return code could not be parsed, return code above 400.
	FTPC_ERR_AUTH		= -6,   // Authorization failed. Wrong user and/or pass.
	FTPC_ERR_CLOSED		= -7,   // Connection closed/timed-out.
	FTPC_ERR_MEMORY		= -8,   // Not enough memory for socket/buffer allocation.
	FTPC_ERR_IO			= -9,   // Local file system I/O error.
	FTPC_SYNTAX_ERROR	= -10,  // Syntax error (reported by the server). The command may be unsupported.
	FTPC_NO_FILE		= -11   // File doesn't exist at the server.
} FTPc_Result_t;

/* FTP client handle. */
typedef struct {
	int connected;
	int cSock;
	int dSock;
	struct sockaddr_in data_addr;
	char buffer[CONFIG_FTPC_BUFFER_SIZE];
} FTPc_t;


/*
 *	Creates a new FTP client.
 *
 *	Returns a new client, or NULL in case of error.
 */
FTPc_t * ftpc_init(void);

/*
 *	Initializes a new FTP client, and connects it to the server.
 *
 *	Parameters:
 *		ftpc			The FTP client handle.
 *		host			The address of the server.
 *		port			The port that the server listens to (default is 21).
 *		username		The connection username. It can be NULL for anonymous connections.
 *		password		The connection password. It can be NULL.
 *
 *	Returns the result of the connection.
 */
FTPc_Result_t ftpc_connect(FTPc_t * ftpc, const char * host, uint16_t port, const char * username, const char * password);

/*
 *	Gets the size of the given file.
 *
 *	Parameters:
 *		ftpc			The FTP client handle.
 *		filename		The name of the file.
 *
 *	Returns the size of the file, or a negated error code.
 */
ssize_t ftpc_filesize(FTPc_t * ftpc, const char * r_file);

/*
 *	Downloads a file from the server.
 *
 *	Parameters:
 *		ftpc			The FTP client handle.
 *		r_file			The remote file name.
 *		l_file			The local file name.
 *
 *	Returns the number of bytes received, or a negated error code.
 */
ssize_t ftpc_retrieve(FTPc_t * ftpc, const char * r_file, const char * l_file);

/*
 *	Uploads a file to the server.
 *
 *	Parameters:
 *		ftpc			The FTP client handle.
 *		r_file			The remote file name.
 *		l_file			The local file name.
 *
 *	Returns the number of bytes sent, or a negated error code.
 */
ssize_t ftpc_store(FTPc_t * ftpc, const char * r_file, const char * l_file);

/*
 *	Uploads a file to the server.
 *	The server assigns a unique name to the file.
 *
 *	Parameters:
 *		ftpc			The FTP client handle.
 *		path			The remote path to upload the file.
 *		l_file			The local file name.
 *
 *	Returns the number of bytes sent, or a negated error code.
 */
ssize_t ftpc_storeUnique(FTPc_t * ftpc, const char * path, const char * l_file);

/*
 *	Renames a file on the server.
 *
 *	Parameters:
 *		ftpc			The FTP client handle.
 *		old				The old file name.
 *		new				The new file name.
 *
 *	Returns the result of the operation.
 */
FTPc_Result_t ftpc_rename(FTPc_t * ftpc, const char * old, const char * new);

/*
 *	Deletes a file from the server.
 *
 *	Parameters:
 *		ftpc			The FTP client handle.
 *		r_file			The remote file to delete.
 *
 *	Returns the result of the operation.
 */
FTPc_Result_t ftpc_delete(FTPc_t * ftpc, const char * r_file);

/*
 *	Creates a directory to the server.
 *
 *	Parameters:
 *		ftpc			The FTP client handle.
 *		path			The directory path.
 *
 *	Returns the result of the operation.
 */
FTPc_Result_t ftpc_createDir(FTPc_t * ftpc, const char * path);

/*
 *	Deletes a directory from the server.
 *
 *	Parameters:
 *		ftpc			The FTP client handle.
 *		path			The directory to delete.
 *
 *	Returns the result of the operation.
 */
FTPc_Result_t ftpc_deleteDir(FTPc_t * ftpc, const char * path);

/*
 *	Closes the session and terminates the FTP client.
 *
 *	Parameters:
 *		ftpc			The FTP client handle.
 *
 *	Returns the result of the operation.
 */
FTPc_Result_t ftpc_quit(FTPc_t * ftpc);


#endif


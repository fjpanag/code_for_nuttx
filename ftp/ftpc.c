/*******************************************************************************
 *
 *	FTP client.
 *
 *	File:	ftpc.c
 *  Author:	Fotis Panagiotopoulos
 *  Date:	28/06/2022
 *
 *
 ******************************************************************************/

#include "ftpc.h"
#include <unistd.h>
#include <fcntl.h>
#include "network.h"
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <time.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <assert.h>
#include <nuttx/config.h>
#include <sys/types.h>

#define FTP_CHECK_CODE(x)	do {                         \
		int code = x;                                    \
		if (code == 550)      return FTPC_NO_FILE;       \
		else if (code == 500) return FTPC_SYNTAX_ERROR;  \
		else if (code < 0)    return FTPC_ERR_CLOSED;    \
		else if (code == 0)   return FTPC_ERR_SERVER;    \
		else if (code >= 400) return FTPC_ERR_SERVER;    \
} while (0)

static int openData(FTPc_t * ftpc);
static int closeData(FTPc_t * ftpc);
static int sendCommand(FTPc_t * ftpc, const char * message);
static size_t receiveCommand(FTPc_t * ftpc, char * buffer, size_t size);
static int returnCode(FTPc_t * ftpc, const char * buffer, size_t size);


FTPc_t * ftpc_init()
{
	FTPc_t * ftpc = malloc(sizeof(FTPc_t));
	if (!ftpc)
		return NULL;

	memset(ftpc, 0, sizeof(FTPc_t));

	ftpc->cSock = -1;
	ftpc->dSock = -1;

	return ftpc;
}

FTPc_Result_t ftpc_connect(FTPc_t * ftpc, const char * host, uint16_t port, const char * username, const char * password)
{
	FTPc_Result_t result = FTPC_ERR_INTERNAL;

	// Check if network is available.
	if (!Network_isUp())
	{
		result = FTPC_ERR_NETWORK;
		goto conn_error;
	}


	// Resolve the hostname.
	struct sockaddr_in server;

	char serv[16];
	itoa(port, serv, 10);

	struct addrinfo hints;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	struct addrinfo * info;

	if (getaddrinfo(host, serv, &hints, &info) != 0)
	{
		result = FTPC_ERR_HOSTNAME;
		goto conn_error;
	}

	memcpy(&server, info->ai_addr, info->ai_addrlen);

	freeaddrinfo(info);

	if (server.sin_family != AF_INET)
	{
		result = FTPC_ERR_HOSTNAME;
		goto conn_error;
	}


	// Open the control socket.
	ftpc->cSock = socket(AF_INET, SOCK_STREAM, 0);
	if (ftpc->cSock < 0)
	{
		result = FTPC_ERR_CONNECT;
		goto conn_error;
	}

	struct timeval c_tv;
	c_tv.tv_sec = CONFIG_FTPC_CONTROL_TIMEOUT;
	c_tv.tv_usec = 0;
	setsockopt(ftpc->cSock, SOL_SOCKET, SO_SNDTIMEO, &c_tv, sizeof(struct timeval));
	setsockopt(ftpc->cSock, SOL_SOCKET, SO_RCVTIMEO, &c_tv, sizeof(struct timeval));

	if (connect(ftpc->cSock, (struct sockaddr*)&server, sizeof(struct sockaddr_in)) < 0)
	{
		result = FTPC_ERR_CONNECT;
		goto conn_error;
	}


	// Read the welcome message.
	if (receiveCommand(ftpc, ftpc->buffer, CONFIG_FTPC_BUFFER_SIZE) == 0)
	{
		result = FTPC_ERR_CLOSED;
		goto conn_error;
	}

	//The server responds with 220 (Service ready for new user.)
	if (returnCode(ftpc, ftpc->buffer, CONFIG_FTPC_BUFFER_SIZE) != 220)
	{
		result = FTPC_ERR_SERVER;
		goto conn_error;
	}

	//Read the rest of the welcome message.
	c_tv.tv_sec = 1;
	c_tv.tv_usec = 0;
	setsockopt(ftpc->cSock, SOL_SOCKET, SO_RCVTIMEO, &c_tv, sizeof(struct timeval));

	while (receiveCommand(ftpc, ftpc->buffer, CONFIG_FTPC_BUFFER_SIZE) > 0);

	c_tv.tv_sec = CONFIG_FTPC_CONTROL_TIMEOUT;
	c_tv.tv_usec = 0;
	setsockopt(ftpc->cSock, SOL_SOCKET, SO_RCVTIMEO, &c_tv, sizeof(struct timeval));


	int code = -1;

	//Set the username.
	snprintf(ftpc->buffer, CONFIG_FTPC_BUFFER_SIZE, "USER %s\r\n", username ? username : "anonymous");
	code = sendCommand(ftpc, ftpc->buffer);
	if (code < 0)
	{
		result = FTPC_ERR_CLOSED;
		goto conn_error;
	}
	else if ((code == 0) || (code >= 400))
	{
		result = FTPC_ERR_SERVER;
		goto conn_error;
	}

	//Set the password.
	snprintf(ftpc->buffer, CONFIG_FTPC_BUFFER_SIZE, "PASS %s\r\n", (username && password) ? password : "");
	code = sendCommand(ftpc, ftpc->buffer);
	if (code == 530)
	{
		result = FTPC_ERR_AUTH;
		goto conn_error;
	}
	else if (code < 0)
	{
		result = FTPC_ERR_CLOSED;
		goto conn_error;
	}
	else if ((code == 0) || (code >= 400))
	{
		result = FTPC_ERR_SERVER;
		goto conn_error;
	}

	// Ask the server for its operating system.
	// Used for informative/checking purposes.
#if 0
	// This is commented out as many servers violate the RFC959,
	// responding with custom messages, presenting their features.
	code = sendCommand(ftpc, "SYST\r\n");
	if (code < 0)
	{
		result = FTPC_ERR_CLOSED;
		goto conn_error;
	}
	else if ((code == 0) || (code >= 400))
	{
		result = FTPC_ERR_SERVER;
		goto conn_error;
	}
#endif

	// Set the server in binary mode.
	code = sendCommand(ftpc, "Type I\r\n");
	if (code < 0)
	{
		result = FTPC_ERR_CLOSED;
		goto conn_error;
	}
	else if ((code == 0) || (code >= 400))
	{
		result = FTPC_ERR_SERVER;
		goto conn_error;
	}

	// Set the server in passive mode.
	code = sendCommand(ftpc, "PASV\r\n");
	if (code < 0)
	{
		result = FTPC_ERR_CLOSED;
		goto conn_error;
	}
	else if (code != 227)
	{
		result = FTPC_ERR_SERVER;
		goto conn_error;
	}

	//Parse the data socket connection settings.
	char * tStr = strtok(ftpc->buffer, "(,");
	(void)tStr;

	int array_pasv[6] = { 0 };
	for (int i = 0; i < 6; i++)
	{
		tStr = strtok(NULL, "(,");
		if (tStr == NULL)
		{
			result = FTPC_ERR_INTERNAL;
			goto conn_error;
		}

		if (sscanf(tStr, "%d", &array_pasv[i]) != 1)
			goto conn_error;
	}

	memset(&ftpc->data_addr, 0, sizeof(struct sockaddr_in));
	ftpc->data_addr.sin_family = AF_INET;
	ftpc->data_addr.sin_port = HTONS(((array_pasv[4] & 0xFF) << 8) | (array_pasv[5] & 0xFF));
	ftpc->data_addr.sin_addr.s_addr = HTONL(((array_pasv[0] & 0xFF) << 24) |
											((array_pasv[1] & 0xFF) << 16) |
											((array_pasv[2] & 0xFF) << 8)  |
											((array_pasv[3] & 0xFF) << 0));

	ftpc->connected = 1;
	result = FTPC_OK;
	return result;


conn_error:
	close(ftpc->cSock);

	memset(ftpc, 0, sizeof(FTPc_t));
	ftpc->cSock = -1;
	ftpc->dSock = -1;

	return result;
}

ssize_t ftpc_filesize(FTPc_t * ftpc, const char * r_file)
{
	if (!ftpc->connected)
		return FTPC_ERR_CLOSED;

	snprintf(ftpc->buffer, CONFIG_FTPC_BUFFER_SIZE, "SIZE %s\r\n", r_file);
	FTP_CHECK_CODE(sendCommand(ftpc, ftpc->buffer));

	ssize_t filesize = 0;

	size_t idx = 0;
	while (!isdigit(ftpc->buffer[idx]) && (idx < CONFIG_FTPC_BUFFER_SIZE)) idx++;
	filesize = strtol(strstr(&ftpc->buffer[idx], " ") + 1, NULL, 10);

	if (filesize == 0)
		return FTPC_ERR_INTERNAL;

	return filesize;
}

ssize_t ftpc_retrieve(FTPc_t * ftpc, const char * r_file, const char * l_file)
{
	if (!ftpc->connected)
		return FTPC_ERR_CLOSED;

	ssize_t filesize = ftpc_filesize(ftpc, r_file);
	if (filesize < 0)
		return filesize;

	if (!openData(ftpc))
		return FTPC_ERR_CONNECT;

	snprintf(ftpc->buffer, CONFIG_FTPC_BUFFER_SIZE, "RETR %s\r\n", r_file);
	FTP_CHECK_CODE(sendCommand(ftpc, ftpc->buffer));


	FTPc_Result_t result = FTPC_ERR_INTERNAL;
	(void)result;

	int fd = open(l_file, (O_WRONLY | O_CREAT), 0666);
	if (fd < 0)
	{
		result = FTPC_ERR_IO;
		goto error;
	}

#ifndef CONFIG_FTPC_SENDFILE
	int remaining = filesize;
	while (remaining > 0)
	{
		memset(ftpc->buffer, 0, CONFIG_FTPC_BUFFER_SIZE);
		int bytes = recv(ftpc->dSock, ftpc->buffer, (CONFIG_FTPC_BUFFER_SIZE < remaining) ? CONFIG_FTPC_BUFFER_SIZE : remaining, 0);

		if (bytes <= 0)
		{
			result = FTPC_ERR_CLOSED;
			goto error;
		}

		if (write(fd, ftpc->buffer, bytes) != bytes)
		{
			result = FTPC_ERR_IO;
			goto error;
		}

		remaining -= bytes;
		DEBUGASSERT(remaining >= 0);
	}
#else
	int received = sendfile(fd, ftpc->dSock, NULL, filesize);
	if ((received < 0) || (received != filesize))
	{
		result = FTPC_ERR_IO;
		goto error;
	}
#endif

	closeData(ftpc);
	close(fd);

	return filesize;


error:
	closeData(ftpc);
	close(fd);

	ftpc->connected = 0;

	return result;
}

ssize_t ftpc_store(FTPc_t * ftpc, const char * r_file, const char * l_file)
{
	if (!ftpc->connected)
		return FTPC_ERR_CLOSED;

	if (access(l_file, F_OK) != 0)
		return FTPC_NO_FILE;

	if (!openData(ftpc))
		return FTPC_ERR_CONNECT;

	snprintf(ftpc->buffer, CONFIG_FTPC_BUFFER_SIZE, "STOR %s\r\n", r_file);
	FTP_CHECK_CODE(sendCommand(ftpc, ftpc->buffer));


	FTPc_Result_t result = FTPC_ERR_INTERNAL;
	(void)result;

	int fd = open(l_file, O_RDONLY, 0666);
	if (fd < 0)
	{
		result = FTPC_ERR_IO;
		goto error;
	}

#ifndef CONFIG_FTPC_SENDFILE
	ssize_t filesize = 0;
	while (1)
	{
		memset(ftpc->buffer, 0, CONFIG_FTPC_BUFFER_SIZE);
		int bytes = read(fd, ftpc->buffer, CONFIG_FTPC_BUFFER_SIZE);

		if (bytes == 0)
			break;

		if (bytes < 0)
		{
			result = FTPC_ERR_IO;
			goto error;
		}

		if (send(ftpc->dSock, ftpc->buffer, bytes, 0) != bytes)
		{
			result = FTPC_ERR_CLOSED;
			goto error;
		}

		filesize += bytes;
	}
#else
	struct stat f_stat;
	if (stat(l_file, &f_stat) < 0)
	{
		result = FTPC_ERR_IO;
		goto error;
	}

	ssize_t filesize = sendfile(ftpc->dSock, fd, NULL, f_stat.st_size);
	if (filesize < 0)
	{
		result = FTPC_ERR_IO;
		goto error;
	}
#endif

	closeData(ftpc);
	close(fd);

	return filesize;


error:
	closeData(ftpc);
	close(fd);

	ftpc->connected = 0;

	return result;
}

ssize_t ftpc_storeUnique(FTPc_t * ftpc, const char * path, const char * l_file)
{
	if (!ftpc->connected)
		return FTPC_ERR_CLOSED;

	if (access(l_file, F_OK) != 0)
		return FTPC_NO_FILE;

	if (!openData(ftpc))
		return FTPC_ERR_CONNECT;

	snprintf(ftpc->buffer, CONFIG_FTPC_BUFFER_SIZE, "STOU %s\r\n", path ? path : "/");
	FTP_CHECK_CODE(sendCommand(ftpc, ftpc->buffer));


	FTPc_Result_t result = FTPC_ERR_INTERNAL;
	(void)result;

	int fd = open(l_file, O_RDONLY, 0666);
	if (fd < 0)
	{
		result = FTPC_ERR_IO;
		goto error;
	}

#ifndef CONFIG_FTPC_SENDFILE
	ssize_t filesize = 0;
	while (1)
	{
		memset(ftpc->buffer, 0, CONFIG_FTPC_BUFFER_SIZE);
		int bytes = read(fd, ftpc->buffer, CONFIG_FTPC_BUFFER_SIZE);

		if (bytes == 0)
			break;

		if (bytes < 0)
		{
			result = FTPC_ERR_IO;
			goto error;
		}

		if (send(ftpc->dSock, ftpc->buffer, bytes, 0) != bytes)
		{
			result = FTPC_ERR_CLOSED;
			goto error;
		}

		filesize += bytes;
	}
#else
	struct stat f_stat;
	if (stat(l_file, &f_stat) < 0)
	{
		result = FTPC_ERR_IO;
		goto error;
	}

	ssize_t filesize = sendfile(ftpc->dSock, fd, NULL, f_stat.st_size);
	if (filesize < 0)
	{
		result = FTPC_ERR_IO;
		goto error;
	}
#endif

	closeData(ftpc);
	close(fd);

	return filesize;


error:
	closeData(ftpc);
	close(fd);

	ftpc->connected = 0;

	return result;
}

FTPc_Result_t ftpc_rename(FTPc_t * ftpc, const char * old, const char * new)
{
	if (!ftpc->connected)
		return FTPC_ERR_CLOSED;

	snprintf(ftpc->buffer, CONFIG_FTPC_BUFFER_SIZE, "RNFR %s\r\n", old);
	FTP_CHECK_CODE(sendCommand(ftpc, ftpc->buffer));

	snprintf(ftpc->buffer, CONFIG_FTPC_BUFFER_SIZE, "RNTO %s\r\n", new);
	FTP_CHECK_CODE(sendCommand(ftpc, ftpc->buffer));

	return FTPC_OK;
}

FTPc_Result_t ftpc_delete(FTPc_t * ftpc, const char * r_file)
{
	if (!ftpc->connected)
		return FTPC_ERR_CLOSED;

	snprintf(ftpc->buffer, CONFIG_FTPC_BUFFER_SIZE, "DELE %s\r\n", r_file);
	FTP_CHECK_CODE(sendCommand(ftpc, ftpc->buffer));

	return FTPC_OK;
}

FTPc_Result_t ftpc_createDir(FTPc_t * ftpc, const char * path)
{
	if (!ftpc->connected)
		return FTPC_ERR_CLOSED;

	snprintf(ftpc->buffer, CONFIG_FTPC_BUFFER_SIZE, "MKD %s\r\n", path);
	FTP_CHECK_CODE(sendCommand(ftpc, ftpc->buffer));

	return FTPC_OK;
}

FTPc_Result_t ftpc_deleteDir(FTPc_t * ftpc, const char * path)
{
	if (!ftpc->connected)
		return FTPC_ERR_CLOSED;

	snprintf(ftpc->buffer, CONFIG_FTPC_BUFFER_SIZE, "RMD %s\r\n", path);
	FTP_CHECK_CODE(sendCommand(ftpc, ftpc->buffer));

	return FTPC_OK;
}

FTPc_Result_t ftpc_quit(FTPc_t * ftpc)
{
	if (ftpc == NULL)
		return FTPC_OK;

	closeData(ftpc);

	if (ftpc->connected)
	{
		snprintf(ftpc->buffer, CONFIG_FTPC_BUFFER_SIZE, "QUIT\r\n");
		sendCommand(ftpc, ftpc->buffer);

		ftpc->connected = 0;
	}

	close(ftpc->cSock);

	free(ftpc);

	return FTPC_OK;
}


int openData(FTPc_t * ftpc)
{
	if (ftpc->cSock <= 0)
		return 0;

	ftpc->dSock = socket(AF_INET, SOCK_STREAM, 0);
	if (ftpc->dSock < 0)
		return 0;

	struct timeval d_tv;
	d_tv.tv_sec  = CONFIG_FTPC_DATA_TIMEOUT;
	d_tv.tv_usec = 0;
	setsockopt(ftpc->dSock, SOL_SOCKET, SO_SNDTIMEO, &d_tv, sizeof(struct timeval));
	setsockopt(ftpc->dSock, SOL_SOCKET, SO_RCVTIMEO, &d_tv, sizeof(struct timeval));

	if (connect(ftpc->dSock, (struct sockaddr*)&ftpc->data_addr, sizeof(struct sockaddr_in)) < 0)
		return 0;

	return 1;
}

int closeData(FTPc_t * ftpc)
{
	close(ftpc->dSock);
	ftpc->dSock = -1;

	return 1;
}

int sendCommand(FTPc_t * ftpc, const char * message)
{
	if (send(ftpc->cSock, message, strlen(message), 0) <= 0)
	{
		ftpc->connected = 0;
		return -1;
	}

	if (receiveCommand(ftpc, ftpc->buffer, CONFIG_FTPC_BUFFER_SIZE) == 0)
	{
		ftpc->connected = 0;
		return -1;
	}

	return returnCode(ftpc, ftpc->buffer, CONFIG_FTPC_BUFFER_SIZE);
}

size_t receiveCommand(FTPc_t * ftpc, char * buffer, size_t size)
{
	size_t idx = 0;
	while (recv(ftpc->cSock, &buffer[idx], 1, 0) == 1)
	{
		idx++;

		if (idx >= (size - 1))
			break;

		if ((idx >= 2) && (buffer[idx - 1] == '\n') && (buffer[idx - 2] == '\r'))
			break;
	}

	buffer[idx] = '\0';

	return idx;
}

int returnCode(FTPc_t * ftpc, const char * buffer, size_t size)
{
	(void)ftpc;

	size_t idx = 0;
	while (!isdigit(buffer[idx]) && (idx < size)) idx++;
	return strtol(&buffer[idx], NULL, 10);
}


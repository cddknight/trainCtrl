/**********************************************************************************************************************
 *                                                                                                                    *
 *  S O C K E T  C . C                                                                                                *
 *  ==================                                                                                                *
 *                                                                                                                    *
 *  This is free software; you can redistribute it and/or modify it under the terms of the GNU General Public         *
 *  License version 2 as published by the Free Software Foundation.  Note that I am not granting permission to        *
 *  redistribute or modify this under the terms of any later version of the General Public License.                   *
 *                                                                                                                    *
 *  This is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied        *
 *  warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more     *
 *  details.                                                                                                          *
 *                                                                                                                    *
 *  You should have received a copy of the GNU General Public License along with this program (in the file            *
 *  "COPYING"); if not, write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111,   *
 *  USA.                                                                                                              *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \file
 *  \brief Socket connections.
 */
#include <netinet/in.h>
#include <netdb.h>
#include <sys/un.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>

#include "socketC.h"

const int MAXCONNECTIONS = 5;

/**********************************************************************************************************************
 *                                                                                                                    *
 *  S E R V E R  S O C K E T  S E T U P                                                                               *
 *  ===================================                                                                               *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Setup a server socket listenning on a port.
 *  \param port Port to listen on.
 *  \result The socket handle of the server, or -1 if server failed.
 */
int ServerSocketSetup (int port)
{
	struct sockaddr_in mAddress;
	int on = 1, mSocket = socket (AF_INET, SOCK_STREAM, 0);

	if (!SocketValid (mSocket))
		return -1;

	if (setsockopt (mSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&on, sizeof (on)) == -1)
	{
		close (mSocket);
		return -1;
	}

	memset (&mAddress, 0, sizeof (mAddress));
	mAddress.sin_family = AF_INET;
	mAddress.sin_addr.s_addr = INADDR_ANY;
	mAddress.sin_port = htons (port);

	if (bind (mSocket, (struct sockaddr *) &mAddress, sizeof (mAddress)) == -1)
	{
		close (mSocket);
		return -1;
	}

	if (listen (mSocket, MAXCONNECTIONS) == -1)
	{
		close (mSocket);
		return -1;
	}
	return mSocket;
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  S E R V E R  S O C K E T  F I L E                                                                                 *
 *  =================================                                                                                 *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Setup a server listenning on a unix socket.
 *  \param fileName Name for the unix socket.
 *  \result The socket handle of the server, or -1 if server failed.
 */
int ServerSocketFile (char *fileName)
{
	int len, mSocket = socket (AF_UNIX, SOCK_STREAM, 0);
	struct sockaddr_un mAddress;

	if (!SocketValid (mSocket))
		return -1;

	mAddress.sun_family = AF_UNIX;
	strcpy (mAddress.sun_path, fileName);
	unlink (mAddress.sun_path);
	len = strlen(mAddress.sun_path) + sizeof(mAddress.sun_family);

	if (bind (mSocket, (struct sockaddr *)&mAddress, len) == -1)
	{
		close (mSocket);
		return -1;
	}

	if (listen (mSocket, MAXCONNECTIONS) == -1)
	{
		close (mSocket);
		return -1;
	}
	return mSocket;
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  S E R V E R  S O C K E T  A C C E P T                                                                             *
 *  =====================================                                                                             *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Accept a new connection on a listening port.
 *  \param socket Listening socket.
 *  \param address Save remote address here.
 *  \result Handle of new socket.
 */
int ServerSocketAccept (int socket, char *address)
{
	struct sockaddr_in mAddress;
	struct timeval timeout;
	fd_set fdset;
	int addr_length = sizeof (mAddress), clientSocket = -1;

	timeout.tv_sec = 1;
	timeout.tv_usec = 0;

	FD_ZERO (&fdset);
	FD_SET (socket, &fdset);
	if (select (FD_SETSIZE, &fdset, NULL, NULL, &timeout) < 1)
	{
		return -1;
	}
	if (!FD_ISSET(socket, &fdset))
	{
		return -1;
	}

	clientSocket = accept (socket, (struct sockaddr *) &mAddress, (socklen_t *) &addr_length);
	if (clientSocket != -1 && address)
	{
		sprintf (address, "%d.%d.%d.%d",
				mAddress.sin_addr.s_addr & 0xFF,
				mAddress.sin_addr.s_addr >> 8  & 0xFF,
				mAddress.sin_addr.s_addr >> 16 & 0xFF,
				mAddress.sin_addr.s_addr >> 24 & 0xFF);
	}
	return clientSocket;
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  C O N N E C T  S O C K E T  F I L E                                                                               *
 *  ===================================                                                                               *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Connect to a unix socket (file).
 *  \param fileName Socket file to connecto to.
 *  \result Handle of socket or -1 if failed.
 */
int ConnectSocketFile (char *fileName)
{
	struct sockaddr_un mAddress;
	int len, mSocket = socket(AF_UNIX, SOCK_STREAM, 0);

	if (!SocketValid (mSocket))
	{
		return -1;
	}

	mAddress.sun_family = AF_UNIX;
	strcpy(mAddress.sun_path, fileName);
	len = strlen(mAddress.sun_path) + sizeof(mAddress.sun_family);
	if (connect (mSocket, (struct sockaddr *)&mAddress, len) == -1)
	{
		close (mSocket);
		return -1;
	}
	return mSocket;
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  C O N N E C T  C L I E N T  S O C K E T                                                                           *
 *  =======================================                                                                           *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Connect to a remote socket.
 *  \param host Host address to connecto to.
 *  \param port Host port to connect to.
 *  \result Handle of socket or -1 if failed.
 */
int ConnectClientSocket (char *host, int port)
{
	struct sockaddr_in mAddress;
	int on = 1, mSocket = socket (AF_INET, SOCK_STREAM, 0);

	if (!SocketValid (mSocket))
	{
		return -1;
	}

	if (setsockopt (mSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&on, sizeof (on)) == -1)
	{
		close (mSocket);
		return -1;
	}

	memset (&mAddress, 0, sizeof (mAddress));
	mAddress.sin_family = AF_INET;
	mAddress.sin_port = htons (port);

	if (inet_pton (AF_INET, host, &mAddress.sin_addr) == EAFNOSUPPORT)
	{
		close (mSocket);
		return -1;
	}
	if (connect (mSocket, (struct sockaddr *) &mAddress, sizeof (mAddress)) != 0)
	{
		close (mSocket);
		return -1;
	}
	return mSocket;
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  S E N D  S O C K E T                                                                                              *
 *  ====================                                                                                              *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Send data to a socket.
 *  \param socket Which socket to send to.
 *  \param buffer Buffer to send.
 *  \param size Size of data to send.
 *  \result Bytes sent.
 */
int SendSocket (int socket, char *buffer, int size)
{
	return send (socket, buffer, size, MSG_NOSIGNAL);
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  R E C V  S O C K E T                                                                                              *
 *  ====================                                                                                              *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Receive data from the socket.
 *  \param socket Which socket to receive from.
 *  \param buffer Buffer to save to.
 *  \param size Max size of the receive buffer.
 *  \result Bytes received.
 */
int RecvSocket (int socket, char *buffer, int size)
{
	int retn = recv (socket, buffer, size, MSG_DONTWAIT);
	return (retn > 0 ? retn : 0);
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  C L O S E  S O C K E T                                                                                            *
 *  ======================                                                                                            *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Clock a socket.
 *  \param socket Socket to close.
 *  \result None.
 */
int CloseSocket (int *socket)
{
	if (SocketValid (*socket))
	{
		close (*socket);
		*socket = -1;
	}
	return -1;
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  S O C K E T  V A L I D                                                                                            *
 *  ======================                                                                                            *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Check socket handle is not -1.
 *  \param socket Socket handle to check.
 *  \result True if handle is not -1.
 */
int SocketValid (int socket)
{
	return (socket != -1);
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  G E T  A D D R E S S  F R O M  N A M E                                                                            *
 *  ======================================                                                                            *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Convert and addess to an IP address with a lookup.
 *  \param name Name to look up.
 *  \param address Out out the address here.
 *  \result 1 if address resolved.
 */
int GetAddressFromName (char *name, char *address)
{
	struct hostent *hostEntry = gethostbyname(name);

	if (hostEntry)
	{
		if (hostEntry -> h_addr_list[0])
		{
			sprintf (address, "%d.%d.%d.%d",
					(int)hostEntry -> h_addr_list[0][0] & 0xFF,
					(int)hostEntry -> h_addr_list[0][1] & 0xFF,
					(int)hostEntry -> h_addr_list[0][2] & 0xFF,
					(int)hostEntry -> h_addr_list[0][3] & 0xFF);
			return 1;
		}
	}
	return 0;
}

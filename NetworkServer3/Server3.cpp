#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <iostream>
using namespace std;
#pragma comment(lib, "Ws2_32.lib")
#include <winsock2.h>
#include <string.h>
#include <time.h>
#include <string>
#include <fstream>

constexpr int PORT = 27015;
constexpr int MAX_SOCKETS = 60;
constexpr int BUFF_SIZE = 1024;


struct SocketState
{
	SOCKET					id;
	enum eSocketStatus		recv;
	enum eSocketStatus		send;
	enum eRequestType		httpReq;
	char					buffer[BUFF_SIZE];
	time_t					prevActivity;
	int						socketDataLen;
};

enum eSocketStatus {
	EMPTY,
	LISTEN,
	RECEIVE,
	IDLE,
	SEND
};

enum eRequestType
{
	GET = 1,
	HEAD,
	PUT,
	POST,
	R_DELETE,
	TRACE,
	OPTIONS
};

bool addSocket(SOCKET id, enum eSocketStatus what);
void removeSocket(int index);
void acceptConnection(int index);
void receiveMessage(int index);
bool sendMessage(int index);
int PutRequest(int index, char* filename);


struct SocketState sockets[MAX_SOCKETS] = { 0 };
int socketsCount = 0;


void main()
{
	time_t currentTime;

	//set timeout for 'select' function
	struct timeval timeOut;
	timeOut.tv_sec = 120;
	timeOut.tv_usec = 0;

	// Initialize Winsock (Windows Sockets).

	// Create a WSADATA object called wsaData.
	// The WSADATA structure contains information about the Windows 
	// Sockets implementation.
	WSAData wsaData;

	// Call WSAStartup and return its value as an integer and check for errors.
	// The WSAStartup function initiates the use of WS2_32.DLL by a process.
	// First parameter is the version number 2.2.
	// The WSACleanup function destructs the use of WS2_32.DLL by a process.
	if (NO_ERROR != WSAStartup(MAKEWORD(2, 2), &wsaData))
	{
		cout << "Time Server: Error at WSAStartup()\n";
		return;
	}

	// Server side:
	// Create and bind a socket to an internet address.
	// Listen through the socket for incoming connections.

	// After initialization, a SOCKET object is ready to be instantiated.

	// Create a SOCKET object called listenSocket. 
	// For this application:	use the Internet address family (AF_INET), 
	//							streaming sockets (SOCK_STREAM), 
	//							and the TCP/IP protocol (IPPROTO_TCP).
	SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	// Check for errors to ensure that the socket is a valid socket.
	// Error detection is a key part of successful networking code. 
	// If the socket call fails, it returns INVALID_SOCKET. 
	// The if statement in the previous code is used to catch any errors that
	// may have occurred while creating the socket. WSAGetLastError returns an 
	// error number associated with the last error that occurred.
	if (INVALID_SOCKET == listenSocket)
	{
		cout << "Time Server: Error at socket(): " << WSAGetLastError() << endl;
		WSACleanup();
		return;
	}

	// For a server to communicate on a network, it must bind the socket to 
	// a network address.

	// Need to assemble the required data for connection in sockaddr structure.

	// Create a sockaddr_in object called serverService. 
	sockaddr_in serverService;
	// Address family (must be AF_INET - Internet address family).
	serverService.sin_family = AF_INET;
	// IP address. The sin_addr is a union (s_addr is a unsigned long 
	// (4 bytes) data type).
	// inet_addr (Iternet address) is used to convert a string (char *) 
	// into unsigned long.
	// The IP address is INADDR_ANY to accept connections on all interfaces.
	serverService.sin_addr.s_addr = INADDR_ANY;
	// IP Port. The htons (host to network - short) function converts an
	// unsigned short from host to TCP/IP network byte order 
	// (which is big-endian).
	serverService.sin_port = htons(PORT);

	// Bind the socket for client's requests.

	// The bind function establishes a connection to a specified socket.
	// The function uses the socket handler, the sockaddr structure (which
	// defines properties of the desired connection) and the length of the
	// sockaddr structure (in bytes).
	if (SOCKET_ERROR == bind(listenSocket, (SOCKADDR*)&serverService, sizeof(serverService)))
	{
		cout << "Time Server: Error at bind(): " << WSAGetLastError() << endl;
		closesocket(listenSocket);
		WSACleanup();
		return;
	}

	// Listen on the Socket for incoming connections.
	// This socket accepts only one connection (no pending connections 
	// from other clients). This sets the backlog parameter.
	if (SOCKET_ERROR == listen(listenSocket, 50))
	{
		cout << "Time Server: Error at listen(): " << WSAGetLastError() << endl;
		closesocket(listenSocket);
		WSACleanup();
		return;
	}

	addSocket(listenSocket, LISTEN);

	// Accept connections and handles them one by one.
	cout << "Waiting for client connections..." << endl;

	while (true)
	{
		// The select function determines the status of one or more sockets,
		// waiting if necessary, to perform asynchronous I/O. Use fd_sets for
		// sets of handles for reading, writing and exceptions. select gets "timeout" for waiting
		// and still performing other operations (Use NULL for blocking). Finally,
		// select returns the number of descriptors which are ready for use (use FD_ISSET
		// macro to check which descriptor in each set is ready to be used).


		// We check for the inactive sockets (inactive for 2 minutes+) and remove them
		for (int i = 1; i < MAX_SOCKETS; i++)
		{
			currentTime = time(0);
			if ((currentTime - sockets[i].prevActivity > 120) && (sockets[i].prevActivity != 0))
			{
				removeSocket(i);
			}
		}

		fd_set waitRecv;
		FD_ZERO(&waitRecv);
		for (int i = 0; i < MAX_SOCKETS; i++)
		{
			if ((sockets[i].recv == LISTEN) || (sockets[i].recv == RECEIVE))
				FD_SET(sockets[i].id, &waitRecv);
		}

		fd_set waitSend;
		FD_ZERO(&waitSend);
		for (int i = 0; i < MAX_SOCKETS; i++)
		{
			if (sockets[i].send == SEND)
				FD_SET(sockets[i].id, &waitSend);
		}

		// Wait for interesting event.
		// Note: First argument is ignored. The fourth is for exceptions.
		// And as written above the last is a timeout, hence we are blocked if nothing happens.
		int nfd;
		nfd = select(0, &waitRecv, &waitSend, NULL, &timeOut);
		if (nfd == SOCKET_ERROR)
		{
			cout << "Time Server: Error at select(): " << WSAGetLastError() << endl;
			WSACleanup();
			return;
		}

		for (int i = 0; i < MAX_SOCKETS && nfd > 0; i++)
		{
			if (FD_ISSET(sockets[i].id, &waitRecv))
			{
				nfd--;
				switch (sockets[i].recv)
				{
				case LISTEN:
					acceptConnection(i);
					break;

				case RECEIVE:
					receiveMessage(i);
					break;
				}
			}
		}

		for (int i = 0; i < MAX_SOCKETS && nfd > 0; i++)
		{
			if (FD_ISSET(sockets[i].id, &waitSend))
			{
				nfd--;
				if (sockets[i].send == SEND)
				{
					if (!sendMessage(i))
						continue;
				}
			}
		}
	}

	// Closing connections and Winsock.
	cout << "HTTP Server: Closing Connection.\n";
	closesocket(listenSocket);
	WSACleanup();
}

bool addSocket(SOCKET id, enum eSocketStatus what)
{
	for (int i = 0; i < MAX_SOCKETS; i++)
	{
		if (sockets[i].recv == EMPTY)
		{
			sockets[i].id = id;
			sockets[i].recv = what;
			sockets[i].send = IDLE;
			sockets[i].prevActivity = time(0);
			sockets[i].socketDataLen = 0;
			socketsCount++;
			return true;
		}
	}

	return false;
}

void removeSocket(int index)
{
	sockets[index].recv = EMPTY;
	sockets[index].send = EMPTY;
	sockets[index].prevActivity = 0;
	socketsCount--;
	cout << "The socket number " << index << " has been removed" << endl;
}

void acceptConnection(int index)
{
	SOCKET id = sockets[index].id;
	sockets[index].prevActivity = time(0);
	struct sockaddr_in from;		// Address of sending partner
	int fromLen = sizeof(from);

	SOCKET msgSocket = accept(id, (struct sockaddr*)&from, &fromLen);
	if (INVALID_SOCKET == msgSocket)
	{
		cout << "HTTP Server: Error at accept(): " << WSAGetLastError() << endl;
		return;
	}

	cout << "HTTP Server: Client " << inet_ntoa(from.sin_addr) << ":" << ntohs(from.sin_port) << " is connected." << endl;

	// Set the socket to be in non-blocking mode.
	unsigned long flag = 1;
	if (ioctlsocket(msgSocket, FIONBIO, &flag) != 0)
	{
		cout << "HTTP Server: Error at ioctlsocket(): " << WSAGetLastError() << endl;
	}

	if (addSocket(msgSocket, RECEIVE) == false)
	{
		cout << "\t\tToo many connections, dropped!\n";
		closesocket(id);
	}

	return;
}

void receiveMessage(int index)
{
	SOCKET msgSocket = sockets[index].id;

	int len = sockets[index].socketDataLen;
	int bytesRecv = recv(msgSocket, &sockets[index].buffer[len], sizeof(sockets[index].buffer) - len, 0);

	if (SOCKET_ERROR == bytesRecv)
	{
		cout << "HTTP Server: Error at recv(): " << WSAGetLastError() << endl;
		closesocket(msgSocket);
		removeSocket(index);
		return;
	}
	if (bytesRecv == 0)
	{
		closesocket(msgSocket);
		removeSocket(index);
		return;
	}
	else
	{
		sockets[index].buffer[len + bytesRecv] = '\0'; //add the null-terminating to make it a string
		cout << "HTTP Server: Recieved: " << bytesRecv << " bytes of \"" << &sockets[index].buffer[len] << "\" message.\n";
		sockets[index].socketDataLen += bytesRecv;

		if (sockets[index].socketDataLen > 0)
		{
			if (strncmp(sockets[index].buffer, "GET", 3) == 0)
			{
				sockets[index].send = SEND;
				sockets[index].httpReq = GET;
				strcpy(sockets[index].buffer, &sockets[index].buffer[5]);
				sockets[index].socketDataLen = strlen(sockets[index].buffer);
				sockets[index].buffer[sockets[index].socketDataLen] = NULL;
				return;
			}
			else if (strncmp(sockets[index].buffer, "HEAD", 4) == 0)
			{
				sockets[index].send = SEND;
				sockets[index].httpReq = HEAD;
				strcpy(sockets[index].buffer, &sockets[index].buffer[6]);
				sockets[index].socketDataLen = strlen(sockets[index].buffer);
				sockets[index].buffer[sockets[index].socketDataLen] = NULL;
				return;
			}
			else if (strncmp(sockets[index].buffer, "PUT", 3) == 0)
			{
				sockets[index].send = SEND;
				sockets[index].httpReq = PUT;
				return;
			}
			else if (strncmp(sockets[index].buffer, "DELETE", 6) == 0)
			{
				sockets[index].send = SEND;
				sockets[index].httpReq = R_DELETE;
				return;
			}
			else if (strncmp(sockets[index].buffer, "TRACE", 5) == 0)
			{
				sockets[index].send = SEND;
				sockets[index].httpReq = TRACE;
				strcpy(sockets[index].buffer, &sockets[index].buffer[5]);
				sockets[index].socketDataLen = strlen(sockets[index].buffer);
				sockets[index].buffer[sockets[index].socketDataLen] = NULL;
				return;
			}
			else if (strncmp(sockets[index].buffer, "OPTIONS", 7) == 0)
			{
				sockets[index].send = SEND;
				sockets[index].httpReq = OPTIONS;
				strcpy(sockets[index].buffer, &sockets[index].buffer[9]);
				sockets[index].socketDataLen = strlen(sockets[index].buffer);
				sockets[index].buffer[sockets[index].socketDataLen] = NULL;
				return;
			}
			else if (strncmp(sockets[index].buffer, "POST", 4) == 0)
			{
				sockets[index].send = SEND;
				sockets[index].httpReq = POST;
				strcpy(sockets[index].buffer, &sockets[index].buffer[6]);
				sockets[index].socketDataLen = strlen(sockets[index].buffer);
				sockets[index].buffer[sockets[index].socketDataLen] = NULL;
				return;
			}
		}
	}
}

bool sendMessage(int index)
{
	int bytesSent = 0, buffLen = 0, fileSize = 0;
	char sendBuff[BUFF_SIZE];
	char *tempFromTok;
	char tempBuff[BUFF_SIZE], readBuff[BUFF_SIZE];
	string fullMessage, fileSizeString, innerAddress;
	ifstream inFile;
	time_t currentTime;
	time(&currentTime); // Get current time
	SOCKET msgSocket = sockets[index].id;
	sockets[index].prevActivity = time(0); // Reset activity

	switch (sockets[index].httpReq)
	{
		case HEAD:
		{
			tempFromTok = strtok(sockets[index].buffer, " ");
			innerAddress = "C:\\Temp\\en\\index.html"; // we redirect to default english file
			inFile.open(innerAddress);
			if (!inFile)
			{
				fullMessage = "HTTP/1.1 404 Not Found ";
				fileSize = 0;
			}
			else 
			{
				fullMessage = "HTTP/1.1 200 OK ";
				inFile.seekg(0, ios::end);
				fileSize = inFile.tellg(); // get length of content in file
			}

			fullMessage += "\r\nContent-type: text/html";
			fullMessage += "\r\nDate:";
			fullMessage += ctime(&currentTime);
			fullMessage += "Content-length: ";
			fileSizeString = to_string(fileSize);
			fullMessage += fileSizeString;
			fullMessage += "\r\n\r\n";
			buffLen = fullMessage.size();
			strcpy(sendBuff, fullMessage.c_str());
			inFile.close();
			break;
		}

		case GET:
		{
			string tempStringFromFile = ""; 
			tempFromTok = strtok(sockets[index].buffer, " ");
			innerAddress = "C:\\Temp\\"; // we redirect to default english file
			char *langPtr = strchr(tempFromTok, '?'); // search if there are query params
			if (langPtr == NULL) // default - english page
			{
				innerAddress += "en";
			}
			else 
			{
				langPtr += 6; // skip to language param ('?lang=' is 6 chars)
				for (int i = 0; i < 2; ++i, langPtr++) // extract language from parameters
					innerAddress += *langPtr;
			}

			innerAddress += '\\';
			tempFromTok = strtok(tempFromTok, "?");
			innerAddress.append(tempFromTok);
			inFile.open(innerAddress);
			if (!inFile)
			{
				fullMessage = "HTTP/1.1 404 Not Found ";
				fileSize = 0;
			}
			else
			{
				fullMessage = "HTTP/1.1 200 OK ";
			}

			if (inFile)
			{
				// Read from file to temp buffer and get its length
				while (inFile.getline(readBuff, BUFF_SIZE))
				{
					tempStringFromFile += readBuff;
					fileSize += strlen(readBuff);
				}
			}

			fullMessage += "\r\nContent-type: text/html";
			fullMessage += "\r\nDate:";
			fullMessage += ctime(&currentTime);
			fullMessage += "Content-length: ";
			fileSizeString = to_string(fileSize);
			fullMessage += fileSizeString;
			fullMessage += "\r\n\r\n";
			fullMessage += tempStringFromFile; // Get content
			buffLen = fullMessage.size();
			strcpy(sendBuff, fullMessage.c_str());
			inFile.close();
			break;
		}

		case PUT:
		{
			char fileName[BUFF_SIZE];
			int returnCode = PutRequest(index, fileName);
			switch (returnCode)
			{
				case 0: 
				{
					cout << "PUT " << fileName << "Failed";
					fullMessage = "HTTP/1.1 412 Precondition failed \r\nDate: ";
					break;
				}

				case 200: 
				{
					fullMessage = "HTTP/1.1 200 OK \r\nDate: ";
					break;
				}

				case 201: 
				{
					fullMessage = "HTTP/1.1 201 Created \r\nDate: ";
					break;
				}

				case 204: 
				{
					fullMessage = "HTTP/1.1 204 No Content \r\nDate: ";
					break;
				}

				default: 
				{
					fullMessage = "HTTP/1.1 501 Not Implemented \r\nDate: ";
					break;
				}
			}

			fullMessage += ctime(&currentTime);
			fullMessage += "Content-length: ";
			fileSizeString = to_string(fileSize);
			fullMessage += fileSizeString;
			fullMessage += "\r\n\r\n";
			buffLen = fullMessage.size();
			strcpy(sendBuff, fullMessage.c_str());
			break;
		}

		case R_DELETE:
		{
			strtok(&sockets[index].buffer[8], " ");
			strcpy(tempBuff, &sockets[index].buffer[8]);
			if (remove(tempBuff) != 0)
			{
				fullMessage = "HTTP/1.1 204 No Content \r\nDate: "; // We treat 204 code as a case where delete wasn't successful
			}
			else
			{
				fullMessage = "HTTP/1.1 200 OK \r\nDate: "; // File deleted succesfully
			}

			fullMessage += ctime(&currentTime);
			fullMessage += "Content-length: ";
			fileSizeString = to_string(fileSize);
			fullMessage += fileSizeString;
			fullMessage += "\r\n\r\n";
			buffLen = fullMessage.size();
			strcpy(sendBuff, fullMessage.c_str());
			break;
		}

		case TRACE:
		{
			fileSize = strlen("TRACE");
			fileSize += strlen(sockets[index].buffer);
			fullMessage = "HTTP/1.1 200 OK \r\nContent-type: message/http\r\nDate: ";
			fullMessage += ctime(&currentTime);
			fullMessage += "Content-length: ";
			fileSizeString = to_string(fileSize);
			fullMessage += fileSizeString;
			fullMessage += "\r\n\r\n";
			fullMessage += "TRACE";
			fullMessage += sockets[index].buffer;
			buffLen = fullMessage.size();
			strcpy(sendBuff, fullMessage.c_str());
			break;
		}

		case OPTIONS:
		{
			fullMessage = "HTTP/1.1 204 No Content\r\nAllow: OPTIONS, GET, HEAD, POST, PUT, TRACE, DELETE\r\n\r\n";
			buffLen = fullMessage.size();
			strcpy(sendBuff, fullMessage.c_str());
			break;
		}

		case POST:
		{
			fullMessage = "HTTP/1.1 200 OK \r\nDate:";
			fullMessage += ctime(&currentTime);
			fullMessage += "\r\n\r\n";
			char* messagePtr = strstr(sockets[index].buffer, "\r\n\r\n"); // Skip to body content
			cout << "==================\nMessage received\n\n==================\n" 
				<< messagePtr + 4 << "\n==================\n\n";
			buffLen = fullMessage.size();
			strcpy(sendBuff, fullMessage.c_str());
			break;
		}
	}

	bytesSent = send(msgSocket, sendBuff, buffLen, 0);
	memset(sockets[index].buffer, 0, BUFF_SIZE);
	sockets[index].socketDataLen = 0;
	if (SOCKET_ERROR == bytesSent)
	{
		cout << "HTTP Server: Error at send(): " << WSAGetLastError() << endl;
		return false;
	}

	cout << "HTTP Server: Sent: " << bytesSent << "\\" << buffLen << " bytes of \n \"" << sendBuff << "\"\message.\n";
	sockets[index].send = IDLE;
	return true;
}

int PutRequest(int index, char* filename)
{
	char* tempPtr = 0;
	int buffLen = 0;
	int retCode = 200; // 'OK' code

	tempPtr = strtok(&sockets[index].buffer[5], " ");
	strcpy(filename, &sockets[index].buffer[5]);
	tempPtr = strtok(nullptr, ":");
	tempPtr = strtok(nullptr, ":");
	tempPtr = strtok(nullptr, " ");
	sscanf(tempPtr, "%d", &buffLen);

	fstream outPutFile;
	outPutFile.open(filename);

	if (!outPutFile.good())
	{
		outPutFile.open(filename, ios::out);
		retCode = 201; // New file created
	}

	if (!outPutFile.good())
	{
		cout << "HTTP Server: Error writing file to local storage: " << WSAGetLastError() << endl;
		return 0; // Error opening file
	}

	tempPtr = strtok(NULL, "\r\n\r\n");

	if (tempPtr == 0)
	{
		retCode = 204; // No content
	}
	else
	{
		while (*tempPtr != '\0') // Write to file
		{
			outPutFile << tempPtr;
			tempPtr += (strlen(tempPtr) + 1);
		}
	}

	outPutFile.close();
	return retCode;
}
/* logging_client.cpp : Defines the entry point for the console application.
*
* The app is used to connect to the webRTC logging server via TCP by specifying
* the remote IP and port.  After successfull connection the app prints out the
* data it receives from the server.
*
* Uses the following command line arguments to connect:
* -s: IP of the server to connect to
* -p: Port on the server to connect to. Defaults to 47002 which is assuming to be available.
*/

#include "stdafx.h"
#include <Ws2tcpip.h>
#include <winsock2.h>
#include <stdio.h>
#include <stdlib.h>

#define DEFAULT_PORT 47002  // Default port to connect to
#define BUFFER_SIZE 16384   // Buffer size for received messages

#pragma comment(lib, "ws2_32.lib")

int main(int argc, char **argv) {
  // Make sure the app receives required command line arguments
  if (argc == 0) {
    printf("Please, provide server IP and port number command line args to connect:\n");
    printf("    -s=<server IP>\n");
    printf("    -p=<port number>\n");
    return 1;
  }

  // Check the command line arguments and set the server IP/Port
  char  serverIP[128];        // IP of the server to connect to
  int   port = DEFAULT_PORT;  // Port on server to connect to
  for (int i = 1; i < argc; ++i) {
    if ((argv[i][0] == '-') || (argv[i][0] == '/')) {
      switch (tolower(argv[i][1])) {
      case 's': // Server
        if (strlen(argv[i]) > 3) {
          strcpy_s(serverIP, &argv[i][3]);
        }
        break;
      case 'p': // Remote port
        if (strlen(argv[i]) > 3) {
          port = atoi(&argv[i][3]);
        }
        break;
      default:
        break;
      }
    }
  }

  // Load the Winsock lib first
  WSADATA wsd;
  if (WSAStartup(MAKEWORD(2, 2), &wsd) != 0) {
    printf("Failed to load Winsock library.\n");
    return 1;
  }

  // Create TCP socket
  SOCKET socketClient;
  socketClient = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (socketClient == INVALID_SOCKET) {
    printf("socket() failed: %d\n", WSAGetLastError());
    return 1;
  }

  // Prepare the server info and connect
  struct sockaddr_in server;
  server.sin_family = AF_INET;
  server.sin_port = htons(port);
  inet_pton(AF_INET, serverIP, &server.sin_addr.s_addr);

  printf("Connecting to %s:%d\n", serverIP, port);
  if (connect(socketClient, (struct sockaddr *)&server, sizeof(server)) == SOCKET_ERROR) {
    printf("connect() failed: %d\n", WSAGetLastError());
    return 1;
  }

  // Open a file to write the logs into
  FILE * fileStream;
  errno_t err = fopen_s(&fileStream, "logs.txt", "w");
  if (err != 0) {
    printf("Could not open the log file.\n");
  }

  // Listen to messages from server
  char msgBuffer[BUFFER_SIZE];
  int ret;
  while (true) {
    ret = recv(socketClient, msgBuffer, BUFFER_SIZE, 0);
    if (ret == 0) {
      break;
    }
    else if (ret == SOCKET_ERROR) {
      printf("recv() failed: %d\n", WSAGetLastError());
      break;
    }

    // Print out the received info in console and file
    msgBuffer[ret] = '\0';
    printf("'%s'\n", msgBuffer);
    if (err == 0)  {
      fprintf(fileStream, "'%s'\n", msgBuffer);
    }
  }

  // Clean up resourses and return
  closesocket(socketClient);
  WSACleanup();
  fclose(fileStream);
  return 0;
}
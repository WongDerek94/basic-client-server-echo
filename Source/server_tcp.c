/*---------------------------------------------------------------------------------------
--	SOURCE FILE:	server_tcp.c - A simple echo server using TCP
--
--	PROGRAM:		tserver
--
--	FUNCTIONS:		init_server_control_channel (int *control_channel_socket, struct sockaddr_in *server, int server_len);
--					accept_client_connection (int *client_socket, int control_channel_socket, struct sockaddr_in *client);
--					receive_client_request (char *ack_request, int client_socket);
--					init_server_data_channel (int *data_channel_socket, struct sockaddr_in *server, int server_len);
--					process_request (char *ack_request, int data_channel_socket, struct sockaddr_in client, int *client_socket);
--					connect_with_retry (int socket, struct sockaddr *remote_entity, int remote_entity_len);
--					send_file (FILE *fp, int sockfd);
--					write_file(int sockfd);
--
--	DATE:			October 4, 2020
--
--	REVISIONS:		N/A
--
--
--	DESIGNERS:		Derek Wong
--
--	PROGRAMMERS:	Derek Wong
--
--	NOTES:
-- The program will accept TCP connections from client machines.
-- The program will read requests from clients (e.g., GET/SEND) and echo the commands back
-- A separate data channel port will be used to transfer files between the client and server
---------------------------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>

// Default ports
#define SERVER_CONTROL_CHANNEL_PORT		7005
#define SERVER_DATA_CHANNEL_PORT 		7006
#define CLIENT_DATA_CHANNEL_PORT		4612

//Buffer length
#define REQ_BUFLEN		80
#define FILE_BUFLEN		1024

// Default strings
#define GET_COMMAND_NAME		"GET"
#define SEND_COMMAND_NAME		"SEND"
#define SEND_FILE_NAME			"send.txt"
#define GET_FILE_NAME			"get.txt"

#define SERVER_IS_UP			1
#define TRUE					1
#define NOT_CONNECTED			1
#define DEFAULT_SLEEP_TIME		1

// Function prototypes
void init_server_control_channel (int *control_channel_socket, struct sockaddr_in *server, int server_len);
void accept_client_connection (int *client_socket, int control_channel_socket, struct sockaddr_in *client);
void receive_client_request (char *ack_request, int client_socket);
void init_server_data_channel (int *data_channel_socket, struct sockaddr_in *server, int server_len);
void process_request (char *ack_request, int data_channel_socket, struct sockaddr_in client, int *client_socket);
void connect_with_retry (int socket, struct sockaddr *remote_entity, int remote_entity_len);
void send_file (FILE *fp, int sockfd);
void write_file (int sockfd);

/*--------------------------------------------------------------------------
 * FUNCTION:       main
 *
 * DATE:           October 6th, 2020
 *
 * REVISIONS:      N/A
 *
 * DESIGNER:       Derek Wong
 *
 * PROGRAMMER:     Derek Wong
 *
 * INTERFACE:      int main (int argc, char **argv)
 *
 * RETURNS:        int
 *
 * NOTES:
 * Main entrypoint into client application
 * -----------------------------------------------------------------------*/
int main (int argc, char **argv)
{
	int	control_channel_socket, data_channel_socket, client_socket;
	struct	sockaddr_in server, client;
	char	ack_request[REQ_BUFLEN];
	
	init_server_control_channel(&control_channel_socket, &server, sizeof(server));

	while (SERVER_IS_UP)
	{
		accept_client_connection(&client_socket, control_channel_socket, &client);
		receive_client_request(ack_request, client_socket);
		init_server_data_channel(&data_channel_socket, &server, sizeof(server));
		
		client.sin_port = htons(CLIENT_DATA_CHANNEL_PORT);
		process_request(ack_request, data_channel_socket, client, &client_socket);
	}
	close(control_channel_socket);
	return(0);
}

// Function definitions

/*--------------------------------------------------------------------------
 * FUNCTION:       init_server_control_channel
 *
 * DATE:           October 6th, 2020
 *
 * REVISIONS:      N/A
 *
 * DESIGNER:       Derek Wong
 *
 * PROGRAMMER:     Derek Wong
 *
 * INTERFACE:      void init_server_control_channel (int *control_channel_socket, struct sockaddr_in *server, int server_len)
 *
 * RETURNS:        void
 *
 * NOTES:
 * Creates server control channel socket, binds address to socket and listens for connections
 * -----------------------------------------------------------------------*/
void init_server_control_channel (int *control_channel_socket, struct sockaddr_in *server, int server_len)
{
	// Create a control channel stream socket
	if ((*control_channel_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		perror ("Can't create a socket");
		exit(1);
	}
	printf("[+]Server control channel socket created successfully.\n");

	// Bind an address to the socket
	bzero((char *)server, sizeof(struct sockaddr_in));
	server->sin_family = AF_INET;
	server->sin_port = htons(SERVER_CONTROL_CHANNEL_PORT); 
	server->sin_addr.s_addr = htonl(INADDR_ANY); // Accept connections from any client

	if (bind(*control_channel_socket, (struct sockaddr *)server, server_len) == -1)
	{
		perror("Can't bind name to socket");
		exit(1);
	}
	printf("[+]Server control channel socket binded successfully.\n");
	
	
	// Listen for connections
	// queue up to 5 connect requests
	if (listen(*control_channel_socket, 5) == -1)
	{
		perror("[-]Error in listening");
		exit(1);
	}
}

/*--------------------------------------------------------------------------
 * FUNCTION:       accept_client_connection
 *
 * DATE:           October 6th, 2020
 *
 * REVISIONS:      N/A
 *
 * DESIGNER:       Derek Wong
 *
 * PROGRAMMER:     Derek Wong
 *
 * INTERFACE:      void accept_client_connection (int *client_socket, int control_channel_socket, struct sockaddr_in *client)
 *
 * RETURNS:        void
 *
 * NOTES:
 * Accepts client connects to the server control channel socket
 * -----------------------------------------------------------------------*/
void accept_client_connection (int *client_socket, int control_channel_socket, struct sockaddr_in *client)
{
	socklen_t client_len = sizeof(client);
	if ((*client_socket = accept (control_channel_socket, (struct sockaddr *)client, &client_len)) == -1)
	{
		fprintf(stderr, "Can't accept client\n");
		exit(1);
	}
	printf("[+]Client connected successfully.\n");

	printf("Client Address: %s\n", inet_ntoa(client->sin_addr));
}

/*--------------------------------------------------------------------------
 * FUNCTION:       receive_client_request
 *
 * DATE:           October 6th, 2020
 *
 * REVISIONS:      N/A
 *
 * DESIGNER:       Derek Wong
 *
 * PROGRAMMER:     Derek Wong
 *
 * INTERFACE:      void receive_client_request (char *ack_request, int client_socket)
 *
 * RETURNS:        void
 *
 * NOTES:
 * Receives a request containing a command in a buffer and reads it; Echo back command to the client and close client control socket
 * -----------------------------------------------------------------------*/
void receive_client_request (char *ack_request, int client_socket)
{
	int	n, bytes_to_read;
	char *request;
	request = ack_request;
		bytes_to_read = REQ_BUFLEN;
		while ((n = recv (client_socket, request, bytes_to_read, 0)) < REQ_BUFLEN)
		{
			request += n;
			bytes_to_read -= n;
		}
		printf ("Acknowledging Request:%s\n", ack_request);
		send (client_socket, ack_request, REQ_BUFLEN, 0);
		close (client_socket);
}

/*--------------------------------------------------------------------------
 * FUNCTION:       init_server_data_channel
 *
 * DATE:           October 6th, 2020
 *
 * REVISIONS:      N/A
 *
 * DESIGNER:       Derek Wong
 *
 * PROGRAMMER:     Derek Wong
 *
 * INTERFACE:      void init_server_data_channel (int *data_channel_socket, struct sockaddr_in *server, int server_len)
 *
 * RETURNS:        void
 *
 * NOTES:
 * Creates server data channel socket, binds address to socket
 * -----------------------------------------------------------------------*/
void init_server_data_channel (int *data_channel_socket, struct sockaddr_in *server, int server_len)
{
	// Create data channel stream socket
		if ((*data_channel_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1)
		{
			perror ("Can't create a socket");
			exit(1);
		}
		printf("[+]Server data channel socket created successfully.\n");
		
		// Bind an address to the socket
		bzero((char *)server, sizeof(struct sockaddr_in));
		server->sin_family = AF_INET;
		server->sin_port = htons(SERVER_DATA_CHANNEL_PORT); 
		server->sin_addr.s_addr = htonl(INADDR_ANY); // Accept connections from any client
		
		if (bind(*data_channel_socket, (struct sockaddr *)server, server_len) == -1)
		{
			perror("Can't bind name to socket");
			exit(1);
		}
		
		printf("[+]Server data channel socket binded successfully.\n");
}

/*--------------------------------------------------------------------------
 * FUNCTION:       process_request
 *
 * DATE:           October 6th, 2020
 *
 * REVISIONS:      N/A
 *
 * DESIGNER:       Derek Wong
 *
 * PROGRAMMER:     Derek Wong
 *
 * INTERFACE:      void process_request (char *ack_request, int data_channel_socket, struct sockaddr_in client, int *client_socket)
 *
 * RETURNS:        void
 *
 * NOTES:
 * Depending on what the acknowledged request command is, the server will either send/receive a file to/from client 
 * -----------------------------------------------------------------------*/
void process_request (char *ack_request, int data_channel_socket, struct sockaddr_in client, int *client_socket)
{
	socklen_t client_len = sizeof(client);
	FILE	*fp;
	// Send file to client
	if (strcmp(ack_request, GET_COMMAND_NAME) == 0)
	{
		connect_with_retry(data_channel_socket, (struct sockaddr *)&client, sizeof(client));

		printf("[+]Connected to client successfully.\n");
		fp = fopen(GET_FILE_NAME, "r");
		if (fp == NULL)
		{
			perror("[-]Error in reading file.");
			exit(1);
		}
		send_file(fp, data_channel_socket);
		printf("[+]File data sent successfully.\n");
		close(data_channel_socket);
		printf("[+]Closing the connection.\n\n");
	}
	// Retrieve file from client
	else if (strcmp(ack_request, SEND_COMMAND_NAME) == 0)
	{
		// Listen for client connections, when a connection is made transfer file over
		if (listen(data_channel_socket, 5) == -1)
		{
			perror("[-]Error in listening");
			exit(1);
		}
		if ((*client_socket = accept (data_channel_socket, (struct sockaddr *)&client, &client_len)) == -1)
		{
			perror("[-]Can't accept client connection");
			exit(1);
		}
		printf("[+]Client connected successfully.\n");
		printf("[+]Client Address:  %s\n", inet_ntoa(client.sin_addr));
		printf("[+]Server will now retrieve %s from client\n", SEND_FILE_NAME);
		write_file(*client_socket);
		printf("[+]Data written locally in the file, %s, successfully.\n", SEND_FILE_NAME);
		close(*client_socket);
		close(data_channel_socket);
		printf("[+]Closing the client and data channel socket connections.\n\n");
	}
}

/*--------------------------------------------------------------------------
 * FUNCTION:       connect_with_retry
 *
 * DATE:           October 6th, 2020
 *
 * REVISIONS:      N/A
 *
 * DESIGNER:       Derek Wong
 *
 * PROGRAMMER:     Derek Wong
 *
 * INTERFACE:      void connect_with_retry (int socket, struct sockaddr *remote_entity, int remote_entity_len)
 *
 * RETURNS:        void
 *
 * NOTES:
 * Utility function to help establish a connection between a remote entity and a client; Retries upon failure with a polling rate
 * -----------------------------------------------------------------------*/
void connect_with_retry (int socket, struct sockaddr *remote_entity, int remote_entity_len)
{
	// Connect to client, sleep when address is currently in use
	int sleep_time = DEFAULT_SLEEP_TIME;
	int interval = 1;
	
	while (NOT_CONNECTED)
	{
		if (connect (socket, remote_entity, remote_entity_len) == -1)
		{
			perror("[-]Can't connect to server");
			sleep(sleep_time);
			sleep_time += interval;
		} 
		else 
		{
			break;
		}
	}
}

/*--------------------------------------------------------------------------
 * FUNCTION:       send_file
 *
 * DATE:           October 6th, 2020
 *
 * REVISIONS:      N/A
 *
 * DESIGNER:       Derek Wong
 *
 * PROGRAMMER:     Derek Wong
 *
 * INTERFACE:      void send_file (FILE *fp, int sockfd)
 *
 * RETURNS:        void
 *
 * NOTES:
 * Sends file data through a specified socket to a remote entity
 * -----------------------------------------------------------------------*/
void send_file(FILE *fp, int sockfd){
  char data[FILE_BUFLEN] = {0};

  while(fgets(data, FILE_BUFLEN, fp) != NULL) {
    if (send(sockfd, data, sizeof(data), 0) == -1) {
      perror("[-]Error in sending file.");
      exit(1);
    }
    bzero(data, FILE_BUFLEN);
  }
}

/*--------------------------------------------------------------------------
 * FUNCTION:       write_file
 *
 * DATE:           October 6th, 2020
 *
 * REVISIONS:      N/A
 *
 * DESIGNER:       Derek Wong
 *
 * PROGRAMMER:     Derek Wong
 *
 * INTERFACE:      void write_file(int sockfd)
 *
 * RETURNS:        void
 *
 * NOTES:
 * Receives file data through a specified socket and file descriptor to write locally to a file called send.txt
 * -----------------------------------------------------------------------*/
void write_file (int sockfd)
{
  int n;
  FILE *fp;
  char *filename = SEND_FILE_NAME;
  char buffer[FILE_BUFLEN];

  fp = fopen(filename, "w");
  while (TRUE) {
    n = recv(sockfd, buffer, FILE_BUFLEN, 0);
    if (n <= 0){
      break;
      return;
    }
    fprintf(fp, "%s", buffer);
	fflush(fp);
    bzero(buffer, FILE_BUFLEN);
  }
  return;	
}

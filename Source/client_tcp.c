/*---------------------------------------------------------------------------------------
--	SOURCE FILE:	client_tcp.c
--
--	PROGRAM:		tclient
--
--	FUNCTIONS:		init_client_control_channel (int *client_socket, int option, struct sockaddr_in client);
--					connect_to_server (int client_socket, struct sockaddr_in server, struct hostent *hp);
--					connect_with_retry (int socket, struct sockaddr *remote_entity, int remote_entity_len);
--					send_request (int client_socket, char *request, char *ack_request);
--					init_client_data_channel (int *client_socket, int option, struct sockaddr_in *client, int client_len);
--					process_request (char *ack_request, int client_socket, struct sockaddr_in server, struct hostent *hp);
--					send_file (FILE *fp, int sockfd);
--					write_file(int sockfd);
--
--	DATE:			October 4, 2020
--
--	REVISIONS:		N/A

--
--	DESIGNERS:		Derek Wong
--
--	PROGRAMMERS:	Derek Wong
--
--	NOTES:
-- The program will establish a TCP connection to a user specifed server.
-- The server can be specified using a fully qualified domain name or and
-- IP address. After the connection has been established the client will
-- send the server a request that will be echoed back.  Once the request 
-- is confirmed, the client will open up a new TCP connection to the 
-- server with their respective data channel sockets to get or send a file. 
---------------------------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>

#include <arpa/inet.h>
#include <unistd.h>
#include <arpa/inet.h>

// Default ports
#define SERVER_CONTROL_CHANNEL_PORT		7005
#define CLIENT_CONTROL_CHANNEL_PORT		4611
#define SERVER_DATA_CHANNEL_PORT		7006
#define CLIENT_DATA_CHANNEL_PORT		4612

// Buffer length
#define REQ_BUFLEN			80
#define FILE_BUFLEN			1024

// Default strings
#define GET_COMMAND_NAME		"GET"
#define SEND_COMMAND_NAME		"SEND"
#define SEND_FILE_NAME			"send.txt"
#define GET_FILE_NAME			"get.txt"

#define TRUE					1
#define NOT_CONNECTED			1
#define DEFAULT_SLEEP_TIME		1

// Function prototypes
void init_client_control_channel (int *client_socket, int option, struct sockaddr_in client);
void connect_to_server (int client_socket, struct sockaddr_in server, struct hostent *hp);
void connect_with_retry (int socket, struct sockaddr *remote_entity, int remote_entity_len);
void send_request (int client_socket, char *request, char *ack_request);
void init_client_data_channel (int *client_socket, int option, struct sockaddr_in *client, int client_len);
void process_request (char *ack_request, int client_socket, struct sockaddr_in server, struct hostent *hp);
void send_file (FILE *fp, int sockfd);
void write_file(int sockfd);

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
	int 		client_socket = 0, option = 1;
	struct 		hostent	*hp = NULL;
	struct 		sockaddr_in server = {0}, client = {0};
	char  		*host = NULL;
	char 		request[REQ_BUFLEN], ack_request[REQ_BUFLEN];

	// Get user parameters
	switch(argc)
	{
		case 3:
			// Get server IP either using FQDN or IP address
			host =	argv[1];
			if ((hp = gethostbyname(host)) == NULL)
			{
				perror("[-]Unknown server address");
				exit(1);
			}
			printf("[+]Host found.\n");
			// Validate request commands are valid
			if (strcmp(argv[2], GET_COMMAND_NAME) == 0 || strcmp(argv[2], SEND_COMMAND_NAME) == 0) 
			{
				strcpy(request, argv[2]);
			} 
			else 
			{
				fprintf(stderr, "Usage: %s host {GET,SEND}\n", argv[0]);
				exit(1);
			}
		break;
		default:
			fprintf(stderr, "Usage: %s host {GET,SEND}\n", argv[0]);
			exit(1);
	}
	
	init_client_control_channel(&client_socket, option, client);
	connect_to_server (client_socket, server, hp);
	send_request(client_socket, request, ack_request);
	init_client_data_channel(&client_socket, option, &client, sizeof(client));
	process_request (ack_request, client_socket, server, hp);
	close (client_socket);
	
	return (0);
}

// Function definitions

/*--------------------------------------------------------------------------
 * FUNCTION:       init_client_control_channel
 *
 * DATE:           October 6th, 2020
 *
 * REVISIONS:      N/A
 *
 * DESIGNER:       Derek Wong
 *
 * PROGRAMMER:     Derek Wong
 *
 * INTERFACE:      void init_client_control_channel (int *client_socket, int option, struct sockaddr_in client)
 *
 * RETURNS:        void
 *
 * NOTES:
 * Creates client control channel socket, sets socket options to allow for reuseable addreses, binds address to socket
 * -----------------------------------------------------------------------*/
void init_client_control_channel (int *client_socket, int option, struct sockaddr_in client)
{
	// Create the socket
	if ((*client_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		perror("[-]Cannot create socket");
		exit(1);
	}
	printf("[+]Client socket created successfully.\n");
	
	// Set Socket Options
	if(setsockopt(*client_socket, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option)) < 0)
	{
		perror("[-]setsockopt failed");
		exit(1);
	}
	
	// Bind an address to the socket
	bzero((char *)&client, sizeof(struct sockaddr_in));
	client.sin_family = AF_INET;
	client.sin_port = htons(CLIENT_CONTROL_CHANNEL_PORT); 
	client.sin_addr.s_addr = htonl(INADDR_ANY); // Accept connections from any server
	
	// Bind address to socket
	if (bind(*client_socket, (struct sockaddr *)&client, sizeof(client)) == -1)
	{
		perror("[-]Can't bind name to socket");
		exit(1);
	}
	printf("[+]Client socket binded successfully.\n");
}

/*--------------------------------------------------------------------------
 * FUNCTION:       connect_to_server
 *
 * DATE:           October 6th, 2020
 *
 * REVISIONS:      N/A
 *
 * DESIGNER:       Derek Wong
 *
 * PROGRAMMER:     Derek Wong
 *
 * INTERFACE:      void connect_to_server (int client_socket, struct sockaddr_in server,  struct hostent *hp)
 *
 * RETURNS:        void
 *
 * NOTES:
 * Create a connection from the client control socket port to the server control port
 * -----------------------------------------------------------------------*/
void connect_to_server (int client_socket, struct sockaddr_in server,  struct hostent *hp)
{
	bzero((char *)&server, sizeof(struct sockaddr_in));
	server.sin_family = AF_INET;
	server.sin_port = htons(SERVER_CONTROL_CHANNEL_PORT);
	
	bcopy(hp->h_addr, (char *)&server.sin_addr, hp->h_length);

	// Connecting to the server
	connect_with_retry(client_socket, (struct sockaddr *)&server, sizeof(server));
	printf("[+]Connected to server successfully.\n");
	printf("[+]Connected:\tServer Name: %s\n", hp->h_name);
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
 * FUNCTION:       send_request
 *
 * DATE:           October 6th, 2020
 *
 * REVISIONS:      N/A
 *
 * DESIGNER:       Derek Wong
 *
 * PROGRAMMER:     Derek Wong
 *
 * INTERFACE:      void send_request (int client_socket, char *request, char *ack_request)
 *
 * RETURNS:        void
 *
 * NOTES:
 * Sends a request with a command in a buffer to be read by the server; Receives an echo of acknoledged command 
 * -----------------------------------------------------------------------*/
void send_request (int client_socket, char *request, char *ack_request)
{
	int n =0, bytes_to_read = 0;
	char *bp = NULL;
	
	// Transmit data through the socket
	printf("[+]Transmitting command %s\n", request);
	int bytes_sent = send(client_socket, request, REQ_BUFLEN, 0);
	printf("[+]Sent %d bytes.\n", bytes_sent);

	// Client makes repeated calls to recv until no more data is expected to arrive.
	bp = ack_request;
	bytes_to_read = REQ_BUFLEN;
	n = 0;
	while ((n = recv (client_socket, bp, bytes_to_read, 0)) < REQ_BUFLEN)
	{
		bp += n;
		bytes_to_read -= n;
	}
	
	printf("[+]Received %d bytes.\n", bytes_to_read);
	printf ("[+]%s command received.\n", ack_request);
	
	close (client_socket);	
}

/*--------------------------------------------------------------------------
 * FUNCTION:       init_client_control_channel
 *
 * DATE:           October 6th, 2020
 *
 * REVISIONS:      N/A
 *
 * DESIGNER:       Derek Wong
 *
 * PROGRAMMER:     Derek Wong
 *
 * INTERFACE:      void init_client_data_channel (int *client_socket, int option, struct sockaddr_in *client, int client_len)
 *
 * RETURNS:        void
 *
 * NOTES:
 * Creates client data channel socket, sets socket options to allow for reuseable addreses, binds address to socket
 * -----------------------------------------------------------------------*/
void init_client_data_channel (int *client_socket, int option, struct sockaddr_in *client, int client_len)
{
	// Create the socket
	if ((*client_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		perror("[-]Cannot create socket");
		exit(1);
	}
	printf("[+]Client socket created successfully.\n");
	
	// Set Socket Options
	if(setsockopt(*client_socket, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option)) < 0)
	{
		perror("[-]setsockopt failed");
		exit(1);
	}
	
	// Bind an address to the socket
	bzero((char *)client, sizeof(struct sockaddr_in));
	client->sin_family = AF_INET;
	client->sin_port = htons(CLIENT_DATA_CHANNEL_PORT); 
	client->sin_addr.s_addr = htonl(INADDR_ANY); // Accept connections from any server
	
	// Bind address to socket
	if (bind(*client_socket, (struct sockaddr *)client, client_len) == -1)
	{
		perror("[-]Can't bind name to socket");
		exit(1);
	}
	printf("[+]Client socket binded successfully.\n");
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
 * INTERFACE:      void process_request (char *ack_request, int client_socket, struct sockaddr_in server, struct hostent *hp)
 *
 * RETURNS:        void
 *
 * NOTES:
 * Depending on what the acknowledged request command is, the client will either send/receive a file to/from server
 * -----------------------------------------------------------------------*/
void process_request (char *ack_request, int client_socket, struct sockaddr_in server, struct hostent *hp)
{
	// Retrieve file from server
	if (strcmp(ack_request, GET_COMMAND_NAME) == 0) 
	{
		if(listen(client_socket, 5) == -1)
		{
			perror("[-]Error in listening");
			exit(1);
		}
		
		socklen_t server_len = sizeof(server);
		int data_channel_socket = 0;
		if ((data_channel_socket = accept (client_socket, (struct sockaddr *)&server, &server_len)) == -1)
		{
			perror("[-]Can't accept server connection");
			exit(1);
		}
		printf("[+]Server connected successfully.\n");
		printf("[+]Server Address:  %s\n", inet_ntoa(server.sin_addr));
		printf("[+]Client will now retrieve %s from server\n", GET_FILE_NAME);
		write_file(data_channel_socket);
		printf("[+]Data written locally in the file, %s, successfully.\n", GET_FILE_NAME);
		close(data_channel_socket);
		close(client_socket);
	}
	// Send file to server
	else if (strcmp(ack_request, SEND_COMMAND_NAME) == 0)
	{
		FILE *fp = NULL;
		
		bzero((char *)&server, sizeof(struct sockaddr_in));
		server.sin_family = AF_INET;
		server.sin_port = htons(SERVER_DATA_CHANNEL_PORT);
		bcopy(hp->h_addr, (char *)&server.sin_addr, hp->h_length);
		
		
		// Connect to server
		connect_with_retry(client_socket, (struct sockaddr *)&server, sizeof(server));
		printf("[+]Connected to server successfully.\n");
		printf("[+]Server Address:  %s\n", inet_ntoa(server.sin_addr));
		printf("[+]Client will now send %s to Server\n", SEND_FILE_NAME);
		
		fp = fopen(SEND_FILE_NAME, "r");
		if (fp == NULL)
		{
			perror("[-]Error in reading file.");
			exit(1);
		}
		send_file(fp, client_socket);
		printf("[+]File data sent successfully.\n");
		close(client_socket);
		printf("[+]Closing the connection.\n\n");
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
void send_file (FILE *fp, int sockfd)
{
  char data[FILE_BUFLEN] = {0};

  while (fgets(data, FILE_BUFLEN, fp) != NULL) {
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
 * Receives file data through a specified socket and file descriptor to write locally to a file called get.txt
 * -----------------------------------------------------------------------*/
void write_file(int sockfd)
{
  int n;
  FILE *fp;
  char *filename = GET_FILE_NAME;
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
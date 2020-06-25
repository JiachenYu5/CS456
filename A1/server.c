#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>

// Get help from tutorials in the following two sites:
// http://www.tutorialspoint.com/unix_sockets/socket_server_example.htm
// http://www.linuxhowtos.org/C_C++/socket.htm

// help function that reverse a string 
void reverseString(char *str) {
    int i = strlen(str) - 1, j = 0;
    char ch;
    while (i > j)
    {
        ch = str[i];
        str[i] = str[j];
        str[j] = ch;
        i--;
        j++;
    }
}

int main(int argc, char* argv[]) {
    // create variables
    int serverTCP_fileDescriptor, serverUDP_fileDescriptor, clientTCP_fileDescriptor;
    int r_portNum, reqCode;
    int n_portNum = 10000;          // define negotiation port number here
    struct sockaddr_in serverTCP_addr, serverUDP_addr, clientTCP_addr, clientUDP_addr;
    char message_buffer_send[1024];
    char message_buffer_read[1024];
    int result;

    // check if the number of commandline inputs is correct
    if (argc != 2) {
        fprintf(stderr, "Incorrect number of arguments\n");
        exit(EXIT_FAILURE);
    }
    // read commandline input
    reqCode = atoi(argv[1]);
    // print negotiation port message
    fprintf(stdout, "SERVER_PORT=%d\n", n_portNum);
    FILE *f = fopen("port.txt", "w");
    fprintf(f, "SERVER_PORT=%d\n", n_portNum);

//  Stage 1 TCP
    // open server socket
    serverTCP_fileDescriptor = socket(AF_INET, SOCK_STREAM, 0);
    bzero((char *) &serverTCP_addr, sizeof(serverTCP_addr));
    serverTCP_addr.sin_family = AF_INET;
    serverTCP_addr.sin_addr.s_addr = INADDR_ANY;
    serverTCP_addr.sin_port = htons(n_portNum);

    // bind server address with the TCP socket
    bind(serverTCP_fileDescriptor, (struct sockaddr *) &serverTCP_addr, sizeof(serverTCP_addr));
    // close port.txt
    fclose(f);

    // waiting for client connection    
    listen(serverTCP_fileDescriptor, 3);
    socklen_t clientTCPSize = sizeof(clientTCP_addr);
    // accept client connection request
    clientTCP_fileDescriptor = accept(serverTCP_fileDescriptor, (struct sockaddr *)&clientTCP_addr, &clientTCPSize);

    // read the request code sent by client and check if they match 
    bzero(message_buffer_read,1024);
    result = read(clientTCP_fileDescriptor, message_buffer_read, 1023);
    result = atoi(message_buffer_read);
    if (result != reqCode) {
        fprintf(stderr, "Request code doesn't match, close server.");
        close(clientTCP_fileDescriptor);
        close(serverTCP_fileDescriptor);
        exit(EXIT_FAILURE);
    }

    // create random port number for transfer data
    r_portNum = rand() % 1025 + 1024;
    snprintf(message_buffer_send, 1024, "%d", r_portNum);
    // send the random port number to client
    result = write(clientTCP_fileDescriptor, message_buffer_send, 1024);


//  Stage 2 UDP
    // create server UDP socket
    serverUDP_fileDescriptor = socket(AF_INET, SOCK_DGRAM, 0);
    bzero((char *) &serverUDP_addr, sizeof(serverUDP_addr));
    serverUDP_addr.sin_family = AF_INET;
    serverUDP_addr.sin_addr.s_addr = INADDR_ANY;
    serverUDP_addr.sin_port = htons(r_portNum);

    // bind server address with the UDP socket
    bind(serverUDP_fileDescriptor, (struct sockaddr *) &serverUDP_addr, sizeof(serverUDP_addr));

    // read message from client
    socklen_t clientUDPSize = sizeof(clientUDP_addr);
    result = recvfrom(serverUDP_fileDescriptor, (char *)message_buffer_read, 1024, MSG_WAITALL, (struct sockaddr *)&clientUDP_addr, &clientUDPSize);
    message_buffer_read[result] = '\0';

    // reverse the string message
    reverseString(message_buffer_read);

    // send the reversed message back to client
    snprintf(message_buffer_send, 1024, "%s", message_buffer_read);
    sendto(serverUDP_fileDescriptor, (const char *)message_buffer_send, 1024, 0, (const struct sockaddr *) &clientUDP_addr, clientUDPSize);
        
    // close UDP socket
    close(serverUDP_fileDescriptor);
}

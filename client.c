#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <netdb.h>

// Get help from tutorials in the following two sites:
// http://www.tutorialspoint.com/unix_sockets/socket_server_example.htm
// http://www.linuxhowtos.org/C_C++/socket.htm

int main(int argc, char* argv[]) {
    // create variables
    int clientTCP_fileDescriptor, clientUDP_fileDescriptor;
    int n_portNum, r_portNum, reqCode;
    struct sockaddr_in serverTCP_addr, serverUDP_addr;
    char message_buffer_send[1024];
    char message_buffer_read[1024];
    int result;
    struct hostent* serverName;

    // check if the number of commandline inputs is correct
    if ( argc != 5 ) {
        fprintf(stderr, "Incorrect number of arguments\n");
        exit(EXIT_FAILURE);
    }

    // read commandline inputs
    serverName = gethostbyname(argv[1]);
    n_portNum = atoi(argv[2]);
    reqCode = atoi(argv[3]);

//  Stage 1 TCP
    // create client TCP socket
    clientTCP_fileDescriptor = socket(AF_INET, SOCK_STREAM, 0);
    bzero((char *) &serverTCP_addr, sizeof(serverTCP_addr));
    serverTCP_addr.sin_family = AF_INET;
    bcopy((char *)serverName->h_addr, (char *)&serverTCP_addr.sin_addr.s_addr, serverName->h_length);
    serverTCP_addr.sin_port = htons(n_portNum);

    // connect to the server
    connect(clientTCP_fileDescriptor, (struct sockaddr*) &serverTCP_addr, sizeof(serverTCP_addr));

    // send request code to the server
    bzero(message_buffer_send, 1024);
    snprintf(message_buffer_send, 1024, "%d", reqCode);
    result = write(clientTCP_fileDescriptor, message_buffer_send, 1024);

    // read random port number from the server
    bzero(message_buffer_send, 1024);
    result = read(clientTCP_fileDescriptor, message_buffer_read, 1023);
    r_portNum = atoi(message_buffer_read);

    // close client TCP socket
    close(clientTCP_fileDescriptor);
      

//  Stage 2 UDP
    // create client UDP socket
    (clientUDP_fileDescriptor = socket(AF_INET, SOCK_DGRAM, 0));
    bzero((char *) &serverUDP_addr, sizeof(serverUDP_addr));
    serverUDP_addr.sin_family = AF_INET;
    bcopy((char *)serverName->h_addr, (char *)&serverUDP_addr.sin_addr.s_addr, serverName->h_length);
    serverUDP_addr.sin_port = htons(r_portNum);     

    // send message to server
    bzero(message_buffer_send, 1024);
    snprintf(message_buffer_send, 1024, "%s", argv[4]);
    sendto(clientUDP_fileDescriptor, (const char *)message_buffer_send, 1024, 0, (const struct sockaddr *) &serverUDP_addr, sizeof(serverUDP_addr));  

    // read the response from server
    socklen_t len;
    bzero(message_buffer_read, 1024);      
    result = recvfrom(clientUDP_fileDescriptor, (char *)message_buffer_read, 1024, 0, (struct sockaddr *) &serverUDP_addr, &len); 
    message_buffer_read[result] = '\0';

    // print out the reversed message
    puts(message_buffer_read);

    // close client UDP socket
    close(clientUDP_fileDescriptor);
}

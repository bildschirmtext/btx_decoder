#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <stdint.h>
#include <sys/socket.h>
#include <netinet/in.h>

/*
 * The original Bildschirmtext service over modem supported
 * error correction in the form of checksummed sequences and
 * a ACK or NACK and resend feature.
 * This layer 2 interface connects using TCP/IP, so the server
 * does not send any error correction information, and this
 * code does not support receiving any.
 */

int sockfd=-1;
unsigned char last_char;
int is_last_char_buffered = 0;

#define BSIZE (1024*1024)

uint8_t buffer[BSIZE];
int rpointer=0;
int wpointer=0;

#define HOST "localhost"
//#define HOST "belgradstr.dyndns.org"
#define PORT 20000 /* XXX the original port for CEPT is 20005 */

void layer2_connect() 
{
	layer2_connect2(HOST,PORT);
}

void layer2_connect2(const char *host, const int port)
{
    struct hostent *he;
    struct sockaddr_in their_addr;
    
    if ((he = gethostbyname(host)) == NULL) {
        herror("gethostbyname");
        exit(1);
    }
    
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(1);
    }
    
    their_addr.sin_family = AF_INET;
    their_addr.sin_port = htons(port);
    their_addr.sin_addr = *((struct in_addr *)he->h_addr);
    bzero(&(their_addr.sin_zero), 8);
    
    if (connect(sockfd, (struct sockaddr *)&their_addr, sizeof(struct sockaddr)) == -1) {
        perror("connect");
        exit(1);
    }
}


int layer2_write_readbuffer(uint8_t c)
{
	int nwpointer=(wpointer+1)%BSIZE;
	if (nwpointer==rpointer) return -1; //No space left in buffer
	buffer[wpointer]=c;
	wpointer=nwpointer;
	return 0;
}

int layer2_eof()
{
	if (wpointer==rpointer) return 0;
	return 1;
}

int layer2_getc()
{
    if (is_last_char_buffered) {
        is_last_char_buffered = 0;
    } else {
	if (sockfd<0) {
		if (rpointer==wpointer) {
			return -1; //EOF or no data left
		}
		last_char=buffer[rpointer];
		rpointer=(rpointer+1)%BSIZE;

	} else {
		ssize_t numbytes = recv(sockfd, &last_char, 1, 0);
		if (numbytes==0) usleep(100000);
		if (numbytes <0) {
		    return -1;
		}
	}
    }
    return last_char;
}

void layer2_ungetc()
{
    is_last_char_buffered = 1;
}

void layer2_write(const unsigned char *s, unsigned int len)
{
	if (sockfd<0) return;
    if (send(sockfd, s, len, 0) == -1){
        perror("send");
        exit (1);
    }
}


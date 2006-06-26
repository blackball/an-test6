#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "solvedfile.h"

static struct sockaddr_in serveraddr = {0, 0, {0}};

int solvedserver_set_server(char* addr) {
	char buf[256];
	char* ind;
	struct hostent* he;
	int len;
	int port;
	ind = index(addr, ':');
	if (!ind) {
		fprintf(stderr, "Invalid IP:port address: %s\n", addr);
		return -1;
	}
	len = ind - addr;
	memcpy(buf, addr, len);
	buf[len] = '\0';
	he = gethostbyname(buf);
	if (!he) {
		fprintf(stderr, "Solved server \"%s\" not found: %s.\n", buf, hstrerror(h_errno));
		return -1;
	}
	memcpy(&(serveraddr.sin_addr), he->h_addr, he->h_length);
	port = atoi(ind+1);
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons(port);
	return 0;
}

int solvedserver_get(int filenum, int fieldnum) {
	FILE* f;
	char buf[256];
	const char* solvedstr = "solved";
	int solved;
	int sock = socket(PF_INET, SOCK_STREAM, 0);
	if (sock == -1) {
 		fprintf(stderr, "Couldn't create socket: %s\n", strerror(errno));
		return -1;
	}
	f = fdopen(sock, "r+b");
	if (!f) {
		fprintf(stderr, "Failed to fdopen socket: %s\n", strerror(errno));
		return -1;
	}
	if (connect(sock, (struct sockaddr*)&serveraddr, sizeof(serveraddr))) {
 		fprintf(stderr, "Couldn't connect to server: %s\n", strerror(errno));
		return -1;
	}
	fprintf(f, "get %i %i\n", filenum, fieldnum);
	fflush(f);
	if (!fgets(buf, 256, f)) {
		fprintf(stderr, "Couldn't read response: %s\n", strerror(errno));
		fclose(f);
		return -1;
	}
	solved = (strncmp(buf, solvedstr, strlen(solvedstr)) == 0);
	fclose(f);
	return solved;
}

void solvedserver_set(int filenum, int fieldnum) {
	FILE* f;
	int sock = socket(PF_INET, SOCK_STREAM, 0);
	if (sock == -1) {
 		fprintf(stderr, "Couldn't create socket: %s\n", strerror(errno));
		return;
	}
	f = fdopen(sock, "wb");
	if (!f) {
		fprintf(stderr, "Failed to fdopen socket: %s\n", strerror(errno));
		return;
	}
	if (connect(sock, (struct sockaddr*)&serveraddr, sizeof(serveraddr))) {
 		fprintf(stderr, "Couldn't connect to server: %s\n", strerror(errno));
		return;
	}
	fprintf(f, "set %i %i\n", filenum, fieldnum);
	fflush(f);
	fclose(f);
}


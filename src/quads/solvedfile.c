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
	struct hostent* he = gethostbyname(addr);
	if (!he) {
		fprintf(stderr, "Solved server not found: %s\n", hstrerror(h_errno));
		return -1;
	}
	memcpy(&serveraddr, he->h_addr, he->h_length);
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


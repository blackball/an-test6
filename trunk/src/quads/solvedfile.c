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
static FILE* fserver = NULL;

int solvedserver_set_server(char* addr) {
	char buf[256];
	char* ind;
	struct hostent* he;
	int len;
	int port;
	if (fserver) {
		if (fflush(fserver) ||
			fclose(fserver)) {
			fprintf(stderr, "Failed to close previous connection to server.\n");
		}
		fserver = NULL;
	}
	if (!addr)
		return -1;
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

static int connect_to_server() {
	int sock;
	if (fserver)
		return 0;
	sock = socket(PF_INET, SOCK_STREAM, 0);
	if (sock == -1) {
		fprintf(stderr, "Couldn't create socket: %s\n", strerror(errno));
		return -1;
	}
	fserver = fdopen(sock, "r+b");
	if (!fserver) {
		fprintf(stderr, "Failed to fdopen socket: %s\n", strerror(errno));
		return -1;
	}
	if (connect(sock, (struct sockaddr*)&serveraddr, sizeof(serveraddr))) {
		fprintf(stderr, "Couldn't connect to server: %s\n", strerror(errno));
		fserver = NULL;
		return -1;
	}
	return 0;
}

int solvedserver_get(int filenum, int fieldnum) {
	char buf[256];
	const char* solvedstr = "solved";
	int nchars;
	int solved;

	if (connect_to_server())
		return -1;
	nchars = sprintf(buf, "get %i %i\n", filenum, fieldnum);
	if ((fwrite(buf, 1, nchars, fserver) != nchars) ||
		fflush(fserver)) {
		fprintf(stderr, "Failed to write request to server: %s\n", strerror(errno));
		fclose(fserver);
		fserver = NULL;
		return -1;
	}
	if (!fgets(buf, 256, fserver)) {
		fprintf(stderr, "Couldn't read response: %s\n", strerror(errno));
		fclose(fserver);
		fserver = NULL;
		return -1;
	}
	solved = (strncmp(buf, solvedstr, strlen(solvedstr)) == 0);
	return solved;
}

void solvedserver_set(int filenum, int fieldnum) {
	char buf[256];
	int nchars;
	if (connect_to_server())
		return;
	nchars = sprintf(buf, "set %i %i\n", filenum, fieldnum);
	if ((fwrite(buf, 1, nchars, fserver) != nchars) ||
		fflush(fserver)) {
		fprintf(stderr, "Failed to send command (%s) to solvedserver: %s\n", buf, strerror(errno));
		return;
	}
	// wait for response.
	if (!fgets(buf, 256, fserver)) {
		fprintf(stderr, "Couldn't read response: %s\n", strerror(errno));
		fclose(fserver);
		fserver = NULL;
		return;
	}
}


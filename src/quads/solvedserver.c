#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/select.h>
#include <fcntl.h>

#include "bl.h"
#include "solvedfile.h"

const char* OPTIONS = "hp:f:";

void printHelp(char* progname) {
	fprintf(stderr, "Usage: %s\n"
			"   [-p <port>] (default 6789)\n"
			"   [-f <filename-pattern>]  (default solved.%%02i)\n",
			progname);
}

int bailout = 0;
char* solvedfnpattern = "solved.%02i";

static void sighandler(int sig) {
	bailout = 1;
}

extern char *optarg;
extern int optind, opterr, optopt;

int handle_request(FILE* fid) {
	char buf[256];
	char fn[256];
	char getsetstr[16];
	int set;
	int get;
	int filenum;
	int fieldnum;

	printf("Fileno %i:\n", fileno(fid));
	if (!fgets(buf, 256, fid)) {
		fprintf(stderr, "Error: failed to read a line of input.\n");
		fflush(stderr);
		fclose(fid);
		return -1;
	}
	//printf("Got request %s\n", buf);
	if (sscanf(buf, "%4s %i %i\n", getsetstr, &filenum, &fieldnum) != 3) {
		fprintf(stderr, "Error: malformed request: %s\n", buf);
		fflush(stderr);
		fclose(fid);
		return -1;
	}

	get = (strcmp(getsetstr, "get") == 0);
	set = (strcmp(getsetstr, "set") == 0);

	sprintf(fn, solvedfnpattern, filenum);

	if (get) {
		int val;
		printf("Get %s [%i].\n", fn, fieldnum);
		val = solvedfile_get(fn, fieldnum);
		if (val == -1) {
			fclose(fid);
			return -1;
		} else {
			fprintf(fid, "%s %i %i\n", (val ? "solved" : "unsolved"),
					filenum, fieldnum);
			fflush(fid);
		}
		return 0;
	} else if (set) {
		printf("Set %s [%i].\n", fn, fieldnum);
		if (solvedfile_set(fn, fieldnum)) {
			fclose(fid);
			return -1;
		}
		fprintf(fid, "ok\n");
		fflush(fid);
		return 0;
		/*
		  sprintf(buf, "ok\n");
		  len = strlen(buf);
		  if ((fwrite(buf, 1, len, fid) != len) ||
		  fflush(fid)) {
		  fprintf(stderr, "Error writing set confirmation.\n");
		  goto bailout;
		  }
		*/
	} else {
		fprintf(stderr, "Error: malformed command.\n");
		fclose(fid);
		return -1;
	}
}

int main(int argc, char** args) {
    int argchar;
	char* progname = args[0];
	int sock;
	struct sockaddr_in addr;
	int port = 6789;
	unsigned int opt;
	pl* clients;
	int flags;

    while ((argchar = getopt (argc, args, OPTIONS)) != -1) {
		switch (argchar) {
		case 'p':
			port = atoi(optarg);
			break;
		case 'f':
			solvedfnpattern = optarg;
			break;
		case 'h':
		default:
			printHelp(progname);
			exit(-1);
		}
	}

	sock = socket(PF_INET, SOCK_STREAM, 0);
	if (sock == -1) {
 		fprintf(stderr, "Error: couldn't create socket: %s\n", strerror(errno));
		exit(-1);
	}

	opt = 1;
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
		fprintf(stderr, "Warning: failed to setsockopt() to reuse address.\n");
	}

	flags = fcntl(sock, F_GETFL, 0);
	if (flags == -1) {
		fprintf(stderr, "Warning: failed to get socket flags: %s\n",
					strerror(errno));
	} else {
		flags |= O_NONBLOCK;
		if (fcntl(sock, F_SETFL, flags) == -1) {
			fprintf(stderr, "Warning: failed to set socket flags: %s\n",
					strerror(errno));
		}
	}

	memset(&addr, 0, sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(port);
	if (bind(sock, (struct sockaddr*)&addr, sizeof(struct sockaddr_in))) {
		fprintf(stderr, "Error: couldn't bind socket: %s\n", strerror(errno));
		exit(-1);
	}

	if (listen(sock, 1000)) {
 		fprintf(stderr, "Error: failed to listen() on socket: %s\n", strerror(errno));
		exit(-1);
	}
	printf("Listening on port %i.\n", port);
	fflush(stdout);

	signal(SIGINT, sighandler);

	clients = pl_new(32);

	// wait for a connection or i/o...
	while (1) {
		struct sockaddr_in clientaddr;
		socklen_t addrsz = sizeof(clientaddr);
		FILE* fid;
		fd_set rset;
		//fd_set wset;
		//fd_set eset;
		struct timeval timeout;
		int res;
		int maxval = 0;
		int i;

		timeout.tv_sec = 1;
		timeout.tv_usec = 0;

		FD_ZERO(&rset);
		//FD_ZERO(&wset);
		//FD_ZERO(&eset);

		maxval = sock;
		for (i=0; i<pl_size(clients); i++) {
			int val;
			fid = pl_get(clients, i);
			val = fileno(fid);
			FD_SET(val, &rset);
			//FD_SET(val, &eset);
			if (val > maxval)
				maxval = val;
		}
		FD_SET(sock, &rset);
		//FD_SET(sock, &eset);
		printf("select().\n");
		res = select(maxval+1, &rset, NULL, NULL, /*&eset,*/ &timeout);
		if (res == -1) {
			if (errno != EINTR) {
				fprintf(stderr, "Error: select(): %s\n", strerror(errno));
				exit(-1);
			}
		}
		if (bailout)
			break;
		if (!res)
			continue;

		for (i=0; i<pl_size(clients); i++) {
			fid = pl_get(clients, i);
			/*
			  if (FD_ISSET(fileno(fid), &eset)) {
			  fprintf(stderr, "Error from fileno %i\n", fileno(fid));
			  if (ferror(fid) || feof(fid)) {
			  pl_remove(clients, i);
			  i--;
			  continue;
			  }
			  }
			*/
			if (FD_ISSET(fileno(fid), &rset)) {
				if (handle_request(fid)) {
					fprintf(stderr, "Error from fileno %i\n", fileno(fid));
					pl_remove(clients, i);
					i--;
					continue;
				}
			}
		}
		if (FD_ISSET(sock, &rset)) {
			int s = accept(sock, (struct sockaddr*)&clientaddr, &addrsz);
			if (s == -1) {
				fprintf(stderr, "Error: failed to accept() on socket: %s\n", strerror(errno));
				continue;
			}
			if (addrsz != sizeof(clientaddr)) {
				fprintf(stderr, "Error: client address has size %i, not %i.\n", addrsz, sizeof(clientaddr));
				continue;
			}
			printf("Connection from %s: ", inet_ntoa(clientaddr.sin_addr));
			fflush(stdout);
			fid = fdopen(s, "a+b");
			printf("fileno %i\n", fileno(fid));
			pl_append(clients, fid);
		}
	}

	printf("Closing socket...\n");
	if (close(sock)) {
		fprintf(stderr, "Error: failed to close socket: %s\n", strerror(errno));
	}

	return 0;
}


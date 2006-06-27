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

const char* OPTIONS = "hp:f:";

void printHelp(char* progname) {
	fprintf(stderr, "Usage: %s\n"
			"   [-p <port>] (default 6789)\n"
			"   [-f <filename-pattern>]  (default solved.%%02i)\n",
			progname);
}

int bailout = 0;

static void sighandler(int sig) {
	bailout = 1;
}

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char** args) {
    int argchar;
	char* progname = args[0];
	int sock;
	struct sockaddr_in addr;
	int port = 6789;
	char* solvedfnpattern = "solved.%02i";
	unsigned int opt;

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

	memset(&addr, 0, sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(port);
	if (bind(sock, (struct sockaddr*)&addr, sizeof(struct sockaddr_in))) {
		fprintf(stderr, "Error: couldn't bind socket: %s\n", strerror(errno));
		exit(-1);
	}

	if (listen(sock, 100)) {
 		fprintf(stderr, "Error: failed to listen() on socket: %s\n", strerror(errno));
		exit(-1);
	}
	printf("Listening on port %i.\n", port);
	fflush(stdout);

	signal(SIGINT, sighandler);

	for (;;) {
		char buf[256];
		FILE* fid;
		char getsetstr[16];
		char fn[256];
		int set;
		int get;
		int filenum;
		int fieldnum;
		struct sockaddr_in clientaddr;
		socklen_t addrsz = sizeof(clientaddr);

		if (bailout) {
			break;
		}

		int s = accept(sock, (struct sockaddr*)&clientaddr, &addrsz);
		if (s == -1) {
			fprintf(stderr, "Error: failed to accept() on socket: %s\n", strerror(errno));
			continue;
		}
		if (addrsz != sizeof(clientaddr)) {
			fprintf(stderr, "Error: client address has size %i, not %i.\n", addrsz, sizeof(clientaddr));
			continue;
		}
		printf("Connection from %s\n", inet_ntoa(clientaddr.sin_addr));
		fflush(stdout);

		fid = fdopen(s, "a+b");
		if (!fgets(buf, 256, fid)) {
			fprintf(stderr, "Error: failed to read a line of input.\n");
			goto bailout;
		}

		//printf("Got request %s\n", buf);

		if (sscanf(buf, "%4s %i %i\n", getsetstr, &filenum, &fieldnum) != 3) {
			fprintf(stderr, "Error: malformed request: %s\n", buf);
			goto bailout;
		}

		get = (strcmp(getsetstr, "get") == 0);
		set = (strcmp(getsetstr, "set") == 0);

		sprintf(fn, solvedfnpattern, filenum);

		if (get) {
			FILE* f = fopen(fn, "rb");
			unsigned char val;
			off_t end;
			printf("Get %s [%i].\n", fn, fieldnum);
			if (!f) {
				// assume it's not solved!
				fprintf(fid, "unsolved %i %i\n", filenum, fieldnum);
				fflush(fid);
				goto bailout;
			}
			if (fseek(f, 0, SEEK_END) ||
				((end = ftello(f)) == -1)) {
				fprintf(stderr, "Error: seeking to end of file %s: %s\n",
						fn, strerror(errno));
				goto bailout;
			} else {
				if (end <= fieldnum) {
					fprintf(fid, "unsolved %i %i\n", filenum, fieldnum);
					fflush(fid);
					fclose(f);
					goto bailout;
				}
			}
			if (fseeko(f, (off_t)fieldnum, SEEK_SET) ||
				(fread(&val, 1, 1, f) != 1)) {
				fprintf(stderr, "Error: seeking or reading from file %s: %s\n",
						fn, strerror(errno));
				fclose(f);
				goto bailout;
			}
			fprintf(fid, "%s %i %i\n", (val ? "solved" : "unsolved"),
					filenum, fieldnum);
			fflush(fid);
			fclose(f);
			goto bailout;
		} else if (set) {
			FILE* f = fopen(fn, "r+b");
			unsigned char val;
			off_t off;
			printf("Set %s [%i].\n", fn, fieldnum);
			if (!f) {
				fprintf(stderr, "Error: failed to open file %s for writing: %s\n",
						fn, strerror(errno));
				goto bailout;
			}
			if (fseeko(f, 0, SEEK_END) ||
				((off = ftello(f)) == -1)) {
				fprintf(stderr, "Error: failed to seek to end of file %s: %s\n",
						fn, strerror(errno));
				goto bailout;
			} else {
				if (off < fieldnum) {
					// pad.
					int npad = fieldnum - off;
					int i;
					val = 0;
					for (i=0; i<npad; i++)
						if (fwrite(&val, 1, 1, f) != 1)
							fprintf(stderr, "Error: failed to write padding to file %s: %s\n",
									fn, strerror(errno));
				}
			}
			val = 1;
			if (fflush(f) ||
				fseeko(f, (off_t)fieldnum, SEEK_SET) ||
				(fwrite(&val, 1, 1, f) != 1)) {
				fprintf(stderr, "Error: seeking or writing to file %s: %s\n",
						fn, strerror(errno));
				fclose(f);
				goto bailout;
			}
			if (fclose(f)) {
				fprintf(stderr, "Error: closing file %s: %s\n", fn, strerror(errno));
				goto bailout;
			}
			goto bailout;
		} else {
			fprintf(stderr, "Error: malformed command.\n");
			goto bailout;
		}

	bailout:
		fclose(fid);
		continue;
	}

	printf("Closing socket...\n");
	if (close(sock)) {
		fprintf(stderr, "Error: failed to close socket: %s\n", strerror(errno));
	}

	return 0;
}


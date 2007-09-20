/*
  This file is part of the Astrometry.net suite.
  Copyright 2006, 2007 Dustin Lang, Keir Mierle and Sam Roweis.

  The Astrometry.net suite is free software; you can redistribute
  it and/or modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation, version 2.

  The Astrometry.net suite is distributed in the hope that it will be
  useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with the Astrometry.net suite ; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
*/

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <signal.h>
#include <assert.h>
#include <unistd.h>
#include <stdarg.h>
#include <libgen.h>
#include <dirent.h>

#include "ioutils.h"

uint32_t ENDIAN_DETECTOR = 0x01020304;

void asprintf_safe(char** strp, const char* format, ...) {
	va_list lst;
	int rtn;
	va_start(lst, format);
    rtn = vasprintf(strp, format, lst);
    if (rtn == -1) {
        fprintf(stderr, "Error, vasprintf() failed: %s\n", strerror(errno));
        fprintf(stderr, "  (format: \"%s\")\n", format);
        assert(0);
        *strp = NULL;
    }
	va_end(lst);
}

sl* dir_get_contents(const char* path, sl* list, bool filesonly, bool recurse) {
    DIR* dir = opendir(path);
    if (!dir) {
        fprintf(stderr, "Failed to open directory \"%s\": %s\n", path, strerror(errno));
        return NULL;
    }
    if (!list)
        list = sl_new(256);
    while (1) {
        struct dirent* de;
        struct stat st;
        char* name;
        char* fullpath;
        bool freeit = FALSE;
        errno = 0;
        de = readdir(dir);
        if (!de) {
            if (errno)
                fprintf(stderr, "Failed to read entry from directory \"%s\": %s\n", path, strerror(errno));
            break;
        }
        name = de->d_name;
        if (!strcmp(name, ".") || !strcmp(name, ".."))
            continue;
        asprintf_safe(&fullpath, "%s/%s", path, name);
        if (stat(fullpath, &st)) {
            fprintf(stderr, "Failed to stat file %s: %s\n", fullpath, strerror(errno));
            closedir(dir);
            sl_free2(list);
            return NULL;
        }

        if (filesonly) {
            if (S_ISREG(st.st_mode) || S_ISLNK(st.st_mode))
                sl_append_nocopy(list, fullpath);
            else
                freeit = TRUE;
        } else {
            sl_append_nocopy(list, fullpath);
        }
        if (recurse && S_ISDIR(st.st_mode)) {
            dir_get_contents(path, list, filesonly, recurse);
        }
        if (freeit)
            free(fullpath);
    }
    closedir(dir);
    return list;
}

char* find_executable(const char* progname, const char* sibling) {
    char* sib;
    char* sibdir;
    char* path;
    char* pathenv;

    // If it's an absolute path, just return it.
    if (progname[0] == '/')
        return strdup(progname);

    // If it's a relative path, resolve it.
    if (strchr(progname, '/')) {
        path = canonicalize_file_name(progname);
        if (path && file_executable(path))
            return path;
        free(path);
    }

    // If "sibling" contains a "/", then check relative to it.
    if (sibling && strchr(sibling, '/')) {
        // dirname() overwrites its arguments, so make a copy...
        sib = strdup(sibling);
        sibdir = strdup(dirname(sib));
        free(sib);

        asprintf_safe(&path, "%s/%s", sibdir, progname);
        free(sibdir);

        if (file_executable(path))
            return path;

        free(path);
    }

    // Search PATH.
    pathenv = getenv("PATH");
    while (1) {
        char* colon;
        int len;
        if (!strlen(pathenv))
            break;
        colon = strchr(pathenv, ':');
        if (colon)
            len = colon - pathenv;
        else
            len = strlen(pathenv);
        if (pathenv[len - 1] == '/')
            len--;
        asprintf_safe(&path, "%.*s/%s", len, pathenv, progname);
        if (file_executable(path))
            return path;
        free(path);
        if (colon)
            pathenv = colon + 1;
        else
            break;
    }

    // Not found.
    return NULL;
}

int run_command_get_outputs(const char* cmd, sl** outlines, sl** errlines, const char** errormsg) {
	int outpipe[2];
	int errpipe[2];
	pid_t pid;

	if (outlines) {
		if (pipe(outpipe) == -1) {
			if (errormsg) *errormsg = "Error creating pipe";
			return -1;
		}
	}
	if (errlines) {
		if (pipe(errpipe) == -1) {
			if (errormsg) *errormsg = "Error creating pipe";
			return -1;
		}
	}

	pid = fork();
	if (pid == -1) {
		if (errormsg) *errormsg = "Error fork()ing";
		return -1;
	} else if (pid == 0) {
		// Child process.
		if (outlines) {
			close(outpipe[0]);
			// bind stdout to the pipe.
			if (dup2(outpipe[1], STDOUT_FILENO) == -1) {
				fprintf(stderr, "Failed to dup2 stdout: %s\n", strerror(errno));
				_exit( -1);
			}
		}
		if (errlines) {
			close(errpipe[0]);
			// bind stdout to the pipe.
			if (dup2(errpipe[1], STDERR_FILENO) == -1) {
				fprintf(stderr, "Failed to dup2 stdout: %s\n", strerror(errno));
				_exit( -1);
			}
		}
		// Use a "system"-like command to allow fancier commands.
		if (execlp("/bin/sh", "/bin/sh", "-c", cmd, (char*)NULL)) {
			fprintf(stderr, "Failed to execlp: %s\n", strerror(errno));
			_exit( -1);
		}
		// execlp doesn't return.
	} else {
		FILE *fout = NULL, *ferr = NULL;
		int status;
		// Parent process.
		if (outlines) {
			close(outpipe[1]);
			fout = fdopen(outpipe[0], "r");
			if (!fout) {
				if (errormsg) *errormsg = "Failed to fdopen() pipe";
				return -1;
			}
		}
		if (errlines) {
			close(errpipe[1]);
			ferr = fdopen(errpipe[0], "r");
			if (!ferr) {
				if (errormsg) *errormsg = "Failed to fdopen() pipe";
				return -1;
			}
		}
		// Wait for command to finish.
		// FIXME - do we need to read from the pipes to prevent the command
		// from blocking?
		//printf("Waiting for command to finish (PID %i).\n", (int)pid);
		do {
			if (waitpid(pid, &status, 0) == -1) {
				if (errormsg) *errormsg = "Failed to waitpid()";
				return -1;
			}
			if (WIFSIGNALED(status)) {
				if (errormsg) *errormsg = "Command was killed by signal"; // %i.\n", WTERMSIG(status));
				return -1;
			} else {
				int exitval = WEXITSTATUS(status);
				if (exitval == 127) {
					if (errormsg) *errormsg = "Command not found";
					return exitval;
				} else if (exitval) {
					if (errormsg) *errormsg = "Command failed"; //: return value %i.\n", exitval);
					return exitval;
				}
			}
		} while (!WIFEXITED(status) && !WIFSIGNALED(status));

		if (outlines) {
			*outlines = fid_get_lines(fout, FALSE);
			fclose(fout);
		}
		if (errlines) {
			*errlines = fid_get_lines(ferr, FALSE);
			fclose(ferr);
		}
	}
	return 0;
}

sl* file_get_lines(const char* fn, bool include_newlines) {
    FILE* fid;
	sl* list;
    fid = fopen(fn, "r");
    if (!fid) {
        fprintf(stderr, "file_get_lines: failed to open file \"%s\": %s\n", fn, strerror(errno));
        return NULL;
    }
	list = fid_get_lines(fid, include_newlines);
	fclose(fid);
	return list;
}

sl* fid_get_lines(FILE* fid, bool include_newlines) {
	sl* list;
	list = sl_new(256);
	while (1) {
		char* line = read_string_terminated(fid, "\n\r\0", 3, include_newlines);
		if (!line) {
			// error.
			fprintf(stderr, "fid_get_lines: failed to read a line.\n");
			sl_free2(list);
			return NULL;
		}
		sl_append_nocopy(list, line);
		if (feof(fid))
			break;
	}
	return list;
}

char* file_get_contents_offset(const char* fn, int offset, int size) {
    char* buf;
    FILE* fid;
    fid = fopen(fn, "rb");
    if (!fid) {
        fprintf(stderr, "file_get_contents_offset: failed to open file \"%s\": %s\n", fn, strerror(errno));
        return NULL;
    }
    buf = malloc(size);
    if (!buf) {
        fprintf(stderr, "file_get_contents_offset: couldn't malloc %lu bytes.\n", (long)size);
        return NULL;
    }
	if (offset) {
		if (fseeko(fid, offset, SEEK_SET)) {
			fprintf(stderr, "file_get_contents_offset: failed to fseeko: %s.\n", strerror(errno));
			return NULL;
		}
	}
	if (fread(buf, 1, size, fid) != size) {
        fprintf(stderr, "file_get_contents_offset: failed to read %lu bytes: %s\n", (long)size, strerror(errno));
        free(buf);
        return NULL;
    }
	fclose(fid);
    return buf;
}

void* file_get_contents(const char* fn, size_t* len, bool addzero) {
    struct stat st;
    char* buf;
    FILE* fid;
    off_t size;
    if (stat(fn, &st)) {
        fprintf(stderr, "file_get_contents: failed to stat file \"%s\"", fn);
        return NULL;
    }
    size = st.st_size;
    fid = fopen(fn, "rb");
    if (!fid) {
        fprintf(stderr, "file_get_contents: failed to open file \"%s\": %s\n", fn, strerror(errno));
        return NULL;
    }
    buf = malloc(size + (addzero ? 1 : 0));
    if (!buf) {
        fprintf(stderr, "file_get_contents: couldn't malloc %lu bytes.\n", (long)size);
        return NULL;
    }
    if (fread(buf, 1, size, fid) != size) {
        fprintf(stderr, "file_get_contents: failed to read %lu bytes: %s\n", (long)size, strerror(errno));
        free(buf);
        return NULL;
    }
	fclose(fid);
    if (addzero)
        buf[size] = '\0';
    if (len)
        *len = size;
    return buf;
}

void get_mmap_size(int start, int size, off_t* mapstart, size_t* mapsize, int* pgap) {
	int ps = getpagesize();
	int gap = start % ps;
	// start must be a multiple of pagesize.
	*mapstart = start - gap;
	*mapsize  = size  + gap;
	*pgap = gap;
}

bool file_exists(const char* fn) {
    return (access(fn, F_OK) == 0);
}

bool file_readable(const char* fn) {
    return (access(fn, R_OK) == 0);
}

bool file_executable(const char* fn) {
    return (access(fn, X_OK) == 0);
}

char* strdup_safe(const char* str) {
	char* rtn;
	if (!str) return NULL;
	rtn = strdup(str);
	if (!rtn) {
		fprintf(stderr, "Failed to strdup: %s\n", strerror(errno));
		assert(0);
	}
	return rtn;
}

static int oldsigbus_valid = 0;
static struct sigaction oldsigbus;
static void sigbus_handler(int sig) {
	fprintf(stderr, "\n\n"
			"SIGBUS (Bus error) signal received.\n"
			"One reason this can happen is that an I/O error is encountered\n"
			"on a file that we are reading with \"mmap\".\n\n"
			"Bailing out now.\n\n");
	fflush(stderr);
	exit(-1);
}

void add_sigbus_mmap_warning() {
	struct sigaction sigbus;
	memset(&sigbus, 0, sizeof(struct sigaction));
	sigbus.sa_handler = sigbus_handler;
	if (sigaction(SIGBUS, &sigbus, &oldsigbus)) {
		fprintf(stderr, "Failed to change SIGBUS handler: %s\n", strerror(errno));
		return;
	}
	oldsigbus_valid = 1;
}

void reset_sigbus_mmap_warning() {
	if (oldsigbus_valid) {
		if (sigaction(SIGBUS, &oldsigbus, NULL)) {
			fprintf(stderr, "Failed to restore SIGBUS handler: %s\n", strerror(errno));
			return;
		}
	}
}

int is_word(const char* cmdline, const char* keyword, char** cptr) {
	int len = strlen(keyword);
	if (strncmp(cmdline, keyword, len))
		return 0;
	*cptr = (char*)(cmdline + len);
	return 1;
}

void read_complain(FILE* fin, const char* attempted) {
	if (feof(fin)) {
		fprintf(stderr, "Couldn't read %s: end-of-file.\n", attempted);
	} else if (ferror(fin)) {
		fprintf(stderr, "Couldn't read %s: error: %s\n", attempted, strerror(errno));
	} else {
		fprintf(stderr, "Couldn't read %s: %s\n", attempted, strerror(errno));
	}
}

int read_u8(FILE* fin, unsigned char* val) {
    if (fread(val, 1, 1, fin) == 1) {
		return 0;
    } else {
		read_complain(fin, "u8");
		return 1;
    }
}

int read_u16(FILE* fin, unsigned int* val) {
	uint16_t v;
    if (fread(&v, 2, 1, fin) == 1) {
		*val = v;
		return 0;
    } else {
		read_complain(fin, "u8");
		return 1;
    }
}

int read_u32_portable(FILE* fin, unsigned int* val) {
    uint32_t u;
    if (fread(&u, 4, 1, fin) == 1) {
		*val = ntohl(u);
		return 0;
    } else {
		read_complain(fin, "u32");
		return 1;
    }
}

int read_double(FILE* fin, double* val) {
    if (fread(val, sizeof(double), 1, fin) == 1) {
		return 0;
    } else {
		read_complain(fin, "double");
		return 1;
    }
}

int read_u32(FILE* fin, unsigned int* val) {
    uint32_t u;
    if (fread(&u, 4, 1, fin) == 1) {
		*val = (unsigned int)u;
		return 0;
    } else {
		read_complain(fin, "u32 native");
		return 1;
    }
}

int read_u32s_portable(FILE* fin, unsigned int* val, int n) {
    int i;
    uint32_t* u = malloc(sizeof(uint32_t) * n);
    if (!u) {
		fprintf(stderr, "Couldn't real uint32s: couldn't allocate temp array.\n");
		return 1;
    }
    if (fread(u, sizeof(uint32_t), n, fin) == n) {
		for (i=0; i<n; i++) {
			val[i] = ntohl(u[i]);
		}
		free(u);
		return 0;
    } else {
		read_complain(fin, "uint32s");
		free(u);
		return 1;
    }
}

int read_fixed_length_string(FILE* fin, char* s, int length) {
	if (fread(s, 1, length, fin) != length) {
		read_complain(fin, "fixed-length string");
		return 1;
	}
	return 0;
}

char* read_string(FILE* fin) {
	return read_string_terminated(fin, "\0", 1, FALSE);
}

static char* growable_buffer_add(char* buf, int index, char c, int* size, int* sizestep, int* maxstep) {
	if (index == *size) {
		// expand
		*size += *sizestep;
		buf = realloc(buf, *size);
		if (!buf) {
			fprintf(stderr, "Couldn't allocate buffer: %i.\n", *size);
			return NULL;
		}
		if (*sizestep < *maxstep)
			*sizestep *= 2;
	}
	buf[index] = c;
	return buf;
}

char* read_string_terminated(FILE* fin, const char* terminators, int nterminators,
							 bool include_terminator) {
	int step = 1024;
	int maxstep = 1024*1024;
	int i = 0;
	int size = 0;
	char* rtn = NULL;
	for (;;) {
		int c = fgetc(fin);
		if (c == EOF)
			break;
		rtn = growable_buffer_add(rtn, i, c, &size, &step, &maxstep);
		if (!rtn)
			return NULL;
		i++;
		if (memchr(terminators, c, nterminators)) {
			if (!include_terminator)
				i--;
			break;
		}
	}
	if (ferror(fin)) {
		read_complain(fin, "string");
		free(rtn);
		return NULL;
	}
	// add \0 if it isn't already there;
	// return "\0" if nothing was read.
	if (i==0 || (rtn[i-1] != '\0')) {
		rtn = growable_buffer_add(rtn, i, '\0', &size, &step, &maxstep);
		if (!rtn)
			return NULL;
		i++;
	}
	if (i < size) {
		rtn = realloc(rtn, i);
		// shouldn't happen - we're shrinking.
		if (!rtn) {
			fprintf(stderr, "Couldn't realloc buffer: %i\n", i);
		}
	}
	return rtn;
}

int write_string(FILE* fout, char* s) {
	int len = strlen(s) + 1;
	if (fwrite(s, 1, len, fout) != len) {
		fprintf(stderr, "Couldn't write string: %s\n", strerror(errno));
		return 1;
	}
	return 0;
}

int write_fixed_length_string(FILE* fout, char* s, int length) {
	char* str;
	int res;
	str = calloc(length, 1);
	if (!str) {
		fprintf(stderr, "Couldn't allocate a temp buffer of size %i.\n", length);
		return 1;
	}
	sprintf(str, "%.*s", length, s);
	res = fwrite(str, 1, length, fout);
	free(str);
	if (res != length) {
		fprintf(stderr, "Couldn't write fixed-length string: %s\n", strerror(errno));
		return 1;
	}
	return 0;
}

int write_double(FILE* fout, double val) {
    if (fwrite(&val, sizeof(double), 1, fout) == 1) {
		return 0;
    } else {
		fprintf(stderr, "Couldn't write double: %s\n", strerror(errno));
		return 1;
    }
}

int write_float(FILE* fout, float val) {
    if (fwrite(&val, sizeof(float), 1, fout) == 1) {
		return 0;
    } else {
		fprintf(stderr, "Couldn't write float: %s\n", strerror(errno));
		return 1;
    }
}

int write_u8(FILE* fout, unsigned char val) {
    if (fwrite(&val, 1, 1, fout) == 1) {
		return 0;
    } else {
		fprintf(stderr, "Couldn't write u8: %s\n", strerror(errno));
		return 1;
    }
}

int write_u32_portable(FILE* fout, unsigned int val) {
    uint32_t v = htonl((uint32_t)val);
    if (fwrite(&v, 4, 1, fout) == 1) {
		return 0;
    } else {
		fprintf(stderr, "Couldn't write u32: %s\n", strerror(errno));
		return 1;
    }
}

int write_u32s_portable(FILE* fout, unsigned int* val, int n) {
    int i;
    uint32_t* v = malloc(sizeof(uint32_t) * n);
    if (!v) {
		fprintf(stderr, "Couldn't write u32s: couldn't allocate temp array.\n");
		return 1;
    }
    for (i=0; i<n; i++) {
		v[i] = htonl((uint32_t)val[i]);
    }
    if (fwrite(v, sizeof(uint32_t), n, fout) == n) {
		free(v);
		return 0;
    } else {
		fprintf(stderr, "Couldn't write u32s: %s\n", strerror(errno));
		free(v);
		return 1;
    }
}

int write_u32(FILE* fout, unsigned int val) {
    uint32_t v = (uint32_t)val;
    if (fwrite(&v, 4, 1, fout) == 1) {
		return 0;
    } else {
		fprintf(stderr, "Couldn't write u32: %s\n", strerror(errno));
		return 1;
    }
}

int write_u16(FILE* fout, unsigned int val) {
    uint16_t v = (uint16_t)val;
    if (fwrite(&v, 2, 1, fout) == 1) {
		return 0;
    } else {
		fprintf(stderr, "Couldn't write u32: %s\n", strerror(errno));
		return 1;
    }
}

int write_uints(FILE* fout, unsigned int* val, int n) {
    if (fwrite(val, sizeof(unsigned int), n, fout) == n) {
		return 0;
    } else {
		fprintf(stderr, "Couldn't write uints: %s\n", strerror(errno));
		return 1;
    }
}

void* buffered_read(bread* br) {
	void* rtn;
	if (!br->buffer) {
		br->buffer = malloc(br->blocksize * br->elementsize);
		br->nbuff = br->off = br->buffind = 0;
	}
	if (br->buffind == br->nbuff) {
		// read a new block!
		uint n = br->blocksize;
		// the new block to read starts after the current block...
		br->off += br->nbuff;
		if (n + br->off > br->ntotal)
			n = br->ntotal - br->off;
		if (!n)
			return NULL;
		memset(br->buffer, 0, br->blocksize * br->elementsize);
		if (br->refill_buffer(br->userdata, br->buffer, br->off, n)) {
			fprintf(stderr, "buffered_read: Error filling buffer.\n");
			return NULL;
		}
		br->nbuff = n;
		br->buffind = 0;
	}
	rtn = (char*)br->buffer + (br->buffind * br->elementsize);
	br->buffind++;
	return rtn;
}

void buffered_read_reset(bread* br) {
	br->nbuff = br->off = br->buffind = 0;
}

void buffered_read_pushback(bread* br) {
	if (!br->buffind) {
		fprintf(stderr, "buffered_read_pushback: Can't push back any further!\n");
		return;
	}
	br->buffind--;
}

void buffered_read_free(bread* br) {
	free(br->buffer);
}

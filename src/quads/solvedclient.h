#ifndef SOLVEDCLIENT_H
#define SOLVEDCLIENT_H

#include "bl.h"

int solvedclient_set_server(char* addr);

int solvedclient_get(int filenum, int fieldnum);

void solvedclient_set(int filenum, int fieldnum);

il* solvedclient_get_fields(int filenum, int firstfield, int lastfield,
							int maxnfields);

#endif

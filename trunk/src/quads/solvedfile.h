#ifndef SOLVEDFILE_H
#define SOLVEDFILE_H

int solvedserver_set_server(char* addr);

int solvedserver_get(int filenum, int fieldnum);

void solvedserver_set(int filenum, int fieldnum);

#endif

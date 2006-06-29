#ifndef SOLVEDCLIENT_H
#define SOLVEDCLIENT_H

int solvedclient_set_server(char* addr);

int solvedclient_get(int filenum, int fieldnum);

void solvedclient_set(int filenum, int fieldnum);

#endif

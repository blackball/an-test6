#ifndef __STRINGEXT_H__
#define __STRINGEXT_H__

/* -------------------------------------------------- */
/* -------------------------------------------------- */

#include <MyTypes.h>
#include <string.h>
#include <stdlib.h>
#ifdef WIN32
  #include <direct.h>
#else
  #include <unistd.h>
#endif
#include <stdio.h>
#include <ctype.h>

/* -------------------------------------------------- */
/* -------------------------------------------------- */

// Standard word delimiter
#define DELIM_INFO     " \t\n\r"

/* -------------------------------------------------- */
/* -------------------------------------------------- */

/*
 * A set of non-standard string functions for manipulating text.
 */

// Return the position of the file name extension in a string
char*  FindExtension(char *str);
// Removes extension from a file name.
void   SetTextName(char **dest, char *file);
// Return the sum of the characters in a string
DWORD  SumString(char *string);
// Return the sum of the characters, ignoring case
DWORD  SumIString(char *string);
// Determine if the character is whitespace
BOOL32 IsWhiteSpace(char data);
// Move ptr to first character after whitespace
BOOL32 EatWhiteSpace(char **ptr);
// Move to the first occurrence of the character, or the end of string
BOOL32 MoveToChar(char data, char **ptr);
// Verify line starts with a particualr string
BOOL32 LineStartsWith(char **CPlace, char* string);
// Skip past any individual string
BOOL32 SkipString(char **CPlace);
// Move passed any numbers
BOOL32 SkipNumber(char **CPlace);
// Compare two strings
BOOL32 StringCmp(char* s1, char *s2);
// Compare two strings up to 'max' chars
BOOL32 StringNCmp(char *s1, char *s2, int max);
// Compare strings ignoring case
BOOL32 StringICmp(char *s1, char *s2);
// Compare strings ignoring case up to 'max' chars
BOOL32 StringINCmp(char *s1, char *s2, int max);
// Concatenate two strings
BOOL32 CatTwo(char *dest, char *s1, char *s2);
// Create full file name using working directory
BOOL32 BuildFullPath(char *dest, char *file, int maxlen);
// Combine file and path into one string
BOOL32 CombineFilePath(char *dest, char *path, char *file, int maxlen);
// Move to start of a file name
void   GetFileNamePtr(char *fullName, char **ptr);
// Return the path porition of a file name
BOOL32 GetPath(char *dest, char *fullName, int maxlen);
// Read and copy from quote to quote from a file
BOOL32 FReadQuotedString(FILE *fp, char *buf, DWORD maxlen);
// Copy from quote to quote from a string
BOOL32 ReadQuotedString(char **ptr, char *buf, DWORD maxlen);
// Move the contents of a string backward in the string by a number of positions.
void   StringShiftBack(char *string, DWORD start, DWORD shift);
// Move the contents of a string forward in the string by a number of positions.
void   StringShiftForward(char *string, DWORD start, DWORD shift);
// Read string from ascii file up to '=' or delimiter
BOOL32 GetParamName(char *dest, char **tptr);
// Get a string from an ascii file after an '='
BOOL32 GetParamString(char *dest, char *tptr);
// Fill trailing whitespace with nulls
DWORD  Chop(char *s);

/* -------------------------------------------------- */
/* -------------------------------------------------- */

#endif

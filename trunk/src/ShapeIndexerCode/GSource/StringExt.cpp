/* -------------------------------------------------- */
/* -------------------------------------------------- */

#include <fstream.h>
#include <StringExt.h>

/* -------------------------------------------------- */
/* -------------------------------------------------- */

/*
 * FindExtension: Returns a pointer to the position in the string
 *                where the file extension begins. Pointer will be
 *                at the end of the string if no extension exists,
 *                otherwise it will start at the '.' starting the extension.
 * Assumes: 'str' is a valid string pointer.
 */
char* FindExtension(char *str)
{
  char *ptr = str;
  char *endpt;

  // go to end
  while (*ptr)
  {
    ptr++;
  }
  endpt = ptr;

  // walk backwards until the beginning of the string or the first
  // period, whichever comes first.
  while ((*ptr != '.') && (ptr != str))
  {
    ptr--;
  }

  // move back to the end if there's no extension
  if (ptr == str)
  {
    ptr = endpt;
  }

  return ptr;
}

/* -------------------------------------------------- */

/*
 * SetTextName: Copies a string 'file', minus the file extention if it exists,
 *              into another string 'dest'. 'dest' has space allocated for it.
 * Assumes: 'file' is a valid string pointer, 'dest' is a valid string pointer-pointer.
 */
void SetTextName(char **dest, char *file)
{
  int n;
  char *ptr;

  // make sure we ignore extension
  ptr = FindExtension(file);
  n = ptr - file;
  *dest = new char[n+1];

  // copy string
  strncpy(*dest, file, n);
  (*dest)[n] = '\0';
}

/* -------------------------------------------------- */

/**
 * SumString: Returns the sum of the integer values of every character in 'string'.
 * Assumes: 'string' is a valid string pointer.
 */
DWORD SumString(char *string)
{
	assert(string);

	DWORD sum = 0;
	int i = 1;

	while (*string)
	{
		sum += (*string)*i;
		i++;
		string++;
	}

	return sum;
}

/* -------------------------------------------------- */

/**
 * SumIString: Sums the integer values of every character in a string
 *             case insensitively. Useful for hashing keys.
 * Assumes: 'string' is a valid string pointer.
 */
DWORD SumIString(char *string)
{
	assert(string);

	DWORD sum = 0;
	int i = 1;

	while (*string)
	{
		sum += tolower((*string))*i;
		i++;
		string++;
	}

	return sum;
}

/* -------------------------------------------------- */

/**
 * IsWhiteSpace: Determines whether the provided character 
 *               is a whitespace characer or not.
 * Assumes: <none>
 */
BOOL32 IsWhiteSpace(char data)
{
  if (isspace(data))
  {
    return TRUE32;
  }
  return FALSE32;
}

/* -------------------------------------------------- */

/**
 * EatWhiteSpace: Moves a text pointer to the first non-whitespace
 *                character. If no non-whitespace characer is found, the
 *                function returns FALSE32, otherwise TRUE32.
 * Assumes: 'ptr' is a valid string pointer-pointer.
 */
BOOL32 EatWhiteSpace(char **ptr)
{
	char *tptr = *ptr;

  assert(ptr);
	assert(*ptr);

  // move forward
	while ((*tptr) && (isspace(*tptr)))
		tptr++;

  // assign udpated position
	*ptr = tptr;
	if (*tptr)
		return TRUE32;

	return FALSE32;
}

/* -------------------------------------------------- */

/**
 * MoveToChar: Moves a text pointer to the first characer of a certain
 *             type in a string. If the characer is not found, the function
 *             return FALSE32, otherwise TRUE32.
 * Assumes: 'ptr' is a valid string pointer-pointer.
 */
BOOL32 MoveToChar(char data, char **ptr)
{
	char *tptr = *ptr;

  assert(ptr);
	assert(*ptr);

  // move forward
	while ((*tptr) && (*tptr != data))
		tptr++;

	*ptr = tptr;
	if (*tptr)
		return TRUE32;

	return FALSE32;
}

/* -------------------------------------------------- */

/**
 * LineStartsWith: Determines whether the first non-whitespace, 
 *                 non-newline string matches the provided string. If 'string' is found
 *                 'CPlace' (the examined string) is moved based the word's occurence.
 *                 If 'string' is found, the function returns TRUE32, otherwise FALSE32.
 * Assumes: 'CPlace' is a valid string pointer-ponter, and 'string' is a valid pointer.
 */
BOOL32 LineStartsWith(char **CPlace, char* string)
{
	char *ptr = *CPlace;
	BOOL32 ret;

  assert(CPlace);
	assert(*CPlace);
	assert(string);

  // search for non-whitespace
	while ((*ptr != '\n') && (isspace(*ptr)))
		ptr++;

  // check string
	if (!strncmp(ptr, string, strlen(string)))
	{
		ret = TRUE32;
		ptr += strlen(string);
	} else
	{
		ret = FALSE32;
	}

	*CPlace = ptr;

	return ret;
}

/* -------------------------------------------------- */

/**
 * SkipString: Moves a text pointer past a quoted string (single or double).
 *             Returns TRUE32 if the end of the string is found, FALSE32 otherwise.
 * Assumes: The first character to be the first quote in the string,
 *          so that the string type (single or double) can be determined.
 */
BOOL32 SkipString(char **CPlace)
{
	assert(CPlace);
	assert(*CPlace);

	char goal = **CPlace;

	if ((goal != '\'') && (goal != '\"'))
	{
		return FALSE32;
	}

	(*CPlace)++;
	for (;;)
	{
		if (!MoveToChar(goal, CPlace))
			return FALSE32;
		if (*((*CPlace)-1) != '\\')
			break;
		if ((*(*CPlace-1) == '\\') && (*(*CPlace-2) == '\\'))
			break;
		(*CPlace)++;
	}
	(*CPlace)++;

	return TRUE32;
}

/* -------------------------------------------------- */

/**
 * SkipNumber: Moves a text pointer past a set of digits
 * Assumes: 'CPlace' is a valid string pointer-pointer.
 */
BOOL32 SkipNumber(char **CPlace)
{
	assert(CPlace);
	assert(*CPlace);

	char *ptr = *CPlace;

	while (isdigit(*ptr))
		ptr++;

	*CPlace = ptr;

	return TRUE32;
}

/* -------------------------------------------------- */

/**
 * StringCmp: Compares the content of two strings. Returns TRUE32 if 
 *            the strings match exactly, FALSE32 otherwise.
 * Assumes: 's1' and 's1' are valid string pointers.
 * 
 */
BOOL32 StringCmp(char* s1, char *s2)
{
	assert(s1);
	assert(s2);

	while (((*s1) && (*s2)) && (*s1 == *s2))
		s1++, s2++;

	if ((*s1) || (*s2))
		return FALSE32;

	return TRUE32;
}

/* -------------------------------------------------- */

/**
 * StringNCmp: Compares the content of two strings up to a maximum
 *             number of characters. Returns TRUE32 if the strings 
 *             match exactly for 'max' characers, or if they both match 
 *             and end before 'max' characters. Returns FALSE32 otherwise.
 * Assumes: 's1' and 's1' are valid string pointers.
 */
BOOL32 StringNCmp(char *s1, char *s2, int max)
{
	assert(s1);
	assert(s2);

	int i;

	i = 0;
	while (((*s1) && (*s2)) && ((*s1 == *s2) && (i < max)))
		s1++, s2++, i++;

	if ((i != max) && (*s1 != *s2))
		return FALSE32;

	return TRUE32;
}

/* -------------------------------------------------- */

/**
 * StringICmp: Compares the content of two strings ignoring case
 *             Returns TRUE32 if the strings match exactly (ignoring case), 
 *             FALSE32 otherwise.
 * Assumes: 's1' and 's1' are valid string pointers.
 */
BOOL32 StringICmp(char *s1, char *s2)
{
	assert(s1);
	assert(s2);

	while (((*s1) && (*s2)) && (toupper(*s1) == toupper(*s2)))
		s1++, s2++;

	if ((*s1) || (*s2))
		return FALSE32;

	return TRUE32;
}

/* -------------------------------------------------- */

/**
 * StringINCmp: Compares the content of two strings up to a maximum
 *              number of characters, ignoring case. Returns TRUE32 if 
 *              the strings match exactly for 'max' characers, ignoring 
 *              case, or if they both match and end before 'max' characters,
 *              and FALSE32 otherwise.
 * Assumes: 's1' and 's1' are valid string pointers.
 */
BOOL32 StringINCmp(char *s1, char *s2, int max)
{
	assert(s1);
	assert(s2);

	int i;

	i = 0;
	while (((*s1) && (*s2)) && ((toupper(*s1) == toupper(*s2)) && (i < max)))
		s1++, s2++, i++;
	
	if ((i != max) && (*s1 != *s2))
		return FALSE32;

	return TRUE32;
}

/* -------------------------------------------------- */

/**
 * CatTwo: Concatenates two strings into a destination string
 * Aasumes: 'dest' can hold the content of the two strings.
 *          's1' is a valid string pointer. 's1' is a valid string
 *          pointer.
 * Returns: TRUE32
 */
BOOL32 CatTwo(char *dest, char *s1, char *s2)
{
	assert(dest);
	assert(s1);
	assert(s2);

	strcpy(dest, s1);
	strcpy(dest+strlen(s1), s2);
	
	return TRUE32;
}

/* -------------------------------------------------- */

/**
 * DoPathCombine: Combines 'path', then 'file' into the string 'dest'
 *                for up to 'maxlen' characters. Returns TRUE32 if the
 *                complete fila name is copied, FALSE32 otherwise.
 * Aasumes: 'dest' can hold up to 'maxlen' characters.
 *          'path' is a properly formed pathname.
 */
BOOL32 DoPathCombine(char *dest, char *path, char *file, int maxlen)
{
	assert(dest);
	assert(path);
	assert(file);

	char *src, *dptr;

	// we'll have to build the relative path
	strcpy(dest, path);
	dptr = dest + strlen(dest);
	*(dptr++) = DIR_MARKER1;
	src = file;
	while (*src)
	{
		if (*src == '.')
		{
			// double dot means go back one directory
			if ((*(src+1) == '.') && ((*(src+2) == DIR_MARKER1) || (*(src+2) == DIR_MARKER2)))
			{
				dptr-=2;
				while (((*dptr != DIR_MARKER1) && (*dptr != DIR_MARKER2)) && (dptr != dest))
				{
					dptr--;
				}
				if (dptr == dest)
					return FALSE32;
				dptr++;
				src += 3;
				continue;
			} else
			{
				// single dot means stay as you are
				if ((*(src+1) == DIR_MARKER1) || (*(src+1) == DIR_MARKER2))
				{
					src+=2;
					continue;
				}
			}
		}
		// copy all we can until the next direcory marker
		while ((((*src != DIR_MARKER1) && (*src != DIR_MARKER2)) && (*src != '\0')) && ((int)(dptr - dest) < (maxlen-1)))
		{
			*(dptr++) = *(src++);
		}
		if (*src)
		{
			if ((int)(dptr - dest) >= (maxlen-1))
				return FALSE32;
			*(dptr++) = DIR_MARKER1;
			src++;
		}
	}
	*dptr = '\0';

	return TRUE32;
}

/* -------------------------------------------------- */

/**
 * BuildFullPath: Builds a full path name in 'dest' for the provided file name 'file'
 *     and places the result into the dest string. If the pathname is relative,
 *     the full name is created based on the current working directory. Returns
 *     TRUE32 if the path was successfullly built, FALSE32 otherwise (i.e. if 
 *     the resulting string was too long).
 * Assumes: 'file' is a properly formed path, 'dest' can hold up to 'maxlen' characters.
 */
BOOL32 BuildFullPath(char *dest, char *file, int maxlen)
{
	assert(dest);
	assert(file);

	char path[MAX_PATH+1];

#ifdef WIN32
	if ((*(file+1) == ':') && ((*(file+2) == DIR_MARKER1) || (*(file+2) == DIR_MARKER2)) )
	{
		strcpy(dest, file);
		return TRUE32;
	}

	if (!_getdcwd(_getdrive(), path, MAX_PATH+1))
		return FALSE32;
#else
	if ((*(file) == DIR_MARKER1) || (*(file) == DIR_MARKER2))
	{
		strcpy(dest, file);
		return TRUE32;
	}
	if (getcwd(path, MAX_PATH+1) == NULL)
		return FALSE32;
#endif

	return DoPathCombine(dest, path, file, maxlen);
}

/* -------------------------------------------------- */

/**
 * CombineFilePath: Builds a full path name for the provided file name and 
 *     the provided path and places the result into the dest string. 
 *     Only if the pathname is relative,  the full name is created based on 
 *     the provided path. Returns TRUE32 if the path was successfullly built,
 *     FALSE32 otherwise (i.e. if the resulting string was too long).
 * Takes: 'dest' as the destination string
 *        'path' as the path to use to build the full path
 *        'file' as the file to build the path for
 *        'maxlen' as the max length of space in the dest string.
 * Assumes: The 'path' is properly formed, 'file' is a valid string pointer,'dest' 
 *          can hold up to 'maxlen' characters.
 */
BOOL32 CombineFilePath(char *dest, char *path, char *file, int maxlen)
{
	assert(dest);
	assert(file);
	assert(path);

  if ((path == NULL) || (path[0] == '\0'))
  {
    strcpy(dest, file);
    return TRUE32;
  }

	// see if any work is necessary -- ie. if the path is complete, copy and quit
#ifdef __WIN32__
	if ((*(file+1) == ':') && ((*(file+2) == DIR_MARKER1) || (*(file+2) == DIR_MARKER2)) )
	{
		strcpy(dest, file);
		return TRUE32;
	}
#else
	if ((*(file) == DIR_MARKER1) || (*(file) == DIR_MARKER2))
	{
		strcpy(dest, file);
		return TRUE32;
	}
#endif

	return DoPathCombine(dest, path, file, maxlen);
}

/* -------------------------------------------------- */

/**
 * GetFileNamePtr: moves the double pointer to the position where the file name
 *                 in the provided string begins -- i.e. past the path.
 * Assumes: 'fullName' is a valid full path.
 */
void GetFileNamePtr(char *fullName, char **ptr)
{
	char *tptr;
	int len;

	assert(fullName);
	assert(ptr);

	// the empty string is trivially at the file name
	if ((len = strlen(fullName)) == 0)
	{
		*ptr = fullName;
		return;
	} else
	{	// move to the end of the string
		tptr = fullName + len - 1;
	}

	// move till the first direcotry change, or the start of the entire string
	while ((tptr != fullName) && (*tptr != DIR_MARKER1))
	{
		tptr--;
	}
	// move past the DIR_MARKER if necessary
  if ((*tptr == DIR_MARKER1) || (*tptr == DIR_MARKER2))
  {
    tptr++;
  }

	*ptr = tptr;
}

/* -------------------------------------------------- */

/**
 * GetPath: pulls out the path name from the 'fullName', placing the path
 *          into 'dest' for up to 'maxlen' characters. Returns TRUE32 on 
 *          success, FALSE32 if the resulting string was too long.
 * Assumes: 'dest' can hold up to 'maxlen' characters, 'fullName' is a valid 
 *          full filename.
 */
BOOL32 GetPath(char *dest, char *fullName, int maxlen)
{
	char *place;

	assert(dest);
	assert(fullName);

	// find the file name
	GetFileNamePtr(fullName, &place);
	if ((int)(place - fullName) >= maxlen)
		return FALSE32;

	// move behind the slash if necessary
	if ((int)(place - fullName) >= 1)
		place--;

	// copy the path
	strncpy(dest, fullName, (int)(place - fullName));
	*(dest+(int)(place - fullName)) = '\0';

	return TRUE32;
}

/* -------------------------------------------------- */

/**
 * FReadQuotedString: Searches a file 'fp' for the first douple quote,
 *                    then reads the text up to the next double quote into
 *                    'buf' up to 'maxlen characters. Returns TRUE32 on 
 *                    success, FALSE32 if the resulting string was too long.
 * Assumes: 'buf' can hold up to 'maxlen' characters, 'fp' is a valid 
 *          file pointer.
 */
BOOL32 FReadQuotedString(FILE *fp, char *buf, DWORD maxlen)
{
  DWORD count;
  char  c;

  while ((fgetc(fp) != '"') && (!feof(fp)))
    ;
  if (!feof(fp))
  {
    count = 0;
    while ((((c = fgetc(fp)) != '"') && (!feof(fp))) && (count < maxlen))
    {
      buf[count] = c;
      count++;
    }
    if (c != '"')
    {
      return FALSE32;
    }
    buf[count] = '\0';
    return TRUE32;
  } else
  {
    return FALSE32;
  }
}

/* -------------------------------------------------- */

/**
 * FReadQuotedString: Searches a string 'ptr' for the first douple quote,
 *                    then reads the text up to the next double quote into
 *                    'buf' up to 'maxlen characters. Moves 'ptr' as it reads.
 *                    Returns TRUE32 on 
 *                    success, FALSE32 if the resulting string was too long.
 * Assumes: 'buf' can hold up to 'maxlen' characters, 'ptr' is a valid 
 *          string pointer-pointer.
 */
BOOL32 ReadQuotedString(char **ptr, char *buf, DWORD maxlen)
{
  DWORD count;
  char *pt;

  pt = *ptr;
  while ((*pt != '"') && (*pt))
  {
    pt++; 
  }
  if (*pt)
  {
    pt++;
    count = 0;
    while ((((*pt != '"') && (*pt))) && (count < maxlen))
    {
      buf[count] = *pt;
      count++;
      pt++;
    }
    if (*pt != '"')
    {
      *ptr = pt;
      return FALSE32;
    }
    buf[count] = '\0';
    *ptr = pt+1;
    return TRUE32;
  } else
  {
    return FALSE32;
  }
}

/* -------------------------------------------------- */

/**
 * StringShiftBack: Shifts the content of 'string' back 'shift' places, starting
 *                  from position 'start' in the string. Shifted characters that go
 *                  off the beginning of the string are deleted.
 * Assumes: 'string' is a valid string pointer. 'start' < strlen('string').
 */
void StringShiftBack(char *string, DWORD start, DWORD shift)
{
  char *ptr;

  // move to the first resonable spot
  if (start < shift)
  {
    start = shift;
  }

  // shift the string backward
  ptr = string+start;
  while (*ptr)
  {
    *(ptr-shift) = *ptr;
    ptr++;
  }
  *(ptr-shift) = *ptr;
}

/* -------------------------------------------------- */

/**
 * StringShiftForward: Shifts the content of 'string' forward 'shift' places, starting
 *                  from position 'start' in the string. Shifted characters that go
 *                  off the end of the string are deleted.
 * Assumes: 'string' is a valid string pointer. 'start' < strlen('string').
 */
void StringShiftForward(char *string, DWORD start, DWORD shift)
{
  char *ptr, *goal;

  ptr  = string+start+strlen(string+start);
  goal = string+start;
  while (ptr >= goal)
  {
    *(ptr+shift) = *ptr;
    ptr--;
  }
}

/* -------------------------------------------------- */

/*
 * GetParamName: Copies a parameter name from an text parameter file
 *               into 'dest' from 'tptr', and moves 'tptr' past the 
 *               parameter name. Returns TRUE32 if a name is correctly found
 *               and a parameter is found afterwards. Returns FALSE32 otherwise.
 * Assumes: 'dest' can hold the string taken from 'tptr'. 'tptr' is a valid
 *          string pointer-pointer.
 */
BOOL32 GetParamName(char *dest, char **tptr)
{
  char *uptr;
  BOOL32 ret;
  uptr = *tptr;

  // move past the leading whitespace
  if ((ret = EatWhiteSpace(&uptr)) == TRUE32) 
  {
    // if the line already ends in a ';', return
    if (*uptr == ';')
    {
      return FALSE32;
    }

    // until we hit an '=' or a whitespace keep copying
    while (((*uptr != '=') && (*uptr)) && (IsWhiteSpace(*uptr) == FALSE32))
    {
      *dest = *uptr;
      uptr++;
      dest++;
    }
    // move up to the end of the string, or the first '='.
    *dest = '\0';
    while ((*uptr != '\0') && (*uptr != '='))
    {
      uptr++;
    }
    // move past the '=' up to the next word.
    if (*uptr == '=')
    {
      uptr++;
      ret = EatWhiteSpace(&uptr);
    } else
    {
      ret = FALSE32;
    }
  }

  *tptr = uptr;

  return ret;
}

/* -------------------------------------------------- */

/*
 * GetParamString: Returns the content of 'tptr' into 'dest' until a space character
 *                 is found. If 'tptr' begins with a double-quote the string
 *                 is copied until the end of string or next double-quote. If no data is
 *                 copied the function returns FALSE32, otherwise TRUE32.
 * Assumes: 'dest' references enough space to copy as much of 'tptr' as is needed. 'tptr' is
 *          a valid string pointer.
 */
BOOL32 GetParamString(char *dest, char *tptr)
{
  BOOL32 qEnd;
  char *dtest;

  // check first character for double-quote
  dtest = dest;
  if (*tptr == '"')
  {
    qEnd = TRUE32;
    tptr++;
  }
  // search the string
  while (TRUE32)
  {
    // check for the end search condition
    if ((((qEnd == TRUE32) && (*tptr == '"')) || 
          (IsWhiteSpace(*tptr) == TRUE32)) || (*tptr == '\0'))
    {
      *dest = '\0';
      break;
    }
    *dest = *tptr;
    dest++;
    tptr++;
  }

  return (dtest == dest) ? FALSE32:TRUE32;
}

/* -------------------------------------------------- */

/*
 * Chop: Fills any trailing whitespace in the string with NULLs. Returns
 *       the length of the post-chopped string.
 * Assumes: 's' is a valid string pointer.
 */
DWORD Chop(char *s)
{
  DWORD len;
  DWORD pos;

  // go to end of string
  len = strlen(s);
  pos = len-1;
  // search from the end of the string for the first non-whitespace 
  // character
  while (pos > 0)
  {
    if (IsWhiteSpace(s[pos]) == TRUE32)
    {
      s[pos] = '\0';
      pos--;
      len--;
    } else
    {
      break;
    }
  }
  // first character is a space, NULL it also
  if (len == 1)
  {
    if (IsWhiteSpace(s[0]) == TRUE32)
    {
      s[0] = '\0';
      len--;
    }
  }

  return len;
}

/* -------------------------------------------------- */
/* -------------------------------------------------- */

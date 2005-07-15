/* 	File: foo.c 
 * 	Author(s):
 * 	Date:
 * 	Purpose:
 */

#include "am_file.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include "am_string.h"
#include "am_string_array.h"
#include "amma.h"

#ifdef PC_PLATFORM
#include <wchar.h>
#include <direct.h>
#endif

#ifdef UNIX_PLATFORM
#include <dirent.h>
#endif

/* Unix uses / as a directory seperator, and
	/ as the root.  */
#define UNIX_PATHSTYLE 0
/* 	Windows uses \ as a directory seperator, and
	\ (with optional DRIVELETTER: ) as the root.
	They also support \MACHINENAME\SHARENAME as a root, IIRC, but
	we're not going to support that.
	NT/XP/2k support Unicode, but we'll
	just hope, for now, that that won't come up. */
#define DOS_PATHSTYLE 1
/* 	If we ever need it.. classic versions of MacOS used : as a
	directory seperator, and :: as the root. I don't know how
	well this translation works though. */
#define MACOS_PATHSTYLE 2
/* 	Whatever's local */
#ifdef PC_PLATFORM
#define HERE_PATHSTYLE 1
#else
#define HERE_PATHSTYLE 0
#endif

#ifdef PC_PLATFORM
#define getcwd _getcwd
#endif

static char* mk_path_translate(char* path, int modefrom, int modeto);
static char get_sep_for_pathstyle(int mode);

char* mk_dospath_from_path(char* path)
{
return mk_path_translate(path, UNIX_PATHSTYLE, DOS_PATHSTYLE);
}

char* mk_path_from_dospath(char* path)
{
return mk_path_translate(path, DOS_PATHSTYLE, UNIX_PATHSTYLE);
}

static char* mk_path_translate(char* path, int modefrom, int modeto)
{ 
char* iter;
char* returner = mk_copy_string(path);
iter = returner;

if(modefrom == modeto) {return returner;}

if(modefrom == DOS_PATHSTYLE)
  { /* If we're coming from windows, check for and remove driveletter prefix. */
  if( (*iter == '\0') /* Pardon my Perl style */
    ||(*(iter+1) != ':')) /* If we don't have a driveletter, move on*/
    {
/*  printf("DEBUG: No removal needed\n");*/
    }
  else
    {
    printf("Note: Removing drive letter from path [%s]\n", path);
    free_string(returner);
    returner = mk_copy_string(iter+2); 
    iter = returner;
    }
  }
while(*iter != '\0')
  {
  if(*iter == get_sep_for_pathstyle(modefrom) )
    *iter = get_sep_for_pathstyle(modeto);
  iter++;
  }
return returner;
}

static char get_sep_for_pathstyle(int mode)
{
switch(mode)
  {
  case DOS_PATHSTYLE:
  return '\\';

  case UNIX_PATHSTYLE:
  return '/';

  case MACOS_PATHSTYLE:
  return ':';

  default:
  my_errorf("Unknown pathstyle %d encountered!", mode);
  return '/'; /* Unused. if only we used function attributes ..*/
  }
}

bool am_mkpath_for(char* filename)
{
char* pathpart;
bool result;
int part = find_last_index_of_char_in_string(filename, get_sep_for_pathstyle(HERE_PATHSTYLE));

if(part == -1) return TRUE;
filename[part] = '\0'; /* Tokenize */
pathpart = mk_copy_string(filename);
filename[part] = get_sep_for_pathstyle(HERE_PATHSTYLE); /* And restore. */
result = am_mkdir_deeply(pathpart);
free_string(pathpart);
return result;
}

bool am_mkdir_deeply(char* dirname)
{
int diri=0;
int keep_going=1;

printf("am_mkdir_deeply(%s)\n", dirname);
while(keep_going)
  { /* Whenever we see our seperator, do a mkdir on all characters up to but
     not including that character */
  char* thispart;

  printf("diri=%d\n", diri);
  if(dirname[diri] == '\0')
    keep_going=0;
  else if(dirname[diri] != get_sep_for_pathstyle(HERE_PATHSTYLE))
    {diri++ ; continue;}

  thispart = mk_copy_string(dirname);
  thispart[diri] = '\0'; /* Chop it off at the current sep */
  am_mkdir(thispart);
  if(keep_going)
    thispart[diri] = get_sep_for_pathstyle(HERE_PATHSTYLE); /* Workaround a bug */

  free_string(thispart);
  diri++;
  }
return am_isdir(dirname); 
}

int am_mkdir(char* fn)
{
int result;
#ifdef PC_PLATFORM
result = mkdir(fn);
#else
result = mkdir(fn, 0755);
#endif
return (!result); /* Stupid disagreement btw kernel and C notions of truth. */
}

bool am_isdir(char* path)
{
#ifdef PC_PLATFORM /*use GetFileAttributes because stat can't
		     handle network (UNC) paths*/
    /*remove trailing backslash else windows won't find the dir*/
    char *new_path = mk_path_rm_trail_sep(path); 

    DWORD ret = GetFileAttributes(new_path);
    if ((ret != (DWORD)-1) && (ret & FILE_ATTRIBUTE_DIRECTORY))
    {
	free_string(new_path);
	return TRUE;
    }
    free_string(new_path);
    return FALSE;
#else
struct stat buf;

if(stat(path, &buf) == -1)
  return FALSE;
 
if(S_IFDIR & buf.st_mode)
  return TRUE;

return FALSE;
#endif
}

bool am_isfile(char* path)
{
#ifdef PC_PLATFORM /*use GetFileAttributes because stat can't
		     handle network (UNC) paths*/
    DWORD ret = GetFileAttributes(path);
    if ((ret != (DWORD)-1) && 
	!(ret & FILE_ATTRIBUTE_DIRECTORY))
	return TRUE;
    return FALSE;
#else
    struct stat buf;

    if(stat(path, &buf) == -1)
	return FALSE;
 
    if(S_IFREG & buf.st_mode)
	return TRUE;
    
    return FALSE;
#endif
}

/* Rename a file or directory */
bool am_rename_file(char* old_file, char *new_file)
{
    if ( rename (old_file, new_file) == 0 )
	return TRUE;
    return FALSE;
}

bool filenames_exist(string_array *filenames)
{
  bool result = TRUE;
  int i;
  for ( i = 0 ; result && i < string_array_size(filenames) ; i++ )
  {
    char *filename = string_array_ref(filenames,i);
    result = result && file_exists(filename);
  }
  return result;
}

bool file_exists(const char *fname)
{
  FILE *s = fopen(fname,"r");
  bool result = s != NULL;
  if ( s != NULL ) fclose(s);
  return result;
}

void remove_file(char *fname,char **r_errmess)
{
  *r_errmess = NULL;
  if (unlink(fname) == -1)
  {
      *r_errmess = mk_printf("Unable to remove '%s':%s", fname, 
			     strerror(errno));
  }
}

void remove_dir(char *dname, bool remove_contents, char** err)
{
    int i;
    *err = NULL;

    if (rmdir(dname) == 0)
    {
	return;
    }
    else if (errno == ENOTEMPTY && remove_contents == TRUE)
    {
	/*remove subdirs*/
	string_array *files = am_get_dir_listing(dname, FALSE, TRUE, err);
	for (i = 0; i < string_array_size(files) && *err == NULL; i++)
	{
	    char *name = string_array_ref(files, i);
	    char *path = mk_join_path2(dname, name);

	    if (strcmp(name,".") == 0 || strcmp(name, "..") == 0)
	    {
		free_string(path);
		continue;
	    }
	    remove_dir(path, remove_contents, err);
	    free_string(path);
	}
	free_string_array(files);

	/*remove files*/
	files = am_get_dir_listing(dname, TRUE, FALSE, err);
	for (i = 0; i < string_array_size(files) && *err == NULL; i++)
	{
	    char *path = mk_join_path2(dname, string_array_ref(files, i));
	    remove_file(path, err);
	    free_string(path);
	}
	free_string_array(files);

	/*now try to remove the dir again*/
	if (*err == NULL && rmdir(dname) == 0)
	{
	    return;
	}
    }

    /*if we got to this point, there was an error*/
    if (*err == NULL)
    {
	*err = mk_printf("Unable to remove '%s':%s", dname, strerror(errno));
    }
}

/*
  return the characters up to the last path separator.  If keep_trail_sep
  is true, the trailing path separator is also returned.
  ex. /usr/bin/emacs => /usr/bin/

  If the path does not contain a separator, an empty string is returned.
*/
char *mk_path_dirname(const char *path, bool keep_trail_sep)
{
    char *str;
    char *dirname = NULL;
    char sep = am_get_path_sep();

    str = strrchr(path, sep);
    if (str)
    {
	dirname = AM_MALLOC_ARRAY(char, str - path + 2);
	strncpy(dirname, path, str - path + 1);
	dirname[str - path + 1] = '\0';
    }
    else
    { 
	dirname = mk_copy_string("");
    }
    if (keep_trail_sep == FALSE)
    {
	char *tmp = mk_path_rm_trail_sep(dirname);
	free_string(dirname);
	dirname = tmp;
    }

    return dirname;
}

/*
  return the characeters after the last '.' 
  ex. /tmp/info.txt.back => back
*/
char *mk_path_suffix(const char *path)
{
    char *str1, *str2;
    char *ext = NULL;
    str1 = strrchr(path, '.');
    str2 = strrchr(path, am_get_path_sep());
    if (str1 && str1 > str2) /*doesn't count if have path sep after last '.'*/
    {	
	str1++;
	if (*str1 != '\0')
	{
	    ext = mk_copy_string(str1);
	}
    }
    if (!ext)
    {
	ext = AM_MALLOC_ARRAY(char, 1);
	ext[0] = '\0';
    }
    return ext;
}

/* Return the characters after the last path separator.  If
   include_suffix is FALSE, then exclude the last '.' and everything
   after it.

   If the path ends in a separator, an empty string is returned.*/
char *mk_path_basename(const char *path, bool include_suffix)
{
    char *str;
    char *basename = NULL;
    char *ext;
    char sep = am_get_path_sep();
    
    ext = strrchr(path, '.');
    str = strrchr(path, sep);
    if (str)
    {
	str++;
	if (*str != '\0')
	{
	    if (ext && ((ext - str) > 0) && !include_suffix)
	    {
		basename = AM_MALLOC_ARRAY(char, ext - str + 1);
		strncpy(basename, str, ext-str);
		basename[ext-str] = '\0';
	    }
	    else
	    {
		basename = mk_copy_string(str);
	    }
	}
	else
	{
	    /*last char was sep.  Return empty string*/
	    basename = mk_copy_string("");
	}
    }
    if (basename == NULL)
    {
	basename = mk_copy_string(path);
    }
    return basename;
}




/*return a copy of path with the trailing slash (if any) removed.
  If the directory is '/' (or '\') or is of the form D:\, don't 
  remove the trailing slash*/
char *mk_path_rm_trail_sep(const char *path)
{
    int len = strlen(path);
    char sep = am_get_path_sep();
    char *ret = NULL;

    if (len > 1 && path[len-1] == sep)
    {
	if (len == 3 && path[1] == ':' && path[2] == sep)
	{
	    /*format is D:\.  Don't remove trailing slash*/
	}
	else
	{
	    ret = AM_MALLOC_ARRAY(char, len);
	    strncpy(ret, path, len-1);
	    ret[len-1] = '\0';
	}
    }
    
    if (!ret) ret = mk_copy_string(path);

    return ret;
}

char am_get_path_sep(void)
{
    return get_sep_for_pathstyle(HERE_PATHSTYLE);
}

/*separator for the paths in the PATH environment variable string*/
char am_get_PATH_envstr_sep(void)
{
#if defined PC_PLATFORM
    return ';';
#elif defined UNIX_PLATFORM
    return ':';
#else 
    return ':';
#endif
}

/*joins path segments using the path separator*/
char *mk_join_path2(const char *path1, const char *path2)
{
    char *path1_nosep = mk_path_rm_trail_sep(path1);
    char sep = am_get_path_sep();
    char *full_path;
   
    if (strcmp(path1_nosep, "") == 0)
    {
	if (strcmp(path2, "") == 0)
	    full_path = mk_printf("");
	else
	    full_path = mk_copy_string(path2);
    }
    else
    {
	if (strcmp(path2, "") == 0)
	    full_path = mk_copy_string(path1_nosep);
	else if (path1_nosep[strlen(path1_nosep)-1] == sep)
	    /*root dir, don't insert extra sep*/
	    full_path = mk_printf("%s%s", path1_nosep, path2);
	else
	    full_path = mk_printf("%s%c%s", path1_nosep, sep, path2);
    }
    free_string(path1_nosep);

    return full_path;
}

char *mk_join_path(const string_array *path_strings)
{
    char *tmp;
    char *full_path = mk_copy_string("");
    int i;
    int size;

    if (!path_strings) return NULL;

    size = string_array_size(path_strings);

    for (i = 0; i < size; i++)
    {
	tmp = mk_join_path2(full_path, string_array_ref(path_strings, i));
	free_string(full_path);
	full_path = tmp;
    }

    return full_path;
}

/*splits path into segments*/
string_array *mk_split_path(const char *path)
{
    char *dirname = mk_path_rm_trail_sep(path);
    char *basename;
    char *tmp;
    string_array *path_list = mk_string_array(0);
    string_array *tmp_array;

    basename = mk_path_basename(dirname, TRUE);
    while(strlen(dirname) > 0)
    {
	if (strlen(basename) > 0)
	{
	    /*add the path segment*/
	    string_array_add(path_list, basename);
	    free_string(basename);
	}
	else if (strlen(dirname) > 0)
	{
	    /*no more segments*/
	    string_array_add(path_list, dirname);
	    break;
	}	    

	/*get the next dirname and basename*/
	tmp = dirname;
	dirname = mk_path_dirname(dirname, FALSE);
	basename = mk_path_basename(dirname, TRUE);
	free_string(tmp);
    }
    if (dirname) free_string(dirname);

    /*now have the path split, but in reverse order*/
    tmp_array = mk_reverse_string_array(path_list);
    free_string_array(path_list);
    path_list = tmp_array;

    return path_list;
}

char *mk_getcwd()
{
    char *cwd = NULL;
    char *tmp;
    int size = 256;
    
    /* get the current working directory*/
    tmp = AM_MALLOC_ARRAY(char, size);
    while (!getcwd(tmp, size) && errno == ERANGE)
    {
	AM_FREE_ARRAY(tmp, char, size);
	size *= 2;
	tmp = AM_MALLOC_ARRAY(char, size);
    }
    if (tmp)
    {
	cwd = mk_copy_string(tmp);
	AM_FREE_ARRAY(tmp, char, size);
    }
    return cwd;
}

/*return the absolute path to 'path'*/
char *mk_abspath(const char *path)
{
    char *orig_dir = NULL;
    char *abs_path = NULL;
    char *filename = NULL;
    char *path_only = mk_copy_string(path);

    /*if its a file, remove the filename*/
    if (am_isfile(path_only))
    {
	filename = mk_path_basename(path, TRUE);
	free_string(path_only);
	path_only = mk_path_dirname(path, FALSE);
    }

    /*save the current directory*/
    orig_dir = mk_getcwd();

    /*get the absolute path*/
    if (orig_dir && path_only)
    {
	if (chdir(path_only) == 0)
	{
	    abs_path = mk_getcwd();
	}
	else
	{
	    abs_path = mk_copy_string("");
	}
	chdir(orig_dir);
    }
    
    /*append the filename*/
    if (filename)
    {
	char *tmp = mk_join_path2(abs_path, filename);
	free_string(abs_path);
	abs_path = tmp;
    }
    
    if (filename)  free_string(filename);
    if (path_only) free_string(path_only);
    if (orig_dir)  free_string(orig_dir);

    return abs_path;
}

/*figure out the full path to an exe...
  - if the input string contains path separators, assume it is the full path
  - else look in the current dir (windows only - on unix even current dir
     should be in the user's path)
  - else look in the user's PATH for an exe with this name.
  - if the exe does not exist, return NULL.
*/
char *mk_get_exe_path(const char *exe)
{
    char *path = NULL;

    if (strchr(exe, am_get_path_sep()) && am_isfile((char *)exe))
    {
	path = mk_copy_string(exe);
    }
    else
    {
#if defined PC_PLATFORM
	/*look in cwd on windows*/
	path = mk_join_path2(".", exe);
	if (!am_isfile(path))
	{
	    free_string(path);
	    path = NULL;
	}
#endif
	if (!path)
	{
	    /*get the user's PATH */
	    char *path_str = getenv("PATH");
	    if (path_str)
	    {
		char *tmp = mk_printf("%c", am_get_PATH_envstr_sep());
		string_array *paths = mk_split_string(path_str, tmp);
		int i;

		free_string(tmp);
		for (i = 0; 
		     i < string_array_size(paths) && path == NULL ; 
		     i++)
		{
		    path = mk_join_path2(string_array_ref(paths, i),
					 exe);
		    if (!am_isfile(path))
		    {
			free_string(path);
			path = NULL;
		    }		    
		}
		free_string_array(paths);
	    }
	    else
	    {
		fprintf(stderr, "PATH variable not set.\n");
	    }
	}
    }
    return path;
}


string_array *
am_get_dir_listing(const char *dir, bool include_files, bool include_dirs,
		   char **err)
{
#ifdef PC_PLATFORM

    WIN32_FIND_DATA FindFileData;
    HANDLE hFind;
    string_array *names = NULL;
    char *full_dir = mk_join_path2(dir, "*");

    hFind = FindFirstFile(full_dir, &FindFileData);
    free_string(full_dir);

    if (hFind == INVALID_HANDLE_VALUE) 
    {
	char *str = strerror(errno);
	if (!str) str = "unknown error.";
	*err = mk_copy_string(str);
    } 
    else 
    {
	bool done = FALSE;
	names = mk_string_array(0);	

	while(!done && FindFileData.cFileName)
	{
	    char *filename = FindFileData.cFileName;
	    char *full_path = mk_join_path2(dir, filename);
	    
	    if (include_files && am_isfile(full_path))
		string_array_add(names, filename);
	    if (include_dirs && am_isdir(full_path))
		string_array_add(names, filename);
	    
	    free_string(full_path);
	    if (!FindNextFile(hFind, &FindFileData))
	    {
		done = TRUE;
	    }
	}
	FindClose(hFind);
    }

    return names;
    
#else /*UNIX*/

    DIR *p_dir;
    struct dirent *de = NULL;
    string_array *names = NULL;

    if (am_isdir((char *)dir) == FALSE)
    {
	*err = mk_printf("'%s' is not a valid directory.", dir);
	return NULL;
    }
    
    p_dir = opendir(dir);
    if (!p_dir)
    {
	char *str = strerror(errno);
	if (!str) str = "unknown error.";
	*err = mk_copy_string(str);
    }
    else
    {
	names = mk_string_array(0);
	de = readdir(p_dir);
	while(de)
	{
	    char *full_path = mk_join_path2(dir, de->d_name);
	    
	    if (include_files && am_isfile(full_path))
		string_array_add(names, de->d_name);
	    if (include_dirs && am_isdir(full_path))
		string_array_add(names, de->d_name);

	    free_string(full_path);
	    de = readdir(p_dir);
	}
	closedir(p_dir);
    }
    return names;
#endif
}



void am_file_test_code()
{
    char sep = am_get_path_sep();
    char *paths[6] = { "~/h/applic/",
		       "/home/sillyputty/",
		       "/home//junk/",
		       "..",
		       "/",
		       "./junk.cfg"};
    int i;
    char *tmp;
    bool flag = FALSE;

    printf("separator char is '%c'\n", sep); /*use 'sep' so no warning*/
    for (i = 0; i < 6; i++)
    {
	tmp = mk_path_dirname(paths[i], flag);
	tmp = mk_path_basename(paths[i], flag);
	tmp = mk_path_suffix(paths[i]);
	tmp = mk_path_rm_trail_sep(paths[i]);
	tmp = mk_abspath(paths[i]);
	
	/*test join, split*/
	{
	    string_array *sa = mk_split_path(paths[i]);
	    tmp = mk_join_path(sa);
	}
    }
}

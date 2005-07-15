/* 	File: am_file.h
 * 	Author(s): Andrew Moore, Pat Gunn
 * 	Date: 31 July 2003
 * 	Purpose: Store things related to file manipulation, e.g.
 *               wrapped fopen/open functions, fcntl stuff, etc.
 *               Also contains code for manipulating file names.
 */

#ifndef AM_FILE_H
#define AM_FILE_H

#include "standard.h"
#include "ambs.h"
#include "am_string_array.h"

/* 	Unix pathstyles are native for auton applications. Here are some things
	that convert windows style filenames to unix-style, and vice versa.
	We do not guarantee that platform-specific things, like drive letters,
	are preserved. Sorry. If this is important, maybe we'll structify
	this at some point. */
char* mk_dospath_from_path(char* path);
char* mk_path_from_dospath(char* path);

/* A wrapper for your system's mkdir() function. 
	   TRUE for success, FALSE for error. */
int am_mkdir(char* filename);

/*wrapper for getcwd that handles string allocation*/
char *mk_getcwd();

/* Makes a directory, making all its parents as needed.
		FALSE if it fails. It does consider it a
		success if the directory already exists.  */
bool am_mkdir_deeply(char* pathname);
bool am_mkpath_for(char* filename);

/* Returns true if the provided path is a file or directory, as appropriate. */
bool am_isdir(char* path);
bool am_isfile(char* path);

/* Rename a file or directory */
bool am_rename_file(char* old_file, char *new_file);

/* Remove a file. */
void remove_file(char* fname, char** r_errmess);
void remove_dir(char *dname, bool remove_contents, char** err);

/* Get a listing of all the files and/or directories in the specified dir*/
string_array *am_get_dir_listing(const char *dir, 
				 bool include_files, bool include_dirs,
				 char **err);

/* Legacy functions that need to be rewritten to use the stat() interface.
   Same function as
   am_isfile. Note that on some Unices, you can open a directory as a file,
   and on others you cannot. Caveat Emptor! */
bool filenames_exist(string_array* filenames);
bool file_exists(const char* fname);

/********** filename manipulation routines ************/

/*return the path separator for this platform*/
char am_get_path_sep(void);

/*separator for the paths in the PATH environment variable string*/
char am_get_PATH_envstr_sep(void);

/*
  return the characters up to the last path separator.  If keep_trail_sep
  is true, the trailing path separator is also returned.
  ex. /usr/bin/emacs => /usr/bin/

  If the path does not contain a separator, an empty string is returned.
*/
char *mk_path_dirname(const char *path, bool keep_trail_sep);

/* Return the characters after the last path separator.  If
   include_suffix is FALSE, then exclude the last '.' and everything
   after it.

   If the path ends in a separator, an empty string is returned.*/
char *mk_path_basename(const char *path, bool include_suffix);

/*
  return the characeters after the last '.' 
  ex. /tmp/info.txt.back => back
*/
char *mk_path_suffix(const char *path);

/*return a copy of path with the trailing slash (if any) removed*/  
char *mk_path_rm_trail_sep(const char *path);

/*joins path segments using the path separator*/
char *mk_join_path2(const char *path1, const char *path2);
char *mk_join_path(const string_array *path_strings);

/*splits path (full path to a file) into segments*/
string_array *mk_split_path(const char *path);

/*return the absolute path to 'path'.  If no such directory exists,
  return the empty string.
*/
char *mk_abspath(const char *path);

/*figure out the full path to an exe...
  - if the input string contains path separators, assume it is the full path
  - else look in the current dir (windows only - on unix even current dir
     should be in the user's path)
  - else look in the user's PATH for an exe with this name.
  - if the exe does not exist, return NULL.
*/
char *mk_get_exe_path(const char *exe);

/*****************************************************/

#endif /* AM_FILE_H */ 


/* *
   File:        amiv.h
   Author:      Andrew W. Moore
   Created:     Sat Apr  8 18:48:25 EDT 1995
   Updated:     26 Nov 96
   Description: Header for Integer and Boolean Dynamic vectors

   Copyright (C) 1995, Andrew W. Moore
*/

#ifndef AMIV_H
#define AMIV_H

#include "standard.h"
#include "ambs.h"
#include "amar.h"

/* An ivec is an array of integers. You can read and write entries in the
   array and you can add or remove array entries. When compiled with AMFAST
   defined, it's all very fast and efficient. Without AMFAST defined it is
   slower but safer (run-time error checking happens).

Return the integer value in the i'th array element
(Precondition: 0 <= i < ivec_size(iv)) 
int ivec_ref(ivec *iv, int i);

Set the integer value in the i'th array element to be "value"
(Precondition: 0 <= i < ivec_size(iv)) 
void ivec_set(ivec *iv,int i,int value);

Increment the integer value in the i'th array element by "value"
(Precondition: 0 <= i < ivec_size(iv)) 
void ivec_increment(ivec *iv,int i,int value);

Return the number of elements in the array
int ivec_size(ivec *iv);

MAKE an ivec with the given number of elements. Eventually, unless 
you are prepared to cause a memory leak, you must free it with
free_ivec(iv). You MUST NOT try freeing with free(iv)
ivec *mk_zero_ivec(int size)

Increase the size of iv by 1, and store value in the new (rightmost) element.
void add_to_ivec(ivec *iv,int value)

Remove the i'th value, decreasing ivec's size by 1.
void ivec_remove(ivec *iv,int index);

Free iv and all its subcomponents. After this you must never access iv again.
void free_ivec(ivec *iv);
*/

typedef struct ivec_struct
{
  int ivec_code;
  int array_size;
  int size;
  int *iarr;
} ivec, *ivec_ptr;

int safe_ivec_ref(const ivec *iv, int i);
void safe_ivec_set(ivec *iv,int i,int value);
void safe_ivec_increment(ivec *iv,int i,int value);
int safe_ivec_size(const ivec *iv);

#ifdef AMFAST

#define ivec_ref(iv,i) ((iv)->iarr[i])
#define ivec_set(iv,i,v) ((iv)->iarr[i] = (v))
#define ivec_increment(iv,i,v) ((iv)->iarr[i] += (v))
#define ivec_size(iv) ((iv)->size)

#else

#define ivec_ref(iv,i) (safe_ivec_ref(iv, i))
#define ivec_set(iv,i,v) (safe_ivec_set(iv, i, v))
#define ivec_increment(iv,i,v) (safe_ivec_increment(iv, i, v))
#define ivec_size(iv) (safe_ivec_size(iv))

#endif

ivec *mk_ivec(int size);

/* Warning: no type checking can be done by the compiler.  You *must*
   send the values as integers for this to work correctly. */
ivec *mk_ivec_x( int size, ...);

void free_ivec(ivec *iv);

void ivec_destructive_resize(ivec* iv, int size);

/* save the ivec on one line of a file in a format easy to load.
   the comment is just to make the file a small bit human readable.
   It may be NULL.
*/
void fprintf_ivec_for_load(FILE *s, const ivec *iv, char *comment);
  
/* save the ivec to a file, in comma-separated value (csv) form
*/
void fprintf_ivec_csv(FILE *s,ivec *x);

/* Warning this function only works if the next line of fp contains an ivec
   stored using the above function
*/
ivec *mk_ivec_from_file(FILE *fp, char **r_errmess);

void fprintf_ivec(FILE *s,char *m1,const ivec *iv,char *m2);
void privec(ivec *iv);
void copy_iarr_to_ivec(int *iarr,int size,ivec *r_iv);
ivec *mk_ivec_from_iarr(int *iarr,int size);
void copy_ivec_to_iarr(ivec *iv, int *iarr);
int *mk_iarr_from_ivec(ivec *iv);

/* Makes an ivec of size end - start:
   { start , start+1 , .... end-2 , end-1 } */
ivec *mk_sequence_ivec(int start_value,int end_value);

/*
   Allocates and returns an ivec of size size in which ivec[i] = i
   ivec[0] = 0
   ivec[1] = 1
    .
    .
   ivec[size-1] = size-1
*/
ivec *mk_identity_ivec(int size);

void shuffle_ivec(ivec *iv);
void constant_ivec(ivec *iv,int value);
ivec *mk_constant_ivec(int size,int value);
void zero_ivec(ivec *iv);
ivec *mk_zero_ivec(int size);

void ivec_plus(ivec *iv_1, ivec *iv_2, ivec *r_d);
ivec *mk_ivec_plus(ivec *a,ivec *b);
void ivec_subtract(ivec *iv_1,ivec *iv_2,ivec *r_d);
ivec *mk_ivec_subtract(ivec *a,ivec *b);
void ivec_scalar_plus(const ivec *iv, int addme, ivec *riv);
ivec *mk_ivec_scalar_plus(ivec *iv,int delta);

void shift_ivec(ivec *iv, int shift);
ivec *mk_shifted_ivec( ivec *iv, int shift);

void ivec_scalar_mult(const ivec *iv, int scale, ivec *riv);
ivec *mk_ivec_scalar_mult(ivec *iv,int scale);
void copy_ivec(const ivec *iv,ivec *r_iv);
/* Copies size elements from start */
void copy_ivec_subset(ivec *iv, int start, int size, ivec *r_iv); 
ivec *mk_copy_ivec_subset(ivec *iv, int start, int size);
ivec *mk_copy_ivec(const ivec *iv);
int num_of_given_value(ivec *iv,int value);
int num_zero_entries(ivec *iv);
int num_nonzero_entries(ivec *iv);
int ivec_min(const ivec *iv);
int ivec_max(const ivec *iv);
int ivec_argmin(const ivec *iv);
int ivec_argmax(const ivec *iv);
bool ivec_equal(const ivec *a,const ivec *b);
ivec *mk_ivec_1(int x0);
ivec *mk_ivec_2(int x0,int x1);
ivec *mk_ivec_3(int x0,int x1,int x2);
ivec *mk_ivec_4(int x0,int x1,int x2,int x3);
ivec *mk_ivec_5(int x0,int x1,int x2,int x3,int x4);
ivec *mk_ivec_6(int x0,int x1,int x2,int x3,int x4,int x5);

void ivec_remove(ivec *iv,int idx); /* Reduces size by 1, removes idx'th 
                                      element. All elements to right of
                                      delete point copied one to left.
                                      See comments in amdm.c more details */

/* removes the values in div from iv */
void ivec_remove_ivec(ivec *iv, ivec *div);

void ivec_remove_last_element(ivec *iv); /* Reduce size by 1, remove 
					    last rightmost elt */
void ivec_remove_last_n_elements( ivec *iv, int n);

/* Remove all occurences of val in iv. Does nothing
   if val doesn't appear in ivec. Keeps remaining
   iv elements in the same order. */
void ivec_remove_value(ivec *iv,int val);

int ivec_sum(const ivec *iv);
int ivec_product_orig(ivec *iv); /* Added by WKW */

bool equal_ivecs(const ivec *iv1, const ivec *iv2);

/* return the number of positions in which x1 and x2 have
different values (defined to the the maximum of the two
lengths if the ivecs are of different lengths)
*/
int ivec_nonmatch(const ivec *x1, const ivec *x2);
void ivec_malloc_report(void);
void add_to_ivec(ivec *iv,int val);
void add_to_ivec_unique(ivec *iv,int val);
int add_to_ivec_unique2(ivec *iv,int val);
void add_to_sorted_ivec(ivec *siv, int val);
int add_to_sorted_ivec2(ivec *siv, int val);

/* Increases the ivec length by one. Inserts val as the index'th element
   in the ivec and moves all items previously with array index greater
   than "index" one to the right.

   Thus iv_after[i] = iv_before[i] if i < index
        iv_after[i+1] = iv_before[i] = i >= index
        iv_after[index] = val */
void ivec_insert(ivec *iv,int idx,int val);

/* Find least index i such that value = ivec_ref(iv,i).
  If not found, returns -1

  (Note there is a corresponding method that returns
   an ivec of all matches located in amdmex.h)
*/
int find_index_in_ivec(const ivec *iv, int value);

/* make and return an ivec containing the indices of iv that have value */
ivec *mk_indices_in_ivec(const ivec *iv, int value);

/* Finds an index i such that value = ivec_ref(iv,i).
  If not found, returns -1.
  It will start searching at index "hint", and will continue searching
  in indices further and further away from hint, alternating between larger
  and smaller values than hint (modulu ivec_size).
*/
int find_index_in_ivec_hint(const ivec *iv, int value, int hint);

/* Finds leftmost index such that siv[index] > val. If none
   such, returns ivec_size(siv) */
int index_in_sorted_ivec(ivec *siv,int val);

/* returns index of val if found, -1 if not. if multiple vals exist, there
   is no guarantee which index will be returned
*/
int find_in_sorted_ivec(ivec *siv, int val);

bool is_in_ivec (ivec *iv, int value); /* Checks membership */


/* Returns list of indices at which value appears. */
ivec *mk_is_in_ivec( ivec *iv, int value);

/* return the number of elements appearing in both ivecs */
int count_ivec_overlap(ivec *a, ivec *b);

/* More sophisticated operations on ivecs, courtesy of jay... */

void ivec_sort(const ivec *dv,ivec *r_dv);
ivec *mk_ivec_sort(const ivec *iv);

/*
 Creates a ivec of indices such that indices[i] is the origional
 location (in the unsorted iv) of the ith smallest value.
 Used when you want the location of the sorted values instead of 
 the sorted vector.
*/
ivec *mk_sorted_ivec_indices(ivec *iv);

/* all the elements of v1 not already in r_v2 will be added to it */
void ivec_union(ivec *v1, ivec *r_v2);
ivec *mk_ivec_union(ivec *v1, ivec *v2);

/*Pre: ivecs are ordered
*/
ivec *mk_ivec_union_ordered(ivec *v1, ivec *v2);
ivec *mk_ivec_diff_ordered(ivec *v1, ivec *v2);
ivec *mk_ivec_intersect_ordered(ivec *v1, ivec *v2);

/* x := x with y appended on the end */
void append_to_ivec(ivec *x,ivec *y);

/* Return x with y appended on the end */
ivec *mk_ivec_append(ivec *x,ivec *y);

ivec *mk_append_ivecs(ivec *a,ivec *b);


bool ivec_weakly_dominates(ivec *dx,ivec *dy);
char *mk_string_from_ivec(ivec *iv);

/* Ivec Arrays */

typedef struct ivec_array
{
  int size;
  int array_size;
  ivec **array;
} ivec_array;

#ifdef AMFAST
#define ivec_array_ref(iva,i) ((iva)->array[i])
#else
#define ivec_array_ref(iva,i) safe_ivec_array_ref(iva,i)
#endif
void pivec_array(ivec_array *iva);

/* Returns the max value in any of the ivecs in iva.
   PRE: Contains at least one non-zero-length ivec */
int ivec_array_max_value(ivec_array *iva);

ivec *mk_ivec_from_ivec_array(ivec_array *iva);

/* Added by Artur */
ivec_array *mk_transpose_ivec_array( ivec_array *iva );

/*Added by Dan: Something I've wanted for a LONG time!*/
#define ivec_array_ref_ref(iva,i,j) ivec_ref(ivec_array_ref(iva,i),j)
#define ivec_array_ref_set(iva,i,j,x) ivec_set(ivec_array_ref(iva,i),j,x)
#define ivec_array_ref_size(iva,i) ivec_size(ivec_array_ref(iva,i))

#define add_to_ivec_array_ref(iva,i,x) add_to_ivec(ivec_array_ref(iva,i),x)

ivec_array *mk_ivec_array(int size);
ivec_array *mk_empty_ivec_array(void);
ivec_array *mk_const_ivec_array(ivec *base_vec, int size);

/* Added by Jeremy - makes an ivec array of length size(lengths)
   where ivec number i an ivec of ivec_ref(lengths,i) zeros */
ivec_array *mk_ivec_array_of_given_lengths(ivec* lengths);

/* Creates an ivec array with size entries each composed of an ivec of size 0 */
ivec_array *mk_zero_ivec_array(int size);

void add_to_ivec_array (ivec_array *ivecarr, const ivec *this_ivec);
int ivec_array_size (const ivec_array *ivecarr);

/* Returns the sum of all ivec_size(...) values of all ivecs
   in iva */
int sum_of_ivec_array_sizes(ivec_array *iva);

ivec *safe_ivec_array_ref (ivec_array *ivecarr, int idx);
void ivec_array_set (ivec_array *iva, int idx, const ivec *iv);
void ivec_array_set_no_copy(ivec_array *iva, int idx, ivec *iv);
void fprintf_ivec_array (FILE *s, char *m1, ivec_array *ivecarr, char *m2);
void free_ivec_array (ivec_array *ivecarr);
int ivec_array_num_bytes(ivec_array *iva);

ivec_array *mk_copy_ivec_array (ivec_array *ivecarr);
bool ivec_array_equal(ivec_array *iva1,ivec_array *iva2);
ivec_array *mk_array_of_zero_length_ivecs (int size);
void ivec_array_remove(ivec_array *iva,int idx);
void ivec_array_remove_last_element( ivec_array *iva);
void ivec_array_remove_last_n_elements( ivec_array *iva, int n);


/* Returns an ivec_array of length equal to ivec_size(rows)
   in which result[i] = iva[rows[i]] */
ivec_array *mk_ivec_array_subset(ivec_array *iva,ivec *rows);

/* when coding variable dimension arrays, these can be used to convert the
   indices of the array onto the index for the 1-d array representation.
   dimsizes tells how many elements there are in each dimension of the
   array.  Both ivecs have a size equal to the number of dimensions in the
   array.
*/
void index_from_indices(ivec *indices, ivec *dimsizes, int *idx);
void indices_from_index(int idx, ivec *dimsizes, ivec *indices);
int next_indices(ivec *indices, ivec *sizes, ivec *freeze);
bool next_subset(ivec *subset, int n);

/* something that seems totally useless: returns a random value from a given ivec 
  added by anna
*/
int ivec_to_random_value(ivec *iv);

/*
   A random permutation of iv is returned. The higher p is, more
   shuffling will be done. p=0 means no change. p=1 means completely
   random permutation. Values in the middle indicate by how much
   the output is likely to differ from the input.
*/
void probabilistic_shuffle_ivec(ivec *iv, double p);

int ivec_num_bytes(ivec *iv);
ivec *mk_copy_ivec_fast(ivec *iv);

/* Returns ivec of size ivec_size(rows) in which
    result[i] = x[rows[i]] */
ivec *mk_ivec_subset(ivec *x,ivec *rows);

#endif /* #ifndef AMIV */

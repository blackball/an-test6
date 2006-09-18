#ifndef __HASHTABLE_HPP__
#define __HASHTABLE_HPP__

/* -------------------------------------------------- */
/* -------------------------------------------------- */

#include <Enumeration.hpp>
#include <BlockMemBank.hpp>
#include <math.h>
#include <stdlib.h>
#include <time.h>

/* -------------------------------------------------- */

#define DWORD_SIZE		  32      // size of a dwrod in bits
#define DEFAULT_RATIO   (0.1f)  // default hash table ratio
#define DEF_HASHB_SIZE  50      // default store block size

/* -------------------------------------------------- */
/* -------------------------------------------------- */

/** 
 * HashNode: Node structure used to hold elements in the hash table class
 */
struct HashNode
{
	DWORD     key;	    	// key for the node
	DWORD     origkey;  	// original (i.e. unadjusted for table length) key
	DELMETHOD method;	  	// the deletion method for this node when the class is deleted
	void     *data;		    // the data the node holds
	HashNode *next;			  // the next node in the hash table slot
  HashNode *prev;       // the previous node int he hash table slot
};

/* -------------------------------------------------- */

/**
 * HashTable: Allows the addition and deletion of arbitrary elements
 *            to and from a hash table structure.
 */
class HashTable
{
friend class HashEnum;    // allow the enumeration access to the table

protected:
	DWORD Size;				      // size of the table in terms of slots
  DWORD StartSize;
	DWORD Usage;			      // number of elements are in the table
	float MaxRatio;			    // maximum allowed usage ratio in the table before rebuilding the table
	float CurRatio;			    // current ratio in the table
	HashNode **Table;       // the table itself

  BlockMemBank DataBank;

	// hash function vars
	DWORD Chunk;		      	// size of a chunk used in the hash function
	DWORD *A;				        // list of chunks used for the hash function
	DWORD ACount;		      	// number of chunks in the hash function
	DWORD Ander;	      		// Ander used to logically and bits during the hash function process

  // enum vars
	DWORD OpNum;    		    // status of the enumeration

  // functional vars
	BOOL32   (*Checker)(void *found, void *check);	// user defined checking function used when 
													                        //  searching the table
  void     (*delFunc)(void *data);                // the data deletion function

public:
	HashTable(DWORD size, float maxratio, BOOL32 (*checker)(void *found, void *check) = NULL, 
            void (*deleter)(void *data) = NULL, DWORD storebsize = DEF_HASHB_SIZE);
	~HashTable();

	void   Empty();                                              // empties the table
	DWORD  GetCount();                                           // returns number of elements in the table
	void   Add(DWORD key, void *data, DELMETHOD method);         // adds data to the table
	BOOL32 Find(DWORD key, void** data, void *testdata = NULL);  // find a specific piece of data
  BOOL32 Exists(DWORD key, void *testdata = NULL);             // simply test for the existance of
	BOOL32 Remove(DWORD key, void** data, void *testdata = NULL);// remove a specific piece of data
  BOOL32 RemoveFirst(void **data);                             // remove the first element in the table
	void   SetMaxRatio(float ratio);                             // set the max ratio of used to empty space
	inline float  GetMaxRatio();                                 // get the max ratio of used to empty space
	inline float  GetCurRatio();                                 // get the current ratio of used to empty space
	void   SetChecker(BOOL32 (*checker)(void *found, void *check) = NULL); // set checking function for searches
  void   SetDelFunc(void (*deleter)(void *data) = NULL);       // set the data deletion function

  // enum funcs
  Enumeration* NewEnum();                                      // returns an enumeration for the table

protected:
  inline DWORD  Hash(DWORD key);                               // hash a key
  void          BuildTable();                                  // build the hash table
  void          DoAdd(HashNode *newNode);                      // add a hash table node
  void          RebuildTable(DWORD newSize);                   // rebuild the table to satisfy the max ratio
  void          RemoveNode(HashNode *node, void **data);       // remove a node from the table
  HashNode*     NewNode();                                     // allocate a new hash node
  void          DeleteNode(HashNode *node);                    // delete a hash node
};

/* -------------------------------------------------- */
/* -------------------------------------------------- */

#endif

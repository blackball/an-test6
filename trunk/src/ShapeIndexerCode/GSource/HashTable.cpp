/* -------------------------------------------------- */
/* -------------------------------------------------- */

#include <HashTable.hpp>

/* -------------------------------------------------- */

// min store block size
#define MIN_HASHBSIZE     1

/* -------------------------------------------------- */
/* -------------------------------------------------- */

/**
 * defDel: default delete function
 */
void hashtableDefDel(void *data)
{
  delete data;
}

/* -------------------------------------------------- */
/* -------------------------------------------------- */

/**
 * HashEnum: enumeration implementation for the hash table class
 */
class HashEnum : public Enumeration
{
friend class HashTable;       // allow the hash table to create this class

protected:
  HashTable  *MyTable;        // the table to enumerate

  DWORD       EnumLength;     // remaining elements in the enumeration
  HashNode   *EnumNode;       // current enumeration node
  DWORD       Pos;            // current table position
  DWORD       Size;           // size of the hash table
  DWORD       OpNum;          // enumeration status
  DWORD      *ParOpNum;       // ptr to hashtable enum status

protected:
  HashEnum(HashTable *table);

public:
  void   ReInitEnum();        // restart the enum
  void*  GetEnumNext();       // return the next element
  BOOL32 GetEnumOK();         // ensure enum is still valid
  DWORD  GetEnumCount();      // return remaining elements
};

/* -------------------------------------------------- */
/* -------------------------------------------------- */

HashEnum::HashEnum(HashTable *table)
{
  assert(table != NULL);

  MyTable= table;
  ParOpNum = &(MyTable->OpNum);

  ReInitEnum();
}

/* -------------------------------------------------- */

void HashEnum::ReInitEnum()
{
  DWORD i;

  EnumLength = MyTable->Usage;
  Pos = 0;
  if (EnumLength > 0)
  {
    OpNum = *ParOpNum;
    Size = MyTable->Size;
    for (i = 0; i < Size; i++)
    {
      if (MyTable->Table[i] != NULL)
      {
        EnumNode = MyTable->Table[Pos = i];
        break;
      }
    }
    assert(i != Size);
  } else
  {
    OpNum = *ParOpNum-1;
    EnumNode = NULL;
  }
}

/* -------------------------------------------------- */

void* HashEnum::GetEnumNext()
{
  void *data;
  DWORD i;

  if (*ParOpNum != OpNum)
  {
    return NULL;
  }

  data = EnumNode->data;

  EnumLength--;
  if (EnumLength == 0)
  {
    OpNum = *ParOpNum-1;
  } else
  {
    if (EnumNode->next != NULL)
    {
      EnumNode = EnumNode->next;
    } else
    {
      ++Pos;
      for (i = Pos; i < Size; i++)
      {
        if (MyTable->Table[i] != NULL)
        {
          EnumNode = MyTable->Table[i];
          break;
        }
      }
      Pos = i;
      assert(i != Size);
    }
  }

  return data;
}

/* -------------------------------------------------- */

BOOL32 HashEnum::GetEnumOK()
{
  return (*ParOpNum == OpNum) ? TRUE32:FALSE32;
}

/* -------------------------------------------------- */

DWORD HashEnum::GetEnumCount()
{
  return EnumLength;
}

/* -------------------------------------------------- */
/* -------------------------------------------------- */

/**
 * HashTable: Initializes the hash table
 * Takes: 'size' as the size of the table to build. Minimum size will be 10 slots.
 *        'maxratio as the maxixum ratio of slots to elements in the table. If set to zero, defaults 
 *           to DEFAULT_RATIO
 *        'checker' as a user function to verify that found data is the desired data when a Find is called.
 *            This can be set to NULL if you don't care about erroneous output, or if all keys are unique.
 */
HashTable::HashTable(DWORD size, float maxratio, BOOL32 (*checker)(void *found, void *check), 
                     void (*deleter)(void *data), DWORD storebsize)
{
  // make sure the block size is OK
  if (storebsize < MIN_HASHBSIZE)
  {
    storebsize = MIN_HASHBSIZE;
  }

	// set the table size
	if (size < 10)
		size = 10;
	Size = StartSize = size;

	// set the ratio
	if (maxratio <= 0.0f)
		maxratio = DEFAULT_RATIO;
	MaxRatio = maxratio;

  // set checking function
	Checker = checker;

	// build the table and hash function
	BuildTable();

  // init the enumeration info
  OpNum = 0;

  DataBank.Init(sizeof(HashNode), storebsize);

  SetDelFunc(deleter);
}

/* -------------------------------------------------- */

/**
 * HashTable: Deinitializes the hash table. Empties it and deletes the allocated space.
 */
HashTable::~HashTable()
{
	Empty();

	// delete table and hash function
	delete [] Table;
	delete [] A;

  DataBank.Deinit();
}

/* -------------------------------------------------- */

void HashTable::SetDelFunc(void (*deleter)(void *data))
{
  if (deleter == NULL)
  {
    delFunc = hashtableDefDel;
  } else
  {
    delFunc = deleter;
  }
}

/* -------------------------------------------------- */

/**
 * NewNode: Create a new node -- take it from a node store which helps to avoid
 *          constant dynamic allocation
 */
HashNode* HashTable::NewNode()
{
  HashNode* retNode;

  retNode = (HashNode*)DataBank.NewData();

  return retNode;
}

/* -------------------------------------------------- */

/**
 * DeleteNode: Delete a node using the store
 */
void HashTable::DeleteNode(HashNode *node)
{
  HashNode *tnode;

  tnode = (HashNode*)DataBank.GetLastData();
  if (node != tnode)
  {
    // copy data
    *node = *tnode;

    // update the links as needed

    // update chained table links
    if (node->prev != NULL)
    {
      node->prev->next = node;
    } else
    {
      Table[node->key] = node;
    }
    if (node->next != NULL)
    {
      node->next->prev = node;
    }
  }

  DataBank.RemoveData((BYTE*)tnode);
}

/* -------------------------------------------------- */

/**
 * Empty: Empties the hash table. Does not delete the table itself.
 */
void HashTable::Empty()
{
	HashNode *node;
	Enumeration *tenum;

  tenum = DataBank.NewEnum(FORWARD);
  while (tenum->GetEnumOK() == TRUE32)
  {
    node = (HashNode*)tenum->GetEnumNext();
    if (node->method == DM_DEALLOC)
    {
      (*delFunc)(node->data);
    }
  }
  delete tenum;
  DataBank.Empty();

  // clean the table
  delete [] Table;
  Size = StartSize;
  BuildTable();

  OpNum++;

  // reset table values
	Usage = 0;
}

/* -------------------------------------------------- */

/**
 * GetCount: Gets the number of elements in the table
 * Returns: The number of elements in the table 
 */
DWORD HashTable::GetCount()
{
  return Usage;
}

/* -------------------------------------------------- */

/**
 * BuildTable: Builds a hash table and function based on the preset Size
 */
void HashTable::BuildTable()
{
	DWORD i;

	// init the table
	Table = new HashNode*[Size];
	for (i = 0; i < Size; i++)
	{
		Table[i] = NULL;
	}

	// build a universal hash function
	// for this function, see Intro. to Algos. by Cormen et al

    // Recall ln(a)/ln(b) = log_b(a)
  Chunk = (int)floor(( log((double)Size) / log(2.0)) );
	ACount = (32/Chunk) + ((Chunk%32) ? 1:0);
	A = new DWORD[ACount];

	Ander = (DWORD)pow(2.0, (double)Chunk);
	srand((unsigned int)clock());
	for (i = 0; i < ACount; i++)
	{
		A[i] = (rand()%Ander);
	}
	Ander -= 1;

	// set the usage
	Usage = 0;
}

/* -------------------------------------------------- */

/**
 * Add: Adds a new element to the table based on key. Does not care about
 *      repeated keys.
 * Takes: 'key' as the key of the element
 *        'data' as a pointer to the data to hold
 *        'method' as an indicator of what to do with the data on empty
 */
void HashTable::Add(DWORD key, void *data, DELMETHOD method)
{
	HashNode *newNode;

	// build the node
	newNode = NewNode();
	newNode->data = data;
	newNode->origkey = key;
	newNode->method  = method;

	// do the real adding
	DoAdd(newNode);
	OpNum++;
}

/* -------------------------------------------------- */

/**
 * DoAdd: Adds a node to the table
 * Takes: 'newNode' as the prefilled node to enter. The node's key
 *        is not expected to be hashed yet.
 */
void HashTable::DoAdd(HashNode *newNode)
{
  DWORD key;

	// increase the usage and see if we need to resize the table. If so, resize
	if ((CurRatio = ((float)(Usage+1)/(float)Size)) > MaxRatio)
	{
		// RebuildTable will implicitly calc the new CurRatio
		RebuildTable(Size*2);
	}
	Usage++;

	// set the hashed key
	newNode->key = Hash(newNode->origkey);

	// place the node in the table. Collisions are chained
  newNode->prev = NULL;
  key = newNode->key;
	if (Table[key] != NULL)
	{
		newNode->next = Table[key];
    Table[key]->prev = newNode;
		Table[key] = newNode;
	} else
	{
		newNode->next = NULL;
		Table[key] = newNode;
	}
}

/* -------------------------------------------------- */

/**
 * RebuildTable: Resizes the table to accomodate the MaxRatio
 * Takes: 'newSize' as the new size of the table
 */
void HashTable::RebuildTable(DWORD newSize)
{
	HashNode **OldTable = Table;
	HashNode *node, *next;
  DWORD i, oldSize;
  DWORD count = 0, oldUsage;

	// we hit the max load ratio, so increase table size by 2
  oldUsage = Usage;
  oldSize = Size;
	Size = newSize;

	// create a new table and hash function
	delete [] A;
	BuildTable();

	// go through the old table and rehash all the data into
	// the new table
  for (i = 0; i < oldSize; i++)
  {
    node = OldTable[i];
    while (node != NULL)
	  {
      next = node->next;
		  DoAdd(node);
      count++;
		  node = next;
	  }
  }
  assert(oldUsage == count);
	delete [] OldTable;
}

/* -------------------------------------------------- */

/**
 * Find: Tries to find an element based on key and testdata
 * Takes: 'key' as the key of the node to search for
 *        'data' as a pointer to a pointer to fill with the address of the data
 *        'testdata' as a pointer to data to test values we find against. Useful
 *           for verification where keys may be identical. May be set to NULL.
 * Returns: TRUE32 on success, FALSE32 on failure
 */
BOOL32 HashTable::Find(DWORD key, void** data, void *testdata)
{
	HashNode *node;
	DWORD Outkey;

	// hash the key
	Outkey = Hash(key);
	// look in the table for the data
	node = Table[Outkey];
	while (node)
	{
		// if the keys match then if there's a checking function test the data furher,
		//   otherwise accept the data as found
		if (((node->key == Outkey) && (node->origkey == key)) && ((!Checker) || ((*Checker)(node->data, testdata))))
		{
			*data = node->data;
			return TRUE32;
		}
		node = node->next;
	}
	return FALSE32;
}

/* -------------------------------------------------- */

/**
 * Exists: test for the existence of a particular member -- do not retrieve
 */
BOOL32 HashTable::Exists(DWORD key, void *testdata)
{
  void *temp;

  // try to find the key, return no data
  return Find(key, &temp, testdata);
}

/* -------------------------------------------------- */

/**
 * RemoveNode: removes a node from the table, saving its data to 'data'
 */
void HashTable::RemoveNode(HashNode *node, void **data)
{
  DWORD outkey;
  
  outkey = node->key;

  // fix the chained list
  if (node->prev == NULL)
  {
	  Table[outkey] = Table[outkey]->next;
    if (Table[outkey] != NULL)
    {
      Table[outkey]->prev = NULL;
    }
  } else
  {
	  node->prev->next = node->next;
    if (node->next != NULL)
    {
      node->next->prev = node->prev;
    }
  }
  // grab the data
  *data = node->data;

  // cleanup
  DeleteNode(node);
  Usage--;
  OpNum++;
}

/* -------------------------------------------------- */

/**
 * Remove: Tries to find an element based on key and testdata, and removes that data from the table
 * Takes: 'key' as the key of the node to search for
 *        'data' as a pointer to a pointer to fill with the address of the data
 *        'testdata' as a pointer to data to test values we find against. Useful
 *           for verification where keys may be identical. May be set to NULL.
 * Returns: TRUE32 on success, FALSE32 on failure
 */
BOOL32 HashTable::Remove(DWORD key, void** data, void *testdata)
{
	HashNode *node, *prev;
	DWORD Outkey;

	// hash the key
	Outkey = Hash(key);

	// search for the element
	prev = node = Table[Outkey];
	while (node)
	{
		// as in find, make sure we get the best data
		if (((node->key == Outkey) && (node->origkey == key)) && ((!Checker) || ((*Checker)(node->data, testdata))))
		{
      // remove the node
      RemoveNode(node, data);
			return TRUE32;
		}
		prev = node;
		node = node->next;
	}

	return FALSE32;	
}

/* -------------------------------------------------- */

BOOL32 HashTable::RemoveFirst(void **data)
{
  DWORD i;

  if (Usage == 0)
  {
    return FALSE32;
  }

  for (i = 0; i < Size; i++)
  {
    if (Table[i] != NULL)
    {
      RemoveNode(Table[i], data);
      return TRUE32;
    }
  }
  assert(i != Size);

  return FALSE32;
}

/* -------------------------------------------------- */

/**
 * SetMaxRatio: Sets the maximum ratio of data to slots that can exist in the table before 
 *                a rebuild
 * Takes: 'ratio' as the new ratio to use. If ratio is zero, it defaults to DEFAULT_RATIO
 *        'data' as a pointer to a pointer to fill with the address of the data
 *        'testdata' as a pointer to data to test values we find against. Useful
 *           for verification where keys may be identical. May be set to NULL.
 */
void HashTable::SetMaxRatio(float ratio)
{
	DWORD mult;
	double dtemp;
	float OldRatio = MaxRatio;
	
	// set the ratio
	if (ratio <= 0)
		ratio = DEFAULT_RATIO;
	MaxRatio = ratio;

	// determine if we need to resize the table
	if ((CurRatio = ((float)Usage/(float)Size)) > MaxRatio)
	{
		dtemp = OldRatio/MaxRatio;
		// find the smallest power of two that when it multiplies
		//   Size will make the real ratio less than the MaxRatio
		mult = (DWORD) pow(2, ceil(log(dtemp) / log(2.0)) );
		
		// RebuildTable will implicitly calc the new CurRatio
		RebuildTable(Size*mult);
    OpNum++;
  }
}

/* -------------------------------------------------- */

/**
 * GetMaxRatio: Gets the maximum ratio of elements to slots being used
 * Returns: the max ratio
 */
float HashTable::GetMaxRatio()
{
	return MaxRatio;
}

/* -------------------------------------------------- */

/**
 * GetCurRatio: Gets the current ratio of element to slots in the table
 * Returns: the current ratio
 */
float  HashTable::GetCurRatio()
{
	return CurRatio;
}

/* -------------------------------------------------- */

/**
 * SetChecker: Sets the checking algorithm used to verify data on Find
 * Takes: 'checker' as a pointer to a function that takes a found elememt and 
 *           checks it against a checking piece of data. Internals are up to the user
 *           as long as TRUE32 is returns on mathc, FALSE32 otherwise.
 */
void HashTable::SetChecker(BOOL32 (*checker)(void *found, void *check))
{
	Checker = checker;
}

/* -------------------------------------------------- */

/**
 * Hash: Hashes a key based on the current hash function
 * Takes: 'key' as a the data to hash
 * Returns: The hashed key -- a table position
 */
DWORD HashTable::Hash(DWORD key)
{
	DWORD i;
	DWORD sum = 0;

	// perform the hash -- see Intro. to Algos. by Cormen et al
	for (i = 0; i < ACount; i++)
	{
		sum += A[i]*((key>>(Chunk*i)) & (Ander));
	}
	sum %= Size;

	return sum;
}

/* -------------------------------------------------- */

Enumeration* HashTable::NewEnum()
{
  return (new HashEnum(this));
}

/* -------------------------------------------------- */
/* -------------------------------------------------- */

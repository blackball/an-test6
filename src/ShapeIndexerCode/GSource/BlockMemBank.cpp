/* -------------------------------------------------- */
/* -------------------------------------------------- */

#include <stdio.h>
#include <memory.h>
#include <BlockMemBank.hpp>

/* -------------------------------------------------- */
/* -------------------------------------------------- */

/*
 * BMEnumNode: A structure indicating the current enumeration place inside a 
 *             BlockMemBank.
 */
struct BMEnumNode
{
  BMStoreNode *dnode;
  DWORD        offset;
};

/* -------------------------------------------------- */

/**
 * BlockMemForwardEnum: Forward Enumeration implementation for the data bank
 */
class BlockMemForwardEnum : public Enumeration
{
friend class BlockMemBank;  // Allow the data bank to create this class

protected:
  BlockMemBank *MyBank;     // The data bank ptr

  DWORD         EnumLength; // Remaining elements in the enumeration
  BMEnumNode    EnumNode;   // Current enum node
  DWORD         EnumPos;    // Current position in the queue
  DWORD         StoreBSize; // Size of the store blocks
  DWORD         OpNum;      // Enum status
  DWORD        *ParOpNum;   // Ptr to enumeration status

protected:
  BlockMemForwardEnum(BlockMemBank *bank);

public:
  void   ReInitEnum();      // Restart the enum
  void*  GetEnumNext();     // Get the next enum element
  BOOL32 GetEnumOK();       // Indicate whether enum is still valid
  DWORD  GetEnumCount();    // Return remaining elements in the enum
};

/* -------------------------------------------------- */
/* -------------------------------------------------- */

/**
 * BlockMemForwardEnum: Create the Enumeration, initializing it with the 
 *                      apporpriate data bank.
 * Assumes: 'bank' is a valid data bank that will persist throughout the life
 *          of the enumeration.
 */
BlockMemForwardEnum::BlockMemForwardEnum(BlockMemBank *bank)
{
  assert(bank != NULL);

  MyBank = bank;
  ParOpNum = &(MyBank->OpNum);
  StoreBSize = MyBank->StoreBSize;
  ReInitEnum();
}

/* -------------------------------------------------- */

/**
 * ReInitEnum: Re-initialize the enumeration to start from the beginning
 *             again.
 * Assumes: <none>
 */
void BlockMemForwardEnum::ReInitEnum()
{
  EnumLength = MyBank->Count;
  if (EnumLength > 0)
  {
    EnumNode.dnode  = MyBank->StoreRoot;
    EnumNode.offset = 0;
    OpNum = *ParOpNum;
  } else
  {
    OpNum = *ParOpNum-1;
    EnumNode.dnode  = NULL;
    EnumNode.offset = 0;
  }
}

/* -------------------------------------------------- */

/**
 * GetEnumNext: Returns a pointer to the next element in the enumeration.
 *              If no more elements are available, or if the data structure
 *              in which the data is stored has been modified (and therefore the enumeration
 *              is no longer valid), NULL is returned.
 * Assumes: <none>
 */
void* BlockMemForwardEnum::GetEnumNext()
{
  void *data;

  if (*ParOpNum != OpNum)
  {
    return NULL;
  }

  data = (void*)&EnumNode.dnode->data[EnumNode.offset];

  EnumLength--;
  if (EnumLength == 0)
  {
    OpNum = *ParOpNum-1;
  } else
  {
    EnumNode.offset += MyBank->DataSize;
    if (EnumNode.offset > (StoreBSize-1)*MyBank->DataSize)
    {
      EnumNode.dnode = EnumNode.dnode->next;
      EnumNode.offset = 0;
    }
  }

  return data;
}


/* -------------------------------------------------- */

/**
 * GetEnumOK: Returns TRUE32 if more elements are available, and if the data structure
 *            in which the data is stored has not been modified (and therefore the enumeration
 *            is still longer valid). Returns FALSE32 otherwise.
 * Assumes: <none>
 */
BOOL32 BlockMemForwardEnum::GetEnumOK()
{
  return (*ParOpNum == OpNum) ? TRUE32:FALSE32;
}

/* -------------------------------------------------- */

/**
 * GetEnumCount: Returns the number of elements remaining to be processed in
 *               enumeration. This should not be used to determine the validity of the
 *               enumeration. Use GetEnumOK for that function.
 * Assumes: <none>
 */
DWORD BlockMemForwardEnum::GetEnumCount()
{
  return EnumLength;
}

/* -------------------------------------------------- */
/* -------------------------------------------------- */

/*
 * BlockMemBackEnum: Backward enumeration implementation for the data bank.
 */
class BlockMemBackEnum : public Enumeration
{
friend class BlockMemBank;  // Allow the data bank to create this class

protected:
  BlockMemBank *MyBank;     // The data bank ptr

  DWORD         EnumLength; // Remaining elements in the enumeration
  BMEnumNode    EnumNode;   // Current enum node
  DWORD         EnumPos;    // Current position in the queue
  DWORD         StoreBSize; // Size of the store blocks
  DWORD         OpNum;      // Enum status
  DWORD        *ParOpNum;   // Ptr to enumeration status

protected:
  BlockMemBackEnum(BlockMemBank *bank);

public:
  void   ReInitEnum();      // Restart the enum
  void*  GetEnumNext();     // Get the next enum element
  BOOL32 GetEnumOK();       // Indicate whether enum is still valid
  DWORD  GetEnumCount();    // Return remaining elements in the enum
};

/* -------------------------------------------------- */
/* -------------------------------------------------- */

/**
 * BlockMemForwardEnum: Create the Enumeration, initializing it with the 
 *                      apporpriate data bank.
 * Assumes: 'bank' is a valid data bank that will persist throughout the life
 *          of the enumeration.
 */
BlockMemBackEnum::BlockMemBackEnum(BlockMemBank *bank)
{
  assert(bank != NULL);

  MyBank = bank;
  ParOpNum = &(MyBank->OpNum);

  ReInitEnum();
}

/* -------------------------------------------------- */

/**
 * ReInitEnum: Re-initialize the enumeration to start from the beginning
 *             again.
 * Assumes: <none>
 */
void BlockMemBackEnum::ReInitEnum()
{
  EnumLength = MyBank->Count;
  if (EnumLength > 0)
  {
    if (MyBank->NextStorePos == 0)
    {
      EnumNode.dnode  = MyBank->Store->prev;
      EnumNode.offset = (StoreBSize-1)*MyBank->DataSize;
    } else
    {
      EnumNode.dnode  = MyBank->Store;
      EnumNode.offset = MyBank->NextStorePos-MyBank->DataSize;
    }
    OpNum = *ParOpNum;
  } else
  {
    OpNum = *ParOpNum-1;
    EnumNode.dnode  = NULL;
    EnumNode.offset = 0;
  }
}

/* -------------------------------------------------- */

/**
 * GetEnumNext: Returns a pointer to the next element in the enumeration.
 *              If no more elements are available, or if the data structure
 *              in which the data is stored has been modified (and therefore the enumeration
 *              is no longer valid), NULL is returned.
 * Assumes: <none>
 */
void* BlockMemBackEnum::GetEnumNext()
{
  void *data;

  if (*ParOpNum != OpNum)
  {
    return NULL;
  }

  data = (void*)&EnumNode.dnode->data[EnumNode.offset];
  EnumLength--;
  if (EnumLength == 0)
  {
    OpNum = *ParOpNum-1;
  } else
  {
    if (EnumNode.offset == 0)
    {
      EnumNode.dnode = EnumNode.dnode->prev;
      EnumNode.offset = (StoreBSize-1)*MyBank->DataSize;
    } else
    {
      EnumNode.offset -= MyBank->DataSize;
    }
  }

  return data;
}

/* -------------------------------------------------- */

/**
 * GetEnumOK: Returns TRUE32 if more elements are available, and if the data structure
 *            in which the data is stored has not been modified (and therefore the enumeration
 *            is still longer valid). Returns FALSE32 otherwise.
 * Assumes: <none>
 */
BOOL32 BlockMemBackEnum::GetEnumOK()
{
  return (*ParOpNum == OpNum) ? TRUE32:FALSE32;
}

/* -------------------------------------------------- */

/**
 * GetEnumCount: Returns the number of elements remaining to be processed in
 *               enumeration. This should not be used to determine the validity of the
 *               enumeration. Use GetEnumOK for that function.
 * Assumes: <none>
 */
DWORD BlockMemBackEnum::GetEnumCount()
{
  return EnumLength;
}

/* -------------------------------------------------- */
/* -------------------------------------------------- */

/*
 * BlockMemBank: Start up the bank. Cannot by immediatly used from this state:
 *               requires a subsequent call to Init().
 * Assumes: <none>
 */
BlockMemBank::BlockMemBank()
{
  DataSize    = 0;
  StoreBSize  = 0;
  StoreLength = 0;
  Store = StoreRoot = NULL;
}

/* -------------------------------------------------- */

/*
 * BlockMemBank: Start up the bank with a particular data size and block size.
 *               The bank can be used immediately.
 * Assumes: dataSize > 0.
 */
BlockMemBank::BlockMemBank(DWORD dataSize, DWORD storebsize)
{
  DataSize    = 0;
  StoreBSize  = 0;
  StoreLength = 0;
  Store = StoreRoot = NULL;

  Init(dataSize, storebsize);
}

/* -------------------------------------------------- */

/*
 * ~BlockMemBank: Shuts down the data bank.
 * Assumes: <none>
 */
BlockMemBank::~BlockMemBank()
{
  Deinit();
}

/* -------------------------------------------------- */

/*
 * Init: Initializes the data bank with a particular data size
 *       and a particular block size.
 *       The 'dataSize' is in bytes per object, and the 'storebsize'
 *       is in objects-per-block.
 *       If 'storebsize' == 0, then the default items per block is used.
 * Assumes: dataSize > 0. 
 */
void BlockMemBank::Init(DWORD dataSize, DWORD storebsize)
{
  assert(dataSize != 0);

  // clear the bank
  Deinit();

  DataSize = dataSize;
  if (storebsize == 0)
    storebsize = DEF_BLOCKMEM_SIZE;

  // std vars
  Count = 0;

  // init the enumeration info
  OpNum = 0;

  // init the store
  Store = StoreRoot = new BMStoreNode;
  StoreRoot->next = StoreRoot->prev = NULL;
  NextStorePos = 0;
  StoreBSize   = storebsize;
  Store->data = new BYTE[StoreBSize*DataSize];
  StoreLength = 1;
}

/* -------------------------------------------------- */

/*
 * Deinit: Shuts down the bank, deallocating all associated space.
 *         The bank cannot be used again without a call to Init().
 * Assumes: <none>
 */
void BlockMemBank::Deinit()
{
  if (StoreRoot != NULL)
  {
    Empty();

    // delete the remaining store
    delete [] StoreRoot->data;
    delete StoreRoot;
    StoreRoot = NULL;
  }
}

/* -------------------------------------------------- */

/*
 * GetCount: Returns the number of data items in the bank.
 * Assumes: <none>
 */
DWORD BlockMemBank::GetCount()
{
  return Count;
}

/* -------------------------------------------------- */

/*
 * NewData: Returns a pointer to a new data item.
 * Assumes: The bank has already been initialized.
 */
BYTE*  BlockMemBank::NewData()
{
  BYTE *bPtr;

  bPtr = NewNode();
  Count++;

  OpNum++;

  return bPtr;  
}

/* -------------------------------------------------- */

/*
 * AddNewData: Creates a new data item and places the values in 'data'
 *             into that new item.
 * Assumes: The bank has already been initialized, 'data' != NULL, 'data'
 *          refers to a data block as large as the data items used in the bank.
 */
void BlockMemBank::AddNewData(void *data)
{
  BYTE *bPtr;

  bPtr = NewData();
  memcpy(bPtr, data, DataSize);
}

/* -------------------------------------------------- */

/*
 * GetLastData: Returns a pointer to the last item already in the bank.
 * Assumes: The bank has already been initialized.
 */
BYTE*  BlockMemBank::GetLastData()
{
  if (Count == 0)
  {
    return NULL;
  }

  return FindNode(Count-1);
}

/* -------------------------------------------------- */

/*
 * RemoveOrder: Remove the data item from the bank in position 'pos'.
 * Assumes: The bank has already been initialized.
 */
BOOL32 BlockMemBank::RemoveOrder(DWORD pos)
{
  BYTE *node;

  if (pos >= Count)
  {
    return FALSE32;
  }

  node  = FindNode(pos);

  RemoveNode(node);

  return TRUE32;
}

/* -------------------------------------------------- */

/*
 * RemoveData: Removes the data in the bank associated with the provided pointer.
 * Assumes: The bank has already been initialized. The provided pointer is a valid
 *          pointer into the bank.
 */
void BlockMemBank::RemoveData(BYTE *node)
{
  RemoveNode(node);
}

/* -------------------------------------------------- */

/*
 * Empty: Remove all the data in the bank and contracts the bank down to a single block
 *        of space.
 * Assumes: The bank has already been initialized.
 */
void BlockMemBank::Empty()
{
	DWORD block;

  // remove the elements through direct access to the store
  for (block = 0; block < StoreLength; block++)
  {
    if (Store->prev != NULL)
    {
      delete [] Store->data;
      Store = Store->prev;
      delete Store->next;
      Store->next = NULL;
    }
  }

  OpNum++;

  // reset table values
  NextStorePos = 0;
  StoreLength = 1;
	Count       = 0;
}

/* -------------------------------------------------- */

/*
 * NewEnum: Returns a new Enumeration of the data in the bank that can
 *          be used to traverse all elements.
 *          The direction 'dir' determines the direction in which the data is traversed.
 * Assumes: The bank has already been initialized.
 */
Enumeration* BlockMemBank::NewEnum(DIRECTION dir)
{
  if (dir == FORWARD)
  {
    return (new BlockMemForwardEnum(this));
  }

  return (new BlockMemBackEnum(this));
}

/* -------------------------------------------------- */

/*
 * FindNode: Returns a pointer to the node in the bank with position 'pos'.
 * Assumes: The bank has already been initialized.
 */
BYTE* BlockMemBank::FindNode(DWORD pos)
{
  BMStoreNode *dnode;
  DWORD cnt, offset;

  dnode  = StoreRoot;
  cnt    = pos/StoreBSize;
  offset = (Count-1) - cnt*StoreBSize;

  while (cnt > 0)
  {
    dnode = dnode->next;
    cnt--;
  }

  return (dnode->data+offset*DataSize);
}

/* -------------------------------------------------- */

/*
 * RemoveNode: Completely removes a particular data element from the bank.
 * Assumes: The bank has already been initialized.
 */
void BlockMemBank::RemoveNode(BYTE *node)
{
  // adjust count
  Count--;

  DeleteNode(node);

  OpNum++;
}

/* -------------------------------------------------- */

/*
 * NewNode: Returns a pointer to a new data item in the bank, expanding the bank as needed.
 * Assumes: The bank has already been initialized.
 */
BYTE* BlockMemBank::NewNode()
{
  BYTE* retData;

  // grab the new node
  retData = &(Store->data[NextStorePos]);
  NextStorePos += DataSize;

  // if we're out of space, create a new data block
  if (NextStorePos >= StoreBSize*DataSize)
  {
    Store->next = new BMStoreNode;
    Store->next->prev = Store;
    Store = Store->next;
    Store->next = NULL;
    Store->data = new BYTE[StoreBSize*DataSize];
    NextStorePos = 0;
    StoreLength++;
  }

  return retData;
}

/* -------------------------------------------------- */

/*
 * DeleteNode: Deletes a particular data item from the bank and contracts
 *             the bank as needed.
 * Assumes: The bank has already been initialized.
 */
void BlockMemBank::DeleteNode(BYTE *node)
{
  DWORD pos;
  BYTE *tnode;
  BMStoreNode *slot;

  // determine where the true last used node is in the store
  if (NextStorePos == 0)
  {
    slot = Store->prev;
    pos  = DataSize*(StoreBSize-1);
  } else
  {
    slot = Store;
    pos  = NextStorePos-DataSize;
  }
  
  tnode = &(slot->data[pos]);

  if (node != tnode)
  {
    // copy data
    memcpy(tnode, node, DataSize);
  }

  // finish contracting the store

  // delete a node block if it's no longer needed
  if (slot != Store)
  {
    delete [] Store->data;
    Store = Store->prev;
    delete Store->next;
    Store->next = NULL;
    StoreLength--;
  }

  // adjust the next node pos
  NextStorePos = pos;
}

/* -------------------------------------------------- */
/* -------------------------------------------------- */

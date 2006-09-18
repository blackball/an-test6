#ifndef __BLOCKMEMBANK_HPP__
#define __BLOCKMEMBANK_HPP__

/* -------------------------------------------------- */
/* -------------------------------------------------- */

#include <MyTypes.h>
#include <Enumeration.hpp>

/* -------------------------------------------------- */
/* -------------------------------------------------- */

// default data store size
#define DEF_BLOCKMEM_SIZE     50

/* -------------------------------------------------- */
/* -------------------------------------------------- */

/*
 * DStoreNode: node that hold a segment of the data bank store. 
 */
struct BMStoreNode
{
  BYTE        *data;   // The list of nodes in this store
  BMStoreNode *next;   // The next store node
  BMStoreNode *prev;   // The previous store node
};

/* -------------------------------------------------- */
/* -------------------------------------------------- */

/*
 * BlockMemBank: A class for allocating and deallocating memory in blocks.
 *               The class is initialized with a data type size and a block size. 
 *               Memory can then be requested from the class and can be deleted by
 *               pointer or by position in the bank.
 *               On deletion, the data in the bank is manipulated so that the last
 *               item in the bank is removed. This means that any pointers to data
 *               in the bank may become incorrect when any data is deleted.
 *               Therefore, the bank came be safely used as a stack, however if an element
 *               that is not last in the bank is to be deleted the user will need
 *               to swap the data they intend to delete with the data in the last element 
 *               of the bank.
 */
class BlockMemBank
{
// Allow enumerations of the data in the bank.
friend class BlockMemForwardEnum;
friend class BlockMemBackEnum;

protected:
  DWORD Count;                  // number of elements in the bank

  // Store vars -- used to manage BMStoreNodes
  BMStoreNode *StoreRoot;       // First node in the store
  BMStoreNode *Store;           // Last node in the store
  DWORD NextStorePos;           // The next position available in the store
  DWORD StoreBSize;             // Size of blocks in the store
  DWORD StoreLength;            // Number of store nodes in the store
  DWORD DataSize;               // Number of blocks in the bank

  // enum vars
	DWORD OpNum;    		          // Status value used by the enumerations

public:
  BlockMemBank();
  BlockMemBank(DWORD dataSize, DWORD storebsize);
  ~BlockMemBank();

  // Initialize the bank with a particular data size and items per block
  void   Init(DWORD dataSize = 1, DWORD storebsize = DEF_BLOCKMEM_SIZE);
  // Shutdown the bank
  void   Deinit();

  // Get the number of data elements in the bank.
  DWORD  GetCount();
  // Get a pointer to a new data item.
  BYTE*  NewData();
  // Add values directly into a new data item.
  void   AddNewData(void *data);
  // Return the last item in the bank.
  BYTE*  GetLastData();
  // Remove a particular item from the bank by pointer
  void   RemoveData(BYTE *data);
  // Remove a particular item in the bank by position
  BOOL32 RemoveOrder(DWORD pos);
  // Empty the data in the bank, but keep the current data size and block size
  void   Empty();

  // Create a new enumeration for examining the content of the bank.
  Enumeration* NewEnum(DIRECTION dir);

protected:
  BYTE* FindNode(DWORD pos);
  void  RemoveNode(BYTE *node);
  BYTE* NewNode();
  void  DeleteNode(BYTE *node);
};

/* -------------------------------------------------- */
/* -------------------------------------------------- */

#endif

/**
 * This file defines a class that can assemble small packets
 * of data into one continuous buffer
 */
#ifndef ASSEMBLER_H_
#define ASSEMBLER_H_

#include <list>
class CAssembler {
public:
    CAssembler();
    virtual ~CAssembler();
    /** @brief clears the internal buffers */
    void Clear();
    /** @brief returns the total number of bytes held by this instance */
    unsigned Size() {
        return m_Size;
    };
    /** @brief appends a block to the list of maintained blocks */
    unsigned Append(unsigned char *pData,unsigned length);
    /** @brief Returns a specified number of bytes at the given offset */
    bool Peek(unsigned char *pBuffer,unsigned size,unsigned offset=0);
    /** @brief Gets data and removes it from the head of the list */
    bool Pop(unsigned char *pBuffer,unsigned size);
    /** @brief removes data from the head of the blocks */    
    void Trim(unsigned size);
protected:
    /** information about each block */
    typedef struct {
        unsigned length;
        unsigned char *pData; ///< Pointer to the start of the data. This could be some bytes into the buffer
        unsigned char *pBuffer; ///< Pointer to the buffer
    }
    BlockInfo_t;
    /** list of blocks we are holding */
    typedef std::list<BlockInfo_t> BlockList_t;

    /** total number of bytes */
    unsigned m_Size;
    /** data list */
    BlockList_t m_Blocks;
};

#endif /*CASSEMBLER_H_*/

/**
 * This file implements a class that can assemble small packets
 * of data into one continuous buffer
 */
#include "BufferAssembler.h"
#include <string.h>

/**
 * Class constructor 
 */
BufferAssembler::BufferAssembler() {
    m_Size=0;
}

/**
 * Class destructor
 */
BufferAssembler::~BufferAssembler() {
    Clear();
}

/**
 * Clear the block list and frees all the resources 
 */
void BufferAssembler::Clear() {
    BlockList_t::iterator it;
    /** free all the blocks */
    for(it=m_Blocks.begin(); it != m_Blocks.end(); it++) {
        delete [] it->pBuffer;
    }
    m_Blocks.clear();
    m_Size=0;
}


/**
 * Adds a block to the list of maintained blocks
 * @param[in] pData pointer to data buffer
 * @param[in] length length of the data buffer
 * @return the new size of the list
 * @note Data will be copied from the input data buffer into internal structures 
 **/
unsigned BufferAssembler::Append(const unsigned char *pData,unsigned length) {
    BlockInfo_t info={0};

    info.pBuffer=new unsigned char[length];
    info.pData=info.pBuffer;
    info.length=length;

    memcpy(info.pData,pData,length);

    m_Blocks.push_back(info);

    m_Size = m_Size + length;

    return m_Size;
}

/**
 * Retrieves data from the block list
 * @param[out] pBuffer pointer to buffer to receive the data
 * @param[in] size number of bytes to retrieve
 * @param[in] offset the starting offset 
 * @retval true if success
 * @retval false if offset or the size cannot be satisfied
 **/
bool BufferAssembler::Peek(unsigned char *pBuffer,unsigned size,unsigned offset) {
    BlockList_t::iterator it,start,end;
    unsigned depth=0,startOffset,endOffset;
    unsigned nBytes=0,copyByteCount;
    unsigned char *pAddr;

    //check for bad inputs
    if((offset+size) > m_Size || size == 0) {
        return false;
    }

    //walk the block list and find the starting block
    for(it=m_Blocks.begin(); it != m_Blocks.end(); it++) {
        start=it;
        if ( depth + it->length  > offset) {
            break;
        } else {
            depth = depth + it->length;
        }
    }
    startOffset=offset-depth;

    depth=0;
    //walk the block list and find the end block
    for(it=m_Blocks.begin(); it != m_Blocks.end(); it++) {
        end=it;

        if ( depth + it->length  >= offset+size ) {
            break;
        } else {
            depth += it->length;
        }
    }
    endOffset=(offset+size)-depth;

    //if the whole buffer is in a single block
    if(start == end) {
        pAddr=start->pData+startOffset;
        memcpy(pBuffer,pAddr,size);
    } else {
        //copy the beginning
        it=start;
        nBytes=0;
        copyByteCount= it->length - startOffset;
        pAddr=it->pData+startOffset;
        memcpy(pBuffer,pAddr,copyByteCount);
        nBytes+=copyByteCount;
        pBuffer+=copyByteCount;
        it++;
        //copy the middle blocks
        for(;it!=end;it++) {
            memcpy(pBuffer,it->pData,it->length);
            nBytes+=it->length;
            pBuffer+=it->length;
        }
        //copy the end
        copyByteCount= endOffset;
        pAddr=it->pData;
        memcpy(pBuffer,pAddr,copyByteCount);
        nBytes+=copyByteCount;
    }

    return true;
}

/**
 * Gets data and removes it from the head of the list
 * @param[out] pBuffer buffer to hold the data.
 * @param[in] size number of bytes to get
 * @retval true if success
 * @retval false if size of bogus 
 **/
bool BufferAssembler::Pop(unsigned char *pBuffer,unsigned size) {
    if(Peek(pBuffer,size)) {
        Trim(size);
        return true;
    }
    return false;
}

/**
 * Removes data from the head of the blocks 
 * @param[in] size number of byte to remove form the head of the list
 **/
void BufferAssembler::Trim(unsigned size) {
    BlockList_t::iterator it,temp;

    m_Size=m_Size-size;

    //walk the block list and find the starting block
    for(it=m_Blocks.begin(); it != m_Blocks.end() && size > 0;) {
        //if the size is bigger than the whole block, free it and move on
        if(size >= it->length) {
            size = size - it->length;
            delete [] it->pBuffer;
            temp=it;
            it++;
            m_Blocks.erase(temp);
        } else {
            //we are just trimming a bit off the start
            it->length = it->length - size;
            //advance the data pointer
            it->pData = it->pData+size;
            //last block
            size=0;
        }
    }
}


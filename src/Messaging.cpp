/**
 * @file Messaging.cpp
 *
 * @date   Mar 12, 2014
 * @author ali
 */

#include "Messaging.h"
#include <unistd.h>
#include <boost/log/trivial.hpp>

#define mSleep(x) (usleep(x*1000))

/**
 * Default constructor
 */
Messaging::Messaging() {
    m_uSendRetry = SEND_RETRY;
    m_uSendRetryDelay = SEND_RETRY_DELAY;
}

/**
 * Class destructor
 */
Messaging::~Messaging() {

}

/**
 * Low level transmit function that can handle partial message transfers.
 * This function will return an error if one of the following occurs:
 * 1) The xmitMsg function returns an error
 * 2) The xmitMsg function fails to transmit any data after several retries
 *
 *  @param pBuffer pointer to the message contents to be sent. If this is a
 *         partial message, the pointer should be already advanced to the
 *         next chunk.
 *  @param uLength number of bytes in the message
 *  @retval true success
 *  @retval false if the message cannot be queued
 **/
bool Messaging::xmitWithRetry(const unsigned char *pBuffer, unsigned uLength) {
    unsigned bytesSent = 0;
    unsigned retryCounter;
    int results;

    retryCounter=0;
    while(retryCounter < m_uSendRetry){
        results = xmitMsg(pBuffer + bytesSent, uLength - bytesSent);
        if(results < 0){
            BOOST_LOG_TRIVIAL(trace) << "Failed to transmit";
            return false;
        }
        if (results == 0) {
            retryCounter++;
            mSleep(m_uSendRetryDelay);
        }
        else{
            //if we a single packet through
            //reset the retry counter
            retryCounter=0;
            bytesSent += results;
            if (bytesSent >= uLength) {
                return true;
            }
        }
    }
    BOOST_LOG_TRIVIAL(trace) << "Xmit Retry timeout";
    return false;
}

/**
 * Sends a message using the low level transmit class.
 * It will retry until the entire message can be queued
 * @param pBuffer Pointer to the buffer to be sent
 * @param uLength Number of bytes included in the message
 * @retval true Message was sent successfully
 * @retval false Message was not sent successfully
 */
bool Messaging::sendMessage(const unsigned char* pBuffer, unsigned uLength) {
    unsigned char header[HEADER_SIZE];
    unsigned char trailer[TRAILER_SIZE]={ETX};

    header[4] = (uLength>>0)  & 0xFF;
    header[3] = (uLength>>8)  & 0xFF;
    header[2] = (uLength>>16) & 0xFF;
    header[1] = (uLength>>24) & 0xFF;
    header[0] = STX;

    //send the size
    if(!xmitWithRetry(header,HEADER_SIZE)){
        return false;
    }
    //send the contents
    if(!xmitWithRetry(pBuffer,uLength)){
            return false;
    }
    //send the contents
    if(!xmitWithRetry(trailer,TRAILER_SIZE)){
            return false;
    }

    return true;
}

/**
 * Processes a chunk of data received from the remote end
 * @param pBuffer The buffer containing partial,entire. or multiple messages
 * @param uLength The length of the buffer
 * @retval true At least one message was received after processing chunk
 * @retval false No complete message was received after processing chunk
 */
bool Messaging::processChunk(unsigned char* pBuffer, unsigned uLength) {
    unsigned char header[HEADER_SIZE];
    unsigned char trailer[TRAILER_SIZE];
    unsigned uMsgLength;
    Message_t msg;
    bool bResults=false;

    //put the chunk on the list
    m_assembler.Append(pBuffer,uLength);

    for(;;){
        if(m_assembler.Size() < HEADER_SIZE){
            //not enough data for header
            break;
        }
        //see if we can process the header
        m_assembler.Peek(header,HEADER_SIZE,0);
        if(header[0]!=STX){
            //bad message or partial message
            //dump everything in the assember in an attempt to recover
            BOOST_LOG_TRIVIAL(trace) << "Bad Start of message";
            m_assembler.Clear();
            break;
        }
        uMsgLength= (unsigned)(header[1]) << 24 |
                    (unsigned)(header[2]) << 16 |
                    (unsigned)(header[3]) << 8  |
                    (unsigned)(header[4]);
        //do we have a complete message?
        if(m_assembler.Size() < uMsgLength+HEADER_SIZE+TRAILER_SIZE){
            break;
        }
        //everything is here. Start pulling things off the assembler
        m_assembler.Trim(HEADER_SIZE);
        msg.pData=new unsigned char[uMsgLength];
        msg.uMsgLength=uMsgLength;
        m_assembler.Pop(msg.pData,msg.uMsgLength);

        //pull the trailer
        m_assembler.Pop(trailer,TRAILER_SIZE);
        //check the trailer
        if(trailer[0] != ETX){
            //oops! The trailer was not valid
            //dump the message
            delete[] msg.pData;
            BOOST_LOG_TRIVIAL(trace) << "Found bad trailer";
            break;
        }
        //put the message in the queue
        m_MsgQueue.push(msg);
        bResults=true;
    }

    return bResults;
}

/**
 * Returns the size of the message at the beginning of the queue
 * @return The size of the message at the beginning of the queue
 * @retval 0 If no messages are pending
 */
unsigned Messaging::getMsgSize() {
    if(!m_MsgQueue.empty()){
        return m_MsgQueue.front().uMsgLength;
    }
    return 0;
}

/**
 * Pops the message first message from the received
 * message queue
 * @return First message in the queue
 * @note This pointer must be deleted by the caller after the message is
 * successfully processed.
 */
Messaging::Message_t Messaging::getMsg() {
    Message_t msg;

    msg.pData=0;
    msg.uMsgLength=0;

    if(!m_MsgQueue.empty()){
        msg=m_MsgQueue.front();
        m_MsgQueue.pop();
    }

    return msg;
}



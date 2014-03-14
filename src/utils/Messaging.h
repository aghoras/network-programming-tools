/**
 * @file Messaging.h
 *
 * @date   Mar 12, 2014
 * @author ali
 */

#ifndef MESSAGING_H
#define MESSAGING_H
#include <string>
#include <queue>

#include "assembler.h"
/**
 * This class encapsulates the task of sending
 * and receiving messages over a streaming interface.
 * This class should serve as the base class
 * for concrete message classes.
 */
class CMessaging {
public:
    /** used to hold complete messages */
    typedef struct {
        unsigned char * pData;
        unsigned long uMsgLength;
    } Message_t;

protected:
    enum {SEND_RETRY=5};
    enum {SEND_RETRY_DELAY= 10};
    enum {STX=2,ETX=3}; //start of text, end of text characters
    enum {HEADER_SIZE=5};
    enum {TRAILER_SIZE=1};

    /** message queue */
    typedef std::queue <Message_t> MessageQueue_t;

    CAssembler     m_assembler;       /**< used to assemble data fragments */
    unsigned       m_uSendRetry;      /**< number of times to retry before giving up on sends */
    unsigned       m_uSendRetryDelay; /**< number of milliseconds to delay between send retries */
    MessageQueue_t m_MsgQueue;        /**< hold a list of completely received messages */

    /** low level transmit function
     *  @param pBuffer pointer to the message contents to be sent. If this is a
     *         partial message, the pointer should be already advanced to the
     *         next chunk.
     *  @param uLength number of bytes in the message
     *  @return number bytes that were successfully transmitted (can be zero)
     *  @retval -1 if the message cannot be queued due to an error
     **/
    virtual int xmitMsg(const unsigned char *pBuffer, unsigned uLength)=0;

    /** @brief calls the xmitMsg function and retries if it cannot queue the message */
    bool xmitWithRetry(const unsigned char *pBuffer, unsigned uLength);

public:
    CMessaging();
    virtual ~CMessaging();
    /** @brief sends a message to remote end */
    bool  sendMessage(const unsigned char *pMsg,unsigned uLength);
    /** @brief adds a chunk of data to internal buffer in order to extract message */
    bool  processChunk(unsigned char *pBuffer, unsigned uLength);
    /** @brief returns the size of the current message */
    unsigned getMsgSize();
    /** @brief returns the first message from the received queue */
    Message_t getMsg();
    /** @brief returns the number of fully received messages available */
    unsigned getMessageCount() {return m_MsgQueue.size();}
};

#endif /* MESSAGING_H */

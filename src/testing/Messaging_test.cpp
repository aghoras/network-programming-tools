/**
 * @file Messaging_test.cpp
 *
 * Unit test procedures for the messaging class.
 * @date   Mar 12, 2014
 * @author ali
 */

#include "Messaging.h"
#include <algorithm>

#include "gtest.h"



/**
 * Used to test messaging on a medium that
 * can handle all the data
 */
class transmitsAll:public CMessaging{
protected:
    CAssembler m_transmittedData; //this holds the the data we get

    virtual int xmitMsg(const unsigned char *pBuffer, unsigned uLength){
        m_transmittedData.Append(pBuffer,uLength);
        return (int)uLength;
    }

public:
    //returns all the data received so far
    //the pointer needs to be freed up by the caller
    unsigned char* getRawData(){
        unsigned char *pData=new unsigned char[getRawDataSize()];
        m_transmittedData.Pop(pData,getRawDataSize());
        return pData;
    }

    //returns the number bytes in the current buffer
    unsigned getRawDataSize(){
        return m_transmittedData.Size();
    }

};

/**
 * Used to test messaging on a medium that can
 * support very small chunks of data
 */
class partialTransmitter: public CMessaging{
protected:
    CAssembler m_transmittedData; //this holds the the data we get
    enum {PACKET_SIZE=4}; //number of bytes per packet
    unsigned m_NumPackets; //number of packets recieved so far
    unsigned m_FailedPacketInterval; //how often force a queue full condition
    unsigned m_bForceTransferError; //when set to true, the transfer function always returns -1

    /**
     * Function that emulates the low level transfer
     */
    virtual int xmitMsg(const unsigned char *pBuffer, unsigned uLength){

        if(m_bForceTransferError){
            return -1;
        }

        m_NumPackets++;
        if(m_NumPackets % m_FailedPacketInterval == 0){
            return 0;
        }
        uLength=std::min(uLength,(unsigned)PACKET_SIZE);
        m_transmittedData.Append(pBuffer,uLength);

        return (int)uLength;
    }

public:
    partialTransmitter(){
        m_NumPackets=0;
        m_FailedPacketInterval=std::numeric_limits<unsigned>::max();
        m_bForceTransferError=false;
    }

    //enables/disables force error
    void setForceError(bool value){
        m_bForceTransferError=value;
    }
    //returns all the data received so far
    //the pointer needs to be freed up by the caller
    unsigned char* getRawData(){
        unsigned char *pData=new unsigned char[getRawDataSize()];
        m_transmittedData.Pop(pData,getRawDataSize());
        return pData;
    }

    //returns the number bytes in the current buffer
    unsigned getRawDataSize(){
        return m_transmittedData.Size();
    }

    void setFailedPacketInterval(unsigned interval){
        m_FailedPacketInterval=interval;
    }
};


/**
 * Simple Transfer Test
 */
TEST(fullTransmiter,simpleTransmitTest){
    transmitsAll t;
    const unsigned messageLength=1000000;
    const char *szTestMsg="Hello world";
    unsigned char transmitBuffer[messageLength];
    unsigned char *pRawData;
    CMessaging::Message_t msg;
    unsigned rawDataSize;

    strncpy((char*)transmitBuffer,szTestMsg,messageLength);
    ASSERT_EQ(t.getMsgSize(),(unsigned)0);
    //send some data
    ASSERT_TRUE(t.sendMessage(transmitBuffer,messageLength));
    //
    //Feed back the data into the processor
    //
    rawDataSize=t.getRawDataSize();
    pRawData=t.getRawData();
    //feed the data back to the processor
    //an entire message should be received
    ASSERT_TRUE(t.processChunk(pRawData,rawDataSize));
    //There should be only one message
    ASSERT_EQ(t.getMessageCount(), (unsigned)1);
    ASSERT_EQ(t.getMsgSize(), messageLength);
    //check the message content
    msg=t.getMsg();
    ASSERT_EQ(strcmp((char*)msg.pData,szTestMsg),0);
    ASSERT_EQ(msg.uMsgLength, messageLength);
    //there should be no more messages
    ASSERT_EQ(t.getMessageCount(), (unsigned)0);

    delete[] msg.pData;
    delete[] pRawData;
}

/**
 * Multiple messages received at once
 */
TEST(fullTransmiter,multipleSimpleTransmitTest){
    transmitsAll t;
    const unsigned messageLength=200;
    const char *szTestMsg1="Hello world";
    const char *szTestMsg2="I'm a traveler of both time and space";
    unsigned char transmitBuffer[messageLength];
    unsigned char *pRawData;
    CMessaging::Message_t msg;
    unsigned rawDataSize;

    strncpy((char*)transmitBuffer,szTestMsg1,messageLength);
    t.sendMessage(transmitBuffer,messageLength);
    strncpy((char*)transmitBuffer,szTestMsg2,messageLength);
    t.sendMessage(transmitBuffer,messageLength);

    //
    //Feed back the data into the processor
    //
    rawDataSize=t.getRawDataSize();
    pRawData=t.getRawData();
    //feed the data back to the processor
    //an entire message should be received
    ASSERT_TRUE(t.processChunk(pRawData,rawDataSize));
    //There should two messages
    ASSERT_EQ(t.getMessageCount(), (unsigned)2);
    //check the message contents of the first message
    msg=t.getMsg();
    ASSERT_EQ(strcmp((char*)msg.pData,szTestMsg1),0);
    ASSERT_EQ(msg.uMsgLength, messageLength);
    //there should be no more messages
    ASSERT_EQ(t.getMessageCount(), (unsigned)1);
    delete[] msg.pData;
    delete[] pRawData;

    //check the message contents of the second message
    msg=t.getMsg();
    ASSERT_EQ(strcmp((char*)msg.pData,szTestMsg2),0);
    ASSERT_EQ(msg.uMsgLength, messageLength);
    //there should be no more messages
    ASSERT_EQ(t.getMessageCount(), (unsigned)0);
    delete[] msg.pData;

}

/**
 * First message is received completely but the second message
 * is broken up
 */
TEST(fullTransmiter,partialMessageReceived){
    transmitsAll t;
    const unsigned messageLength=200;
    const unsigned firstRxChunkSize=220;
    unsigned secondRxChunkSize;
    const char *szTestMsg1="Hello world";
    const char *szTestMsg2="I'm a traveler of both time and space";
    unsigned char transmitBuffer[messageLength];
    unsigned char *pRawData;
    CMessaging::Message_t msg;
    unsigned rawDataSize;

    strncpy((char*)transmitBuffer,szTestMsg1,messageLength);
    t.sendMessage(transmitBuffer,messageLength);
    strncpy((char*)transmitBuffer,szTestMsg2,messageLength);
    t.sendMessage(transmitBuffer,messageLength);

    //
    //Feed back the data into the processor
    //
    rawDataSize=t.getRawDataSize();
    secondRxChunkSize=rawDataSize-firstRxChunkSize;
    pRawData=t.getRawData();
    //feed the data back to the processor but split
    //it over two
    ASSERT_TRUE(t.processChunk(pRawData,firstRxChunkSize));
    //There should two messages
    ASSERT_EQ(t.getMessageCount(), (unsigned)1);
    //check the message contents of the first message
    msg=t.getMsg();
    ASSERT_EQ(strcmp((char*)msg.pData,szTestMsg1),0);
    ASSERT_EQ(msg.uMsgLength, messageLength);
    //there should be no more messages
    ASSERT_EQ(t.getMessageCount(), (unsigned)0);
    delete[] msg.pData;

    //process the rest of the data
    ASSERT_TRUE(t.processChunk(pRawData+firstRxChunkSize,secondRxChunkSize))
        << "Did not get complete message after processing second chunk\n";
    //There should two messages
    ASSERT_EQ(t.getMessageCount(), (unsigned)1);
    //check the message contents of the first message
    msg=t.getMsg();
    ASSERT_EQ(strcmp((char*)msg.pData,szTestMsg2),0);
    ASSERT_EQ(msg.uMsgLength, messageLength);
    //there should be no more messages
    ASSERT_EQ(t.getMessageCount(), (unsigned)0);
    delete[] msg.pData;

    delete[] pRawData;

}


/**
 * Simple Transfer Test for small packet
 * mediums
 */
TEST(partialTransmitter,simpleTransmitTest){
    partialTransmitter t;
    const unsigned messageLength=1000000;
    const char *szTestMsg="Hello world";
    unsigned char transmitBuffer[messageLength];
    unsigned char *pRawData;
    CMessaging::Message_t msg;
    unsigned rawDataSize;

    strncpy((char*)transmitBuffer,szTestMsg,messageLength);

    //send some data
    ASSERT_TRUE(t.sendMessage(transmitBuffer,messageLength));
    //
    //Feed back the data into the processor
    //
    rawDataSize=t.getRawDataSize();
    pRawData=t.getRawData();
    //feed the data back to the processor
    //an entire message should be received
    ASSERT_TRUE(t.processChunk(pRawData,rawDataSize));
    //There should be only one message
    ASSERT_EQ(t.getMessageCount(), (unsigned)1);
    //check the message content
    msg=t.getMsg();
    ASSERT_EQ(strcmp((char*)msg.pData,szTestMsg),0);
    ASSERT_EQ(msg.uMsgLength, messageLength);
    //there should be no more messages
    ASSERT_EQ(t.getMessageCount(), (unsigned)0);

    delete[] msg.pData;
    delete[] pRawData;
}

/**
 * Simple Transfer Test for small packet mediums.
 * This test checks proper operation when the medium
 * reports a full condition
 */
TEST(partialTransmitter,withRetries){
    partialTransmitter t;
    const unsigned messageLength=1000000;
    const char *szTestMsg="Hello world";
    unsigned char transmitBuffer[messageLength];
    unsigned char *pRawData;
    CMessaging::Message_t msg;
    unsigned rawDataSize;

    strncpy((char*)transmitBuffer,szTestMsg,messageLength);
    t.setFailedPacketInterval(5000);

    //send some data
    ASSERT_TRUE(t.sendMessage(transmitBuffer,messageLength));
    //
    //Feed back the data into the processor
    //
    rawDataSize=t.getRawDataSize();
    pRawData=t.getRawData();
    //feed the data back to the processor
    //an entire message should be received
    ASSERT_TRUE(t.processChunk(pRawData,rawDataSize));
    //There should be only one message
    ASSERT_EQ(t.getMessageCount(), (unsigned)1);
    //check the message content
    msg=t.getMsg();
    ASSERT_EQ(strcmp((char*)msg.pData,szTestMsg),0);
    ASSERT_EQ(msg.uMsgLength, messageLength);
    //there should be no more messages
    ASSERT_EQ(t.getMessageCount(), (unsigned)0);

    delete[] msg.pData;
    delete[] pRawData;
}

/**
 * Test various vailure modes
 */
TEST(partialTransmitter,expectedFailures){
    partialTransmitter t;
    const unsigned messageLength=1000;
    const char *szTestMsg="Hello world";
    unsigned char transmitBuffer[messageLength];
    unsigned char *pRawData;
    unsigned rawDataSize;

    strncpy((char*)transmitBuffer,szTestMsg,messageLength);

    //force transfer failure
    t.setForceError(true);
    //send some data which should fail
    ASSERT_FALSE(t.sendMessage(transmitBuffer,messageLength));
    //clear the error
    t.setForceError(false);

    //makextransfer to fail on every attempt
    t.setFailedPacketInterval(1);
    //send some data which should fail
    ASSERT_FALSE(t.sendMessage(transmitBuffer,messageLength));
    t.setFailedPacketInterval(1000000);

    //send data successfully
    ASSERT_TRUE(t.sendMessage(transmitBuffer,messageLength));
    rawDataSize=t.getRawDataSize();
    pRawData=t.getRawData();
    //mangle header
    pRawData[0]=0;
    ASSERT_FALSE(t.processChunk(pRawData,rawDataSize));
    //There should be only no messages
    ASSERT_EQ(t.getMessageCount(), (unsigned)0);
    delete[] pRawData;

    //send data successfully
    ASSERT_TRUE(t.sendMessage(transmitBuffer,messageLength));
    rawDataSize=t.getRawDataSize();
    pRawData=t.getRawData();
    //mangle length
    pRawData[4]=0;
    ASSERT_FALSE(t.processChunk(pRawData,rawDataSize));
    //There should be only no messages
    ASSERT_EQ(t.getMessageCount(), (unsigned)0);
    delete[] pRawData;



}


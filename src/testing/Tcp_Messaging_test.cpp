/**
 * @file Tcp_Messaging_test.cpp
 *
 * @date   Mar 14, 2014
 * @author ali
 */

#include <algorithm>
#include "gtest.h"
#include "tcp_server.h"
#include "tcp_messaging.h"


/**
 * This function is used to forward received data back to the message handler
 */
 static void helperFunction(CTcpServer::Handle_t handle,unsigned char *pData, unsigned uLength,void *pUser){
     CTcpMessaging *pDest=(CTcpMessaging *) pUser;

     pDest->processChunk(pData,uLength);
 }

/**
 * Test the general operation of the tcp_messaging class
 *
 */
TEST(TcpMessaging,general){
    const unsigned uPort=9450;
    const char *pTestMessage="Oh let the sun beat down upon my face, stars to fill my dream";
    CTcpMessaging src,dest;
    CTcpServer server(uPort);
    CMessaging::Message_t message;

    //start the server
    server.RegisterDataCallback(helperFunction,&dest);
    ASSERT_TRUE(server.StartSeverThread());
    sleep(1);

    //connect to the server
    EXPECT_TRUE(src.connect("127.0.0.1",uPort));
    //send a message to the destination
    src.sendMessage((const unsigned char*)pTestMessage,(unsigned)strlen(pTestMessage)+1);
    sleep(1);
    EXPECT_TRUE(dest.getMessageCount());
    message=dest.getMsg();
    EXPECT_STREQ((char*)message.pData,pTestMessage);
    delete[] message.pData;

    server.StopSeverThread();
}


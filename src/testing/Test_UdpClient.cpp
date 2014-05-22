/** 
 * @brief Unit Test for the UDP client
 *
 * @date   May 22, 2014
 * @author ali
 * 
 * 
 */
#include "UdpClient.h"
#include "udp_server.h"
#include "Timer.h"
#include "gtest.h"
#include <list>
#include <boost/shared_ptr.hpp>
#include <boost/bind.hpp>
#include "logging.h"


/**
 * Call back used to register data
 */
static void dataCallback( std::list<std::string> *pList,unsigned char *pData,size_t size){
    std::string msg;

    msg.assign((char*)pData,size);
    pList->push_back(msg);
}

TEST(UdpClient,test1){
    LocalLogLevel ll(boost::log::trivial::trace);
    UdpServer server(1546);

    const char *szMessage="But unlike a child my tears dont help me get my way";
    std::list<std::string> rxMessageList;

    server.RegisterDataCallback(boost::bind(dataCallback,&rxMessageList,_1,_2));

    server.StartServerThread();
    UdpClient client(1546,"127.0.0.1");
    EXPECT_TRUE(client.Send((unsigned char *)szMessage,strlen(szMessage)));
    usleep(1000);
    ASSERT_EQ(rxMessageList.size(),1u);
    EXPECT_EQ(szMessage,rxMessageList.front());
    server.StopServerThread();
}

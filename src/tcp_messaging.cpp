/**
 * @file tcp_messaging.cpp
 *
 * @date   Mar 14, 2014
 * @author ali
 */

#include "tcp_messaging.h"
#include "TRACE.h"
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

using namespace std;

CTcpMessaging::CTcpMessaging() {
    m_socket=-1;
    m_sIpAddress="127.0.0.1";
    m_uPortNumber=8080;

}

CTcpMessaging::~CTcpMessaging() {
   if(m_socket != -1){
       disconnect();
   }
}


/** low level transmit function
 *  @param pBuffer pointer to the message contents to be sent. If this is a
 *         partial message, the pointer should be already advanced to the
 *         next chunk.
 *  @param uLength number of bytes in the message
 *  @return number bytes that were successfully transmitted (can be zero)
 *  @retval -1 if the message cannot be queued due to an error
 **/
int CTcpMessaging::xmitMsg(const unsigned char *pBuffer, unsigned uLength){
    int nResults;
    if(m_socket < 0){
        return -1;
    }

    nResults=send(m_socket, pBuffer, uLength, 0);
    if(nResults <0){
        PERROR1("send socket failed. Reason: %s\n ",strerror(errno));
    }

    return nResults;
}

/**
 * Connects to a TCP server (blocking)
 * @param sIpAddress Address of the server
 * @param uPort Port address to connect to
 * @retval true Connections successful
 * @retval false Connection failed
 */
bool CTcpMessaging::connect(string sIpAddress, unsigned uPort) {
    struct sockaddr_in serverAddr;
    int nResults;

    m_sIpAddress=sIpAddress;
    m_uPortNumber=uPort;

    memset(&serverAddr,0,sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(m_uPortNumber);

    if(inet_pton( AF_INET,m_sIpAddress.c_str(),&serverAddr.sin_addr) <=0){
        PTRACE1("Could not convert address %s to a network address\n",m_sIpAddress.c_str());
        return false;
    }

    m_socket=socket(AF_INET,SOCK_STREAM,0);
    if(m_socket < 0){
        PTRACE("Failed to create socket\n");
        return false;
    }

    nResults=::connect(m_socket,(struct sockaddr *)&serverAddr,sizeof(serverAddr));
    if(nResults < 0){
        PTRACE1("Failed to connect to server. Reason:  %s\n", strerror(errno) );
        close(m_socket);
        m_socket=-1;
        return false;
    }

    return true;

}

/**
 * Disconnects from the server
 */
void CTcpMessaging::disconnect() {
    if(m_socket != -1){
        shutdown(m_socket,SHUT_RDWR);
        close(m_socket);
        m_socket=-1;
    }
}

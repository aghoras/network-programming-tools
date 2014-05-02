#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>

#ifdef WIN32
#   include <winsock2.h>
#   define close closesocket
#   define SHUT_RDWR SD_BOTH
#   define socklen_t int
#elif defined(OS_VXWORKS)
#   include "sched.h"
#   include "sockLib.h"
#   include "inetLib.h"
#   include "hostLib.h"
#   include "ioLib.h"
#   include "ioctl.h"
#   include "types.h"
#   include "socket.h"
#   include <netinet/tcp.h>
#   define SHUT_RDWR 2
#else
#   include <sys/socket.h>
#   include <netinet/in.h>
#   include <netinet/tcp.h>
#   include <netdb.h>
#   include <arpa/inet.h>
#   include <unistd.h>
#endif

#include <cctype>

#include <fcntl.h>
#include <ctime>
#include <signal.h>
#include <pthread.h>
#include <list>
#include <errno.h>
#include <assert.h>
#include "udp_server.h"
#include <boost/log/trivial.hpp>
#include <stdexcept>

using namespace std;

/**
 * Class constructor
 * @param uPort The port to receive on
 */
CUdpServer::CUdpServer(unsigned uPort)
{
    m_uPort=uPort;
    m_bBiDirectional = false;
    if(initServer() == false){
        throw new runtime_error("Could not init server");
    }
}

/**
 * Class constructor for connected socket
 * @param uSendPort The port number to send to
 * @param uReceivePort The port to receive on
 * @param serverAddress The address of the server to connect to
 */
CUdpServer::CUdpServer(unsigned uSendPort,string serverAddr,unsigned uReceivePort)
{
    struct sockaddr_in sin;

    m_uPort=uReceivePort;
    m_bBiDirectional = true;
    if(initServer() == false){
        throw new runtime_error("Could not init server");
    }
    
    memset(&sin,0,sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr =  inet_addr(serverAddr.c_str());;
    sin.sin_port = htons(uSendPort); 

    BOOST_LOG_TRIVIAL(trace) << "Connecting to client..";
    if(connect(m_Socket,(sockaddr*)&sin,sizeof(sin)) < 0){
#       ifndef WIN32
            int err=errno;
#       else
            int err=WSAGetLastError();
#       endif
        BOOST_LOG_TRIVIAL(error) << "Error during connect:  " << strerror(err);
        close(m_Socket);
        m_Socket=-1;
        m_bBiDirectional =false;
        throw new runtime_error("Could not connect to client");
    }
    BOOST_LOG_TRIVIAL(trace) << "Done with client initialization";
}

CUdpServer::~CUdpServer(void)
{
    if(m_Socket != -1){
        shutdown(m_Socket,SHUT_RDWR);
        close(m_Socket);
        m_Socket = -1;
    }
#   ifdef WIN32
    WSACleanup();
#   endif
}

/**
 * Sends data to connected client. This function is only valid for bi-directional servers
 * @param pData pointer to data buffer to send
 * @param uLength Length of data buffer
 * @retval true Success
 * @retval false failure
 */
bool CUdpServer::SendToClient(const unsigned char *pData,unsigned uLength) const{
    if(m_bBiDirectional == false){
        BOOST_LOG_TRIVIAL(error) << "Cannot send data to unidirectional client\n";
    }

    if(send(m_Socket,(char*)pData,uLength,0) < 0){
        return false;
    }

    return true;
}

/**
 * Initializes the server socket 
 * @retval true Success
 * @retval false failure
 */
bool CUdpServer::initServer(){
    struct sockaddr_in sin;
    
    
    /* Used so we can re-bind to our port while a previous connection is still in TIME_WAIT state. */
    int reuse_addr = 1;

    m_pNewDataCallback=NULL;
    m_pNewDataUser=NULL;

    memset(&sin,0,sizeof(sin));
#   ifdef WIN32
    //winsock intialization stuff
    WORD wVersionRequested;
    WSADATA wsaData;

    wVersionRequested = MAKEWORD( 2, 2 );
    if(WSAStartup( wVersionRequested, &wsaData )){
        return false;
    }
#   endif

    //create the receive socket
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = htons(m_uPort);    
    m_Socket=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
    if (m_Socket == -1) {
#       ifndef WIN32
            int err=errno;
#       else
            int err=WSAGetLastError();
#       endif
        BOOST_LOG_TRIVIAL(error) << "Error during socket creation: Errno: " <<  err;
        perror("socket");
        return false;
    }
    /* So that we can re-bind to it without TIME_WAIT problems */
    BOOST_LOG_TRIVIAL(trace) << "Setting socket options...";
    setsockopt(m_Socket, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse_addr, sizeof(reuse_addr));

    // bind to the interface
    BOOST_LOG_TRIVIAL(trace) << "Binding to interface...";
    if (::bind(m_Socket,(struct sockaddr *)&sin,sizeof(sin)) < 0) {
#       ifndef WIN32
            int err=errno;
#       else
            int err=WSAGetLastError();
#       endif
        BOOST_LOG_TRIVIAL(error) << "Error during bind: Errno: " << err;
        perror("bind");
        close(m_Socket);
        m_Socket=-1;
        return false;
    }

#   ifndef WIN32
    BOOST_LOG_TRIVIAL(trace) << "Setting signals...";
    //this prevents writing to closed socket from seq faulting the process
    signal(SIGPIPE, SIG_IGN);
#   endif

    BOOST_LOG_TRIVIAL(trace) <<  "Done with server initialization...";
    return true;
}

/**
 * Registers a callback function for new data
 * @param pCallback Pointer to Callback function
 * @param pUser Pointer to user provided pointer passed back into the callback function
 */
void CUdpServer::RegisterDataCallback(DataCallback_t pCallback){
    m_pNewDataCallback=pCallback;
}

/** 
 * Starts the server and does not return until the socket is shutdown
 * @retval true sucess
 * @retval false failure
 */
bool CUdpServer::start(){
    std::time_t now=time(0);
    unsigned char buffer[64*1024];
    int buffer_length=sizeof(buffer);
    int nResults=0;

    BOOST_LOG_TRIVIAL(trace) <<  "Server on port " << m_uPort << " started at " << ctime(&now);
    //do we have good socket to listen over
    if(m_Socket == -1) {
        //bad socket
        return false;
    }
    do{
        nResults=recv(m_Socket,(char*)buffer,buffer_length,0);
        //check the results
        if(nResults > 0 && m_pNewDataCallback != NULL){
            //give the data to the user
            //m_pNewDataCallback(buffer,nResults,m_pNewDataUser);
            m_pNewDataCallback(buffer,nResults);
        }
    }while(1);
    
    //shut downs are ok.. that's how we stop the receive
    if(nResults < 0){
#       ifndef WIN32
            int err=errno;
#       else
            int err=WSAGetLastError();
#       endif
        BOOST_LOG_TRIVIAL(error) << "Error during recv: Errno: << " << strerror(err);
        close(m_Socket);
        m_Socket = -1;
        return false;
    }

    return true;
}

/** 
 * This function starts a thread and calls the start function 
 */
bool CUdpServer::StartServerThread(){
    return ( pthread_create(&m_threadId,NULL,udp_server_thread_helper,this) == 0);
}

/**
 * This function stops the server thread 
 */
bool CUdpServer::StopServerThread(){     
    return (pthread_cancel(m_threadId) == 0);
}

/**
 * Helper function for running the thread
 * @param pUser pointer to class instance
 * @return not used
 */
void * udp_server_thread_helper(void *pUser){
    CUdpServer *pServer = (CUdpServer*)pUser;

    pServer->start();

    return NULL;
}

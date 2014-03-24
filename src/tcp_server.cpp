/**
 * This server is a simple TCP server class
 * @author AG
 * @date 3/4/2007 
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#ifdef WIN32
#   include <winsock2.h>
#   define close closesocket
#   define SHUT_RDWR SD_BOTH
#   define socklen_t int
#else
#   include <sys/socket.h>
#   include <netinet/in.h>
#   include <netinet/tcp.h>
#   include <netdb.h>
#   include <arpa/inet.h>
#   include <unistd.h>
#endif

#include <cctype>
#include "tcp_server.h"
#include <fcntl.h>
#include <ctime>
#include <signal.h>
#include <pthread.h>
#include <list>
#include <errno.h>
#include <assert.h>
#define _SUPRESS_TRACE
#include "TRACE.h"

/** cygwin hack */
#ifndef MSG_WAITALL
#       define MSG_WAITALL 0
#endif

/** enable/disable Nagle's algorithm */
#define DISABLE_NAGLE


/**
 * Calls constructor
 * @param uPort port number or listen over
 */
CTcpServer::CTcpServer(unsigned uPort) {
    struct sockaddr_in sin;

    /* Used so we can re-bind to our port while a previous connection is still in TIME_WAIT state. */
    int reuse_addr = 1;
    
    memset(&sin,0,sizeof(sin));
#   ifdef WIN32
    //winsock initialization stuff
    WORD wVersionRequested;
    WSADATA wsaData;
    
    wVersionRequested = MAKEWORD( 2, 2 ); 
    if(WSAStartup( wVersionRequested, &wsaData )){   
        return;
    }
#   endif
    m_uPort=uPort;
    m_ListenSocket=-1;
    m_pConnectionCallback= NULL;
    m_pNewDataCallback=NULL;

    pthread_mutex_init(&m_ClientListMutex, NULL);// PTHREAD_MUTEX_RECURSIVE);

    //clear the selector list
    FD_ZERO(&m_ReadSocks);
    FD_ZERO(&m_ErrorSocks);

    //create the listen socket
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = htons(m_uPort);
    //first socket is the listen socket
    m_ListenSocket=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    if (m_ListenSocket == -1) {
#       ifndef WIN32
            int err=errno;
#       else
            int err=WSAGetLastError();
#       endif
        PERROR1("Error during socket creation: Errno: %d\n",err);
        perror("socket");        
        return;
    }
    /* So that we can re-bind to it without TIME_WAIT problems */
    PTRACE("Setting socket options..\n");
    setsockopt(m_ListenSocket, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse_addr, sizeof(reuse_addr));
    PTRACE("Setting non blocking socket options..\n");
    SetNoBlocking(m_ListenSocket);

    // bind to the interface
    PTRACE("Binding to interface..\n");
    if (bind(m_ListenSocket,(struct sockaddr *)&sin,sizeof(sin)) == -1) {
        int err=errno;
        PERROR1("Error during bind: Errno: %d\n",err);
        perror("bind");
        close(m_ListenSocket);
        m_ListenSocket=-1;
        return;
    }
    
#   ifndef WIN32
    PTRACE("Setting signals..\n");
    //this prevents writing to closed socket from seq faulting the process
    signal(SIGPIPE, SIG_IGN);
#   endif

    PTRACE("Done with basic initialization..\n");
}

/**
 * Starts the server, waits for incoming connections and executes commands
 * @retval true if successful
 * @retval false if error
 * @note this function blocks until a kill command is received
 */
bool CTcpServer::start() {
    int nFds=0;
    int numSelected=0;
    ClientList_t::iterator connection;
    std::time_t now=time(0);
    std::list<Handle_t> close_list;
    std::list<Handle_t> data_list;


    PTRACE1("Server started at %s\n",ctime(&now));
    //do we have good socket to listen over
    if(m_ListenSocket == -1) {
        //bad socket
        return false;
    }
    //wait for the connection
    if (listen(m_ListenSocket,MAX_CONNECTIONS) == -1) {
        int err=errno;
        PERROR1("Error during listen: Errno: %d\n",err);
        perror("listen");
        return false;
    }

    while(  m_ListenSocket != -1) {
        //need to build the FD list every time        
        nFds=BuildSelectList();        
        if(nFds < 1) {
            return false;
        }
        //If there are any connections that need to be removed, now is a good time
        if(close_list.size() > 0) {
            //Remove the connections marked for deletion
            for(std::list<Handle_t>::iterator it=close_list.begin();
                    it!= close_list.end();
                    it++) {
                now=time(0);
                PTRACE1("Client disconnected at %s\n",ctime(&now));
                CloseConnectionCallback(*it);
                pthread_mutex_lock(&m_ClientListMutex);
                m_ClientList.erase(*it);
                pthread_mutex_unlock(&m_ClientListMutex);
            }
            //clear the list
            close_list.clear();
        }
        //This waits forever        
        numSelected=select(nFds+1,&m_ReadSocks,NULL,&m_ErrorSocks,NULL);        
        //check for error
        if(numSelected == -1) {
            //if we get a bad file descriptor, just continue.
            //This will probably mean that someone closed a socket we were receiving on
            //This should not cause a problem and the socket will be removed from
            //the client list by the close_list service routine
            if(errno != EBADF) {
                int err=errno;
                PERROR1("Error Select: Errno: %d\n",err);
                perror("select");
                return false;
            }
        }


        //Note: we cannot just erase elements as we find them because it will corrupt the iterator
        //      so we make a list and delete them after we're done walking the list
        //check for new data        
        pthread_mutex_lock(&m_ClientListMutex);        
        for(connection=m_ClientList.begin();
                connection != m_ClientList.end();
                connection++) {
            
            if(FD_ISSET(connection->first,&m_ErrorSocks)) {
                close_list.push_back(connection->first);
            }
            if(FD_ISSET(connection->first,&m_ReadSocks)) {                
                data_list.push_back(connection->first);                                
            }
           
            //is the connection just marked for deletion?
            if(connection->second.bClosed == true) {
                close_list.push_back(connection->first);          
            }

        }
        pthread_mutex_unlock(&m_ClientListMutex);      
        
        //process data after walking the connection list.
        //This because the connection list mutex may be locked and
        //handle data callback may need to access the client list mutex (as in SendData)
        //which would results in a deadlock
        for(std::list<Handle_t>::iterator it=data_list.begin();
                    it!= data_list.end();
                    it++) {
            if(!HandleData(*it)) {
                //if there is an error in the handler, we'll close this connection
                close_list.push_back(*it);
            }
        }
        data_list.clear();

        //did we get a new connection (must do this after checking for data)
        if(FD_ISSET(m_ListenSocket,&m_ReadSocks)) {            
            HandleConnection(m_ListenSocket);
        }        
    }
    return true;
}


/**
 * handles an incoming connection and prepares to receive data over it
 * @param[in] socket The listen socket
 * @retval true success
 * @retval false error
 */
bool CTcpServer::HandleConnection(SOCKET socket) {
    struct sockaddr_in cin;
    socklen_t addrlen=0;
    addrlen=sizeof(cin);
    std::time_t now=time(0);
    int nFlag = 1;
    int nResults;

    UNUSED(now);

    memset(&cin,0,sizeof(cin));

    if(m_ClientList.size() >= MAX_CONNECTIONS) {
        PTRACE("No more room for connections\n");
        return false;
    }
    
    SOCKET NewSocket=accept(socket,(struct sockaddr *)&cin,&addrlen);

#ifdef DISABLE_NAGLE
    nResults = setsockopt(
                   NewSocket,       /* socket affected */
                   IPPROTO_TCP,     /* set option at TCP level */
                   TCP_NODELAY,     /* name of option */
                   (char *) &nFlag, /* the cast is historical cruft */
                   sizeof(int));    /* length of option value */
    if(nResults == -1){
        int err=errno;        
        PERROR1("Could not disable Nagle's Algorithm. Errno: %d\n",err);
        perror("TCP_NODELAY ");
    }
#endif

    if(NewSocket != -1) {
        ClientInfo_t clientInfo={0};

        clientInfo.handle=NewSocket;

        PTRACE2("Client connected from %s at %s\n",inet_ntoa(cin.sin_addr),ctime(&now));
        PTRACE1("Client Count: %u\n",(unsigned)m_ClientList.size());

        //if we can call the callback
        if(m_pConnectionCallback != NULL) {
            //if the user does not want the connection, immediately close it
            if(m_pConnectionCallback(New,cin,NewSocket,m_pConntectionUser) == false) {
                PTRACE("User rejected connection!\n");
                close(NewSocket);
                return true;
            }
        }
        //add it to the list of sockets
        pthread_mutex_lock(&m_ClientListMutex);
        m_ClientList.insert(ClientList_t::value_type(NewSocket,clientInfo));
        pthread_mutex_unlock(&m_ClientListMutex);
    } else {
        int err=errno;        
        PERROR1("Accept failed! Errno; %d\n",err);
        perror("accept:");
        return false;
    }

    return true;
}

/**
 * Handles incoming data
 * @param[in] socket socket to receive data
 * @retval true all's well
 * @retval false the other end closed the connection
 */
bool CTcpServer::HandleData(SOCKET socket) {
    unsigned char buffer[16*1024];
    size_t length;


    if(socket != -1) {        
        length=recv(socket,(char*)buffer,sizeof(buffer),0);        
        //get the command
        if(length == (unsigned)-1) {
            int err=errno;
            PERROR1("Recieve error. Errno: %d\n",err);
            perror("recv");            
            return false;
        } else {
            //new data
            if (m_pNewDataCallback != NULL && length > 0) {                
                m_pNewDataCallback(socket,buffer,(unsigned)length,m_pNewDataUser);                
            }
            //connection is being closed
            if (m_pConnectionCallback != NULL && length == 0) {
                PTRACE("Remote end closed the connection!\n");
                return false;
            }
        }
    } else {
        assert(!"Bad Socket Address");
    }    
    return true;
}

/**
 * Class destructor
 */
CTcpServer::~CTcpServer() {
    ClientList_t::iterator connection;
    PTRACE("Server closing down...\n");
    if(m_ListenSocket != -1) {
        shutdown(m_ListenSocket,SHUT_RDWR);
        close (m_ListenSocket);
        m_ListenSocket=-1;
    }

    pthread_mutex_lock(&m_ClientListMutex);
    for(connection=m_ClientList.begin();connection != m_ClientList.end();connection++) {
        shutdown(connection->first,SHUT_RDWR);
        close(connection->first);
    }
    m_ClientList.clear();
    pthread_mutex_unlock(&m_ClientListMutex);

    pthread_mutex_destroy(&m_ClientListMutex);

#   ifdef WIN32
    WSACleanup();
#   endif
}


/**
 * Sets the non blocking option on the socket.
 * @param[in] socket Socket descriptor to set to nonblocking
 * @retval true successful
 * @retval false failed
 */
bool CTcpServer::SetNoBlocking(SOCKET socket) {

#ifndef WIN32
    int opts;
    //read the current options
    opts = fcntl(socket,F_GETFL);
    if (opts < 0) {
        int err=errno;
        PERROR1("fcntl(F_GETFL) failed: Errno: %d\n",err);
        perror("fcntl(F_GETFL)");
        return false;
    }
    //or in  the no block flag
    opts = (opts | O_NONBLOCK);
    if (fcntl(socket,F_SETFL,opts) < 0) {
        int err=errno;
        PERROR1("fcntl(F_SETFL) failed: Errno: %d\n",err);
        perror("fcntl(F_SETFL)");
        return false;
    }
#else
    //for windows
    u_long iMode = 0;
    ioctlsocket(socket, FIONBIO, &iMode);
#endif
    return true;
}


/**
 * builds the select list
 * @retval 0 if failed
 * @retval The highest FD encountered 
 */
int CTcpServer::BuildSelectList() {
    ClientList_t::iterator connection;
    SOCKET highestFd=m_ListenSocket;

    FD_ZERO(&m_ReadSocks);
    FD_ZERO(&m_ErrorSocks);
    if(m_ListenSocket == -1) {
        return 0;
    }
    //add the listen socket to the select list
    FD_SET(m_ListenSocket,&m_ReadSocks);
    pthread_mutex_lock(&m_ClientListMutex);
    for(connection=m_ClientList.begin(); connection != m_ClientList.end(); connection++) {
        FD_SET(connection->first,&m_ReadSocks);
        FD_SET(connection->first,&m_ErrorSocks);
        if(connection->first > highestFd) {
            highestFd=connection->first;
        }
    }
    pthread_mutex_unlock(&m_ClientListMutex);

    return (int)highestFd;
}

/**
 * Closes all the connections.
 * Call this function after stopping the server thread
 */
void CTcpServer::CloseAllConnections(){    
    ClientList_t::iterator itClient;

    pthread_mutex_lock(&m_ClientListMutex); 
    for(itClient=m_ClientList.begin(); itClient!= m_ClientList.end(); itClient++) {
        CloseConnectionCallback(itClient->second.handle);
        shutdown(itClient->first,SHUT_RDWR);
        close(itClient->first);
        //do not actually remove the connection from the list because if
        //this is called when we are handling data by going throught the list of
        //connections, the iterator will be corrupted
        itClient->second.bClosed=true;
    }
    pthread_mutex_unlock(&m_ClientListMutex);
}
/**
 * Closes a open connection 
 * @param handle connection handle
 * @retval true Connection was closed.
 * @retval false Connection was not managed by this class
 */
bool CTcpServer::CloseConnection(Handle_t handle) {
    ClientList_t::iterator itClient;

    pthread_mutex_lock(&m_ClientListMutex);
    itClient=m_ClientList.find(handle);

    if(itClient!=m_ClientList.end()) {
        CloseConnectionCallback(handle);
        shutdown(itClient->first,SHUT_RDWR);
        close(itClient->first);
        //do not actually remove the connection from the list because if
        //this is called when we are handling data by going throught the list of
        //connections, the iterator will be corrupted
        itClient->second.bClosed=true;
    }
    pthread_mutex_unlock(&m_ClientListMutex);

    return true;
}

/**
 * Calls the registered connection callback function
 * @param handle connection handle
 */
void CTcpServer::CloseConnectionCallback(Handle_t handle) {

    if(m_pConnectionCallback != NULL) {
        struct sockaddr_in clientAddr;
        m_pConnectionCallback(Close,clientAddr,handle,m_pConntectionUser);
    }
}

/**
 * Registers a callback function for new data 
 * @param pCallback Pointer to Callback function
 * @param pUser Pointer to user provided pointer passed back into the callback function
 */
void CTcpServer::RegisterDataCallback(DataCallback_t pCallback,void *pUser) {
    m_pNewDataCallback=pCallback;
    m_pNewDataUser=pUser;
}

/**
 * Registers a callback function for connection state change
 * @param pCallback Pointer to Callback function
 * @param pUser Pointer to user provided pointer passed back into the callback function
 */
void CTcpServer::RegisterConnectionCallback(ConntectionCallback_t pCallback,void *pUser) {
    m_pConnectionCallback=pCallback;
    m_pConntectionUser=pUser;
}


/**
 * Sends replies back to the client
 * @param handle Handle of the connection 
 * @param pData pointer to data buffer
 * @param uLength Length of the data buffer
 * @retval true if the send was successful
 * @retval false if the send failed or if we are not connected 
 */
bool CTcpServer::SendToClient(Handle_t handle,unsigned char *pData,unsigned uLength) {

    //are we connected to this client
    pthread_mutex_lock(&m_ClientListMutex);
    if(m_ClientList.find(handle)== m_ClientList.end()) {
        pthread_mutex_unlock(&m_ClientListMutex);
        PERROR1("handle %d doesnot exist\n",handle);
        return false;
    }
    pthread_mutex_unlock(&m_ClientListMutex);
    //ship the data
    if(send(handle,(const char*)pData,uLength,0) == -1) {
        int err=errno;                
        PERROR2("Failed to send data to handle %d. Errno: %d\n",handle,err);
        return false;
    }
    return true;
}

/** 
 * Starts a thread to run the server
 * @retval true Success
 * @retval false failure
 */
bool CTcpServer::StartSeverThread(){

    return ( pthread_create(&m_threadId,NULL,threadHelper,this) == 0);
}



/** 
 * Starts a thread to run the server
 * @retval true Success
 * @retval false failure
 */
bool CTcpServer::StopSeverThread(){

    return (pthread_cancel(m_threadId) == 0);
}

/**
 * Helper function for running the server thread
 */
void *CTcpServer::threadHelper(void *pUser){
    CTcpServer *pServer = (CTcpServer*)pUser;

    pServer->start();

    return NULL;
}

#ifndef _TCP_SERVER_H_
#define _TCP_SERVER_H_

#ifdef WIN32
#   include <winsock2.h>
#else
#   include <sys/socket.h>
#   define SOCKET int
#endif

#include <pthread.h>
#include <map>

extern "C" void * ThreadHelper(void *);

/** class encapsulating the server behavior */
class CTcpServer {
public:
    /** connection state */
    typedef enum {New,Close} ConnectionState_t;
    /** Bad connection handles */
    enum  {INVALID_HANDLE = -1};
    /** connection handle type */
    typedef SOCKET Handle_t;
    /** connection info structure */
    typedef struct {
        Handle_t handle;
        bool     bClosed;
    }
    ClientInfo_t;
    /**
     *  state : indicates whether this connection is being establised or closed
     *  clientAddr: address of the client. This variable is only valid for new connections
     *  handle: handle for this connection
     *  pUser: pointer passed in during registration 
     **/
    typedef bool (*ConntectionCallback_t)(ConnectionState_t state,const struct sockaddr_in &clientAddr,Handle_t handle,void *pUser);
    /**
     *   uHandle: handle for this connection
     *   pData: pointer to data buffer 
     *   uLength : Length of the data buffer
     *   pUser: pointer passed in during registration 
     **/
    typedef bool (*DataCallback_t)(Handle_t handle,unsigned char *pData, unsigned uLength,void *pUser);
    /** @brief starts the server */
    bool start();
    /** @brief Class constructor */
    CTcpServer(unsigned uPort);
    /** @brief class destructor */
    ~CTcpServer();
    /** @brief register a callback function for connection state change */
    void RegisterConnectionCallback(ConntectionCallback_t pCallback,void *pUser);
    /** @brief registers a callback function for data reception */
    void RegisterDataCallback(DataCallback_t pCallback,void *pUser);
    /** @brief Closes an open connection */
    bool CloseConnection(Handle_t handle);
    void CloseAllConnections();
    /** @brief Sends dara back to client */
    bool SendToClient(Handle_t handle,unsigned char *pData,unsigned uLength);
    /** @brief this function starts a thread and calls the start function */
    bool StartSeverThread();
    /** @brief this function stops the server thread */
    bool StopSeverThread();
    

protected:
    /** maximum number of connections we can handle at the same time */
    enum  {MAX_CONNECTIONS= 500};  

    typedef std::map<Handle_t,ClientInfo_t> ClientList_t;
    /** port we are listening over */
    unsigned m_uPort;
    /** mutex for accessing the connection list */
    pthread_mutex_t m_ClientListMutex;
    /** socket list */
    ClientList_t m_ClientList;
    /** listen socket */
    SOCKET m_ListenSocket;
    /** descriptor list used for select */
    fd_set m_ReadSocks;
    fd_set m_ErrorSocks;
    /** new connection callback function */
    ConntectionCallback_t m_pConnectionCallback;
    /** new data callback function */
    DataCallback_t m_pNewDataCallback;
    /** place to store users pointer to new data callbacks */
    void * m_pNewDataUser;
    /** place to store users pointer to connection callbacks */
    void * m_pConntectionUser;
    /** thread id of the server thread */
    pthread_t m_threadId;

    /** @brief Disable socket blocking*/
    bool SetNoBlocking(SOCKET socket);
    /** @brief process incoming connection */
    bool HandleConnection(SOCKET socket);
    /** @brief process incoming data*/
    bool HandleData(SOCKET socket);
    /** @brief builds the select list */
    int BuildSelectList(void);
    /** @brief Calls the users close connection callback */
    void CloseConnectionCallback(Handle_t handle);
    
    
    friend void * ThreadHelper(void *);;
};

#endif /*_TCP_SERVER_H_*/

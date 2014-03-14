#ifndef _UDP_SERVER_H
#define _UDP_SERVER_H
#ifdef WIN32
#   include <winsock2.h>
#else
#   include <sys/socket.h>
#   define SOCKET int
#endif

#include <pthread.h>
#include <map>

extern "C" void * udp_server_thread_helper(void *);

class CUdpServer
{
public:
	/** @brief Class constructor */
	CUdpServer(unsigned uPort);
    /** @brief constructor used for bi directional connected UDP sockets */
    CUdpServer(unsigned uSendPort,std::string serverAddr,unsigned uReceivePort);
    /** @brief Sends data to bi-directional client */
    bool SendToClient(unsigned char *pData,unsigned length);
    
	/** @brief Class destructor */
	~CUdpServer(void);

	 /**
     *   pData: pointer to data buffer 
     *   uLength : Length of the data buffer
     *   pUser: pointer passed in during registration 
     **/
    typedef void (*DataCallback_t)(unsigned char *pData, unsigned uLength,void *pUser);
	/** @brief registers a callback function for data reception */
    void RegisterDataCallback(DataCallback_t pCallback,void *pUser);	
	/** @brief this function starts a thread and calls the start function */
    bool StartServerThread();
    /** @brief this function stops the server thread */
    bool StopServerThread();
protected:
	/** @brief starts the server (will not return)*/
    bool start();
    /** @brief initializes the server socket */
    bool initServer();
	
	/** place to store users pointer to new data callbacks */
    void * m_pNewDataUser;
	/** thread id of the server thread */
    pthread_t m_threadId;
	/** receive socket */
	SOCKET m_Socket;
	/** port to listen on */
	unsigned m_uPort;
	/** new data callback function */
    DataCallback_t m_pNewDataCallback;
    /** when set to true, we are a bi-directional connected UDP */
    bool m_bBiDirectional;

	friend void * udp_server_thread_helper(void *);
};

#endif //_UDP_SERVER_H
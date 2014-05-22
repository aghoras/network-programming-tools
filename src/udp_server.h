#ifndef _UDP_SERVER_H
#define _UDP_SERVER_H
#ifdef WIN32
#   include <winsock2.h>
#else
#   include <sys/socket.h>
#   define SOCKET int
#endif

#include <string>
#include <pthread.h>
#include <map>
#include <boost/function.hpp>

extern "C" void * udp_server_thread_helper(void *);

/**
 * Notes on creating a callback:
 * 1)Create a class member (could be of any access type)
 *  eg. protected: incomingDataCallback(unsigned char *pData, unsigned uLength);
 * 2)Call the UdpServer::RegisterCallback
 *  e.g  m_UdpServer.RegisterDataCallback(boost::bind(&CIpChannel::incomingDataCallback, this,_1,_2));
 * 3)It is possible to bind additional variables to the callback function during registration via
 *   a the boost::bind function. These variables will be passed back into the callback function
 */
class UdpServer
{
public:
	/** @brief Class constructor */
	UdpServer(unsigned uPort);
    /** @brief constructor used for bi directional connected UDP sockets */
    UdpServer(unsigned uSendPort,std::string serverAddr,unsigned uReceivePort);
    /** @brief Sends data to bi-directional client */
    bool SendToClient(const unsigned char *pData,unsigned length) const;
    
	/** @brief Class destructor */
	~UdpServer(void);

	 /**
     *   pData: pointer to data buffer 
     *   uLength : Length of the data buffer
     *   pUser: pointer passed in during registration 
     **/
	typedef boost::function<void(unsigned char*,unsigned)> DataCallback_t;
	/** @brief registers a callback function for data reception */
    void RegisterDataCallback(DataCallback_t callback);
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

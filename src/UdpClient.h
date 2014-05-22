/** 
 * @brief Ecapsulates the behavior of a UDP client
 *
 * @date   May 22, 2014
 * @author ali
 * 
 * 
 */
#ifndef UDPCLIENT_H_
#define UDPCLIENT_H_

# include <netinet/in.h>
#include <string>
/**
 * This class implements a UDP client
 */
class UdpClient {
public:
    UdpClient(unsigned port,std::string address);
    virtual ~UdpClient();
    bool Send(unsigned char *data,size_t size);
protected:
    int m_SendSocket;
    struct  sockaddr_in m_ServerAddress;
};

#endif /* UDPCLIENT_H_ */

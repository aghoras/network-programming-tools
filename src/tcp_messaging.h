/**
 * @file tcp_messaging.h
 *
 * @date   Mar 14, 2014
 * @author ali
 */

#ifndef TCPMESSAGING_H
#define TCPMESSAGING_H

#include "Messaging.h"
#include <string.h>

class CTcpMessaging: public CMessaging {
protected:
    std::string m_sIpAddress;
    unsigned    m_uPortNumber;
    int         m_socket;

    /** @brief low level messaging */
    virtual int xmitMsg(const unsigned char *pBuffer, unsigned uLength);
public:
    CTcpMessaging();
    ~CTcpMessaging();
    /** @brief connects to server on the client side*/
    bool connect(std::string sIpAddress,unsigned uPort);
    /** @brief disconnects from sever on the client side */
    void disconnect();

    const std::string& getSIpAddress() const {  return m_sIpAddress;   }
    unsigned getUPortNumber()          const {  return m_uPortNumber;  }
    bool     isConnected()             const {  return (m_socket>0);   }
};

#endif /* TCPMESSAGING_H */


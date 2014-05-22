/** 
  *
 * @date   May 22, 2014
 * @author ali
 * 
 * 
 */
#include "UdpClient.h"
#include <string.h>
#include <arpa/inet.h>
#include <boost/log/trivial.hpp>
#include <errno.h>

/**
 * Class constructor
 * @param port Port to send to
 * @param addresss IP address of the server
 */
UdpClient::UdpClient(unsigned port, std::string address) {
    m_SendSocket = socket(AF_INET, SOCK_DGRAM, 0);

    memset(&m_ServerAddress, 0, sizeof(m_ServerAddress));
    m_ServerAddress.sin_family = AF_INET;
    m_ServerAddress.sin_addr.s_addr = inet_addr(address.c_str());
    m_ServerAddress.sin_port = htons(port);
}

/**
 * Class destructor
 */
UdpClient::~UdpClient() {
    shutdown(m_SendSocket,SHUT_WR);
    close(m_SendSocket);
}

/**
 * Sends data to to the remote destination
 * @param data Buffer to transmit
 * @param size Size of the buffer
 * @retval true Success
 * @retval false Failure
 */
bool UdpClient::Send(unsigned char* data, size_t size) {
    if (sendto(m_SendSocket, (char*)data, size, 0, (const struct sockaddr *) &m_ServerAddress, sizeof(m_ServerAddress)) < 0) {
        BOOST_LOG_TRIVIAL(error) << "UdpClient SendToClient failed: " << strerror(errno);
        return false;
    }

    return true;
}

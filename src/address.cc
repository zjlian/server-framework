#include "address.h"
#include <algorithm>
#include <arpa/inet.h>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <netinet/in.h>
#include <sstream>
#include <sys/socket.h>
#include <unistd.h>

namespace zjl
{

int Address::getFamily() const
{
    return getAddr()->sa_family;
}

std::string Address::toString()
{
    thread_local static std::stringstream ss;
    insert(ss);
    return ss.str();
}


// 对比两个地址的大小。先逐字节对比地址的数据大小，如果相同，则对比地址的字节长度
bool Address::operator<(const Address& rhs) const
{
    socklen_t min_len = std::min(getAddrLen(), rhs.getAddrLen());
    int result = memcmp(getAddr(), rhs.getAddr(), min_len);
    if (result != 0)
    {
        return result < 0;
    }
    else {
        return getAddrLen() < rhs.getAddrLen();
    }
}

bool Address::operator==(const Address& rhs) const
{
    return (getAddrLen() == rhs.getAddrLen()) &&
           (memcmp(getAddr(), rhs.getAddr(), getAddrLen()) == 0);

}

bool Address::operator!=(const Address& rhs) const
{
    return !(*this == rhs);
}


IPv4Address::IPv4Address(uint32_t address, uint16_t port)
{
    memset(&m_addr, 0, sizeof(m_addr));
    m_addr.sin_family = AF_INET;
    m_addr.sin_port = htons(port);
    m_addr.sin_addr.s_addr = htonl(address);
}

const sockaddr* IPv4Address::getAddr() const
{
    return (sockaddr*)(&m_addr);
}

socklen_t IPv4Address::getAddrLen() const
{
    return sizeof(m_addr);
}

IPAddress::ptr IPv4Address::broadcastAddress(uint32_t prefix_len)
{
    assert(!(prefix_len > 32));
    sockaddr_in baddr(m_addr);
    // baddr.sin_addr.s_addr
}

IPAddress::ptr IPv4Address::networkAddress(uint32_t prefix_len)
{

}

IPAddress::ptr IPv4Address::subnetMask(uint32_t prefix_len)
{

}

uint32_t IPv4Address::getPort() const
{
    return ntohs(m_addr.sin_port);
}

void IPv4Address::setPort(uint16_t port)
{
    m_addr.sin_port = htons(port);
}

std::ostream& IPv4Address::insert(std::ostream& os) const
{
    auto addr = ntohl(m_addr.sin_addr.s_addr);
    os << ((addr >> 24) & 0xff) << "."
       << ((addr >> 16) & 0xff) << "."
       << ((addr >>  8) & 0xff) << "."
       << ((addr >>  0) & 0xff) << ":" << ntohs(m_addr.sin_port);
    return os;
}

IPv6Address::IPv6Address()
{
    memset(&m_addr, 0, sizeof(m_addr));
    m_addr.sin6_family = AF_INET6;
}

IPv6Address::IPv6Address(const char *address, uint16_t port)
{
    memset(&m_addr, 0, sizeof(m_addr));
    m_addr.sin6_family = AF_INET6;
    m_addr.sin6_port = htons(port);
    memcpy(&m_addr.sin6_addr.s6_addr, address, 16);
}

const sockaddr* IPv6Address::getAddr() const
{
    return (sockaddr*)(&m_addr);
}

socklen_t IPv6Address::getAddrLen() const
{
    return sizeof(m_addr);
}

IPAddress::ptr IPv6Address::broadcastAddress(uint32_t prefix_len)
{

}

IPAddress::ptr IPv6Address::networkAddress(uint32_t prefix_len)
{

}

IPAddress::ptr IPv6Address::subnetMask(uint32_t prefix_len)
{

}

uint32_t IPv6Address::getPort() const
{

}

void IPv6Address::setPort(uint16_t port)
{

}

std::ostream& IPv6Address::insert(std::ostream& os) const
{
    os << "[";
    uint16_t *addr = (uint16_t*)(&m_addr.sin6_addr.s6_addr);
    bool used_zeros = false;

    return os;
}


UnknowAddress::UnknowAddress()
{

}

const sockaddr* UnknowAddress::getAddr() const
{

}

socklen_t UnknowAddress::getAddrLen() const
{

}

std::ostream& UnknowAddress::insert(std::ostream& os) const
{

}

} // namespace zjl

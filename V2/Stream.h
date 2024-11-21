#ifndef STREAM_INTERFACE_H
#define STREAM_INTERFACE_H

#include <ThorsSocket/SocketStream.h>

#include <sstream>
#include <filesystem>

class Message
{
    std::stringstream   ss;
    public:
        template<typename T>
        Message& operator<<(T const& data) {ss << data;return *this;}

        operator std::string() {return ss.str();}
};

void handleConnection(ThorsAnvil::ThorsSocket::SocketStream& socket, std::filesystem::path const& contentDir);

#endif


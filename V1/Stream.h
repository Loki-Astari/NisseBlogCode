#ifndef STREAM_INTERFACE_H
#define STREAM_INTERFACE_H

#include <string>
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

class Stream
{
    public:
        virtual ~Stream()   {}

        virtual std::string_view    getNextLine()               = 0;
        virtual void                ignore(std::size_t size)    = 0;

        virtual void sendMessage(std::string const& message)    = 0;
        virtual void sync()                                     = 0;

        virtual bool hasData()  const                           = 0;
        virtual void close()                                    = 0;
};

void handleConnection(Stream& socket, std::filesystem::path const& contentDir);

#endif

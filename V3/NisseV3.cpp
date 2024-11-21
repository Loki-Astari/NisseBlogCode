#include "../V1/Stream.h"
#include "../V2/ServerInit.h"

#include <ThorsSocket/Server.h>
#include <ThorsSocket/SocketStream.h>
#include <ThorsSocket/SocketUtil.h>
#include <ThorsLogging/ThorsLogging.h>

#include <iostream>
#include <exception>

namespace TASock    = ThorsAnvil::ThorsSocket;

/*
 * Class Declarations:
 *
 *      Socket:             An implementation of Stream Interface using TASock::SocketStream
 *      WebServer:          A class to represent and manage incoming connections.
 *
 */

class Socket: public Stream
{
    TASock::SocketStream    stream;
    public:
        Socket(TASock::SocketStream&& stream)
            : stream(std::move(stream))
        {}

        virtual std::string_view    getNextLine()               override
        {
            static std::string line;
            std::getline(stream, line);
            return line;
        }
        virtual void ignore(std::size_t size)                   override {stream.ignore(size);}
        virtual void sendMessage(std::string const& message)    override {stream << message;}
        virtual void sync()                                     override {stream.sync();}
        virtual bool hasData()  const                           override {return static_cast<bool>(stream);}
        virtual void close()                                    override {stream.close();}
};

class WebServer
{
    TASock::Server                      connection;
    bool                                finished;
    std::filesystem::path const&        contentDir;
    public:
        WebServer(TASock::ServerInit&& serverInit, std::filesystem::path const& contentDir);

        void run();
};

int main(int argc, char* argv[])
{
    if (argc != 4 && argc != 3)
    {
        std::cerr << "Usage: NisseV1 <port> <documentPath> [<SSL Certificate Path>]" << "\n";
        return 1;
    }

    try
    {
        static const int port = std::stoi(argv[1]);
        static const std::filesystem::path      contentDir  = std::filesystem::canonical(argv[2]);
        std::optional<std::filesystem::path>    certDir;
        if (argc == 4) {
            certDir = std::filesystem::canonical(argv[3]);
        }

        std::cout << "Nisse Proto 3\n";
        WebServer   server(getServerInit(port, certDir), contentDir);
        server.run();
    }
    catch(std::exception const& e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
        throw;
    }
    catch(...)
    {
        std::cerr << "Exception: UNKNOWN\n";
        throw;
    }
}

/*
 * Class Implementation:
 */

// WebServer
// =========
WebServer::WebServer(TASock::ServerInit&& serverInit, std::filesystem::path const& contentDir)
    : connection{std::move(serverInit)}
    , finished{false}
    , contentDir{contentDir}
{}

void WebServer::run()
{
    while (!finished)
    {
        TASock::SocketStream socketStream = connection.accept();
        Socket  socket(std::move(socketStream));

        handleConnection(socket, contentDir);
    }
}


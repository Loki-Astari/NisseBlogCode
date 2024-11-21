#include "Stream.h"

#include <ThorsSocket/Server.h>
#include <ThorsSocket/SocketStream.h>
#include <ThorsSocket/SocketUtil.h>
#include <ThorsLogging/ThorsLogging.h>

#include <iostream>
#include <string>
#include <exception>
#include <filesystem>

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <arpa/inet.h>
#include <unistd.h>

/*
 * Class Declarations:
 *
 *      WebServer:          A class to represent and manage incoming connections.
 *
 * Notes:
 *      Socket/Server class has been replaced by ThorsAnvil::ThorsSocket::SocketStream / ThorsAnvil::ThorsSocket::Server
 */

class WebServer
{
    ThorsAnvil::ThorsSocket::Server connection;
    bool                            finished;
    std::filesystem::path const&    contentDir;
    public:
        WebServer(ThorsAnvil::ThorsSocket::ServerInit&& serverInit, std::filesystem::path const& contentDir);

        void run();
};

ThorsAnvil::ThorsSocket::ServerInit getServerInit(int port, std::optional<std::filesystem::path> certPath)
{
    // If there is only a port.
    // i.e. The user did not provide a certificate path return a `ServerInfo` object.
    // This will create a normal listening socket.
    if (!certPath.has_value()) {
        return ThorsAnvil::ThorsSocket::ServerInfo{port};
    }

    // If we have a certificate path.
    // Use this to create a certificate objext.
    // This assumes the standard names for these files as provided by "Let's encrypt".
    ThorsAnvil::ThorsSocket::CertificateInfo     certificate{std::filesystem::canonical(std::filesystem::path(*certPath) /= "fullchain.pem"),
                                                             std::filesystem::canonical(std::filesystem::path(*certPath) /= "privkey.pem")
                                                            };
    ThorsAnvil::ThorsSocket::SSLctx              ctx{ThorsAnvil::ThorsSocket::SSLMethodType::Server, certificate};

    // Now that we have created the approporiate SSL objects needed.
    // We return an SServierInfo object.
    // Please Note: This is a different type to the ServerInfo returned above (one less S in the name).
    return ThorsAnvil::ThorsSocket::SServerInfo{port, std::move(ctx)};

    // We can return these two two different types becuase
    // ServerInit is actually a std::variant<ServerInfo, SServerInfo>
}

// The application body.


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

        std::cout << "Nisse Proto 2\n";
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
WebServer::WebServer(ThorsAnvil::ThorsSocket::ServerInit&& serverInit, std::filesystem::path const& contentDir)
    : connection{std::move(serverInit)}
    , finished{false}
    , contentDir{contentDir}
{}

void WebServer::run()
{
    while (!finished)
    {
        ThorsAnvil::ThorsSocket::SocketStream socket = connection.accept();

        handleConnection(socket, contentDir);
    }
}


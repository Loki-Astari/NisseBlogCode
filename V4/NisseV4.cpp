#include "../V2/Stream.h"

#include "JobQueue.h"

#include <ThorsSocket/Server.h>
#include <ThorsSocket/SocketStream.h>
#include <ThorsSocket/SocketUtil.h>
#include <ThorsLogging/ThorsLogging.h>

#include <iostream>
#include <exception>
#include <mutex>
#include <map>

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <arpa/inet.h>
#include <unistd.h>

namespace NisServer = ThorsAnvil::Nisse::Server;
namespace TASock    = ThorsAnvil::ThorsSocket;

/*
 * Class Declarations:
 *
 *      WebServer:          A class to represent and manage incoming connections.
 */

class WebServer
{

    TASock::Server                      connection;
    bool                                finished;
    std::filesystem::path const&        contentDir;
    // State information that can be used by the threads.
    // Objects placed in a std::map are not moved once inserted so taking
    // a reference to them is safe and can be used by another thread.
    std::mutex openSocketMutex;
    std::map<int, TASock::SocketStream>         openSockets;
    // A JobQueue that holds a pool of threads to execute inserted jobs asynchronously.
    NisServer::JobQueue jobQueue;
    public:
        WebServer(std::size_t workerCount, TASock::ServerInit&& serverInit, std::filesystem::path const& contentDir);

        void run();
};

TASock::ServerInit getServerInit(int port, std::optional<std::filesystem::path> certPath)
{
    // If there is only a port.
    // i.e. The user did not provide a certificate path return a `ServerInfo` object.
    // This will create a normal listening socket.
    if (!certPath.has_value()) {
        return TASock::ServerInfo{port};
    }

    // If we have a certificate path.
    // Use this to create a certificate object.
    // This assumes the standard names for these files as provided by "Let's encrypt".
    TASock::CertificateInfo     certificate{std::filesystem::canonical(std::filesystem::path(*certPath) /= "fullchain.pem"),
                                            std::filesystem::canonical(std::filesystem::path(*certPath) /= "privkey.pem")
                                           };
    TASock::SSLctx              ctx{TASock::SSLMethodType::Server, certificate};

    // Now that we have created the appropriate SSL objects needed.
    // We return an SServierInfo object.
    // Please Note: This is a different type to the ServerInfo returned above (one less S in the name).
    return TASock::SServerInfo{port, std::move(ctx)};

    // We can return these two two different types because
    // ServerInit is actually a std::variant<ServerInfo, SServerInfo>
}

// The application body.


int main(int argc, char* argv[])
{
    static constexpr std::size_t workerCount = 4;

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

        std::cout << "Nisse Proto 4\n";
        WebServer   server(workerCount, getServerInit(port, certDir), contentDir);
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
WebServer::WebServer(std::size_t workerCount, TASock::ServerInit&& serverInit, std::filesystem::path const& contentDir)
    : connection{std::move(serverInit)}
    , finished{false}
    , contentDir{contentDir}
    , jobQueue{workerCount}
{}

void WebServer::run()
{
    while (!finished)
    {
        // Main thread waits for a new connection.
        TASock::SocketStream newSocket = connection.accept();

        // Add the “newSocket” into the std::map object “openSockets”
        int fd = newSocket.getSocket().socketId();
        std::unique_lock<std::mutex>    lock(openSocketMutex);
        auto [iter, ok] = openSockets.insert_or_assign(fd, std::move(newSocket));

        // Add a lambda to the JobQueue to handle the newly created socket.
        // Note: A copy of the “iter” is placed in the object “iterator” so we can use
        //       this to extract a reference to the socket object. This is thread safe
        //       as iterators to std::map are not invalidated by operations on the map
        //       (as long as the object is not deleted).
        jobQueue.addJob([&, iterator = iter](){
            // Get a reference to the socket.
            auto& socket = iterator->second;
            // Handle the reference as before.
            handleConnection(socket, contentDir);
            // Once processing is complete remove the storage for Socket
            // and cleanup any associated storage.
            std::unique_lock<std::mutex>    lock(openSocketMutex);
            openSockets.erase(iterator);
        });
    }
}


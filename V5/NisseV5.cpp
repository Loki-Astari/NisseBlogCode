#include "../V1/Stream.h"
#include "../V2/ServerInit.h"
#include "../V4/JobQueue.h"
#include "EventHandler.h"

#include <ThorsSocket/Server.h>
#include <ThorsSocket/SocketStream.h>
#include <ThorsSocket/SocketUtil.h>
#include <ThorsLogging/ThorsLogging.h>

#include <iostream>
#include <exception>
#include <mutex>
#include <map>

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
    // State information that can be used by the threads.
    // Objects placed in a std::map are not moved once inserted so taking
    // a reference to them is safe and can be used by another thread.
    std::mutex                          openSocketMutex;
    std::map<int, Socket>               openSockets;
    // A JobQueue that holds a pool of threads to execute inserted jobs asynchronously.
    JobQueue                            jobQueue;
    EventHandler                        eventHandler;
    public:
        WebServer(std::size_t workerCount, TASock::ServerInit&& serverInit, std::filesystem::path const& contentDir);

        void run();
    private:
        void newConnectionHandler(int fd);
        void normalConnectionHandler(int fd);
};

int main(int argc, char* argv[])
{
    loguru::g_stderr_verbosity = 9;
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

        std::cout << "Nisse Proto 5\n";
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
    , eventHandler{jobQueue}
{}

void WebServer::run()
{
    std::cerr << "Listen to: " << connection.socketId() << "\n";
    eventHandler.add(connection.socketId(), [&](int fd){this->newConnectionHandler(fd);});
    eventHandler.run();
}

void WebServer::newConnectionHandler(int)
{
    std::cerr << "newConnectionHandler\n";
    // Main thread waits for a new connection.
    TASock::SocketStream socketStream = connection.accept();
    int fd = socketStream.getSocket().socketId();
    Socket newSocket(std::move(socketStream));

    // Add the “newSocket” into the std::map object “openSockets”
    std::unique_lock<std::mutex>    lock(openSocketMutex);
    auto [iter, ok] = openSockets.insert_or_assign(fd, std::move(newSocket));

    eventHandler.add(fd, [&](int fd){this->normalConnectionHandler(fd);});
}

void WebServer::normalConnectionHandler(int fd)
{
    std::cerr << "normalConnectionHandler\n";
    jobQueue.addJob([&](){
        std::cerr << "Job Running\n";
        // Get a reference to the socket.
        auto find = openSockets.find(fd);
        auto& socket = find->second;
        // Handle the reference as before.
        handleConnection(socket, contentDir);
        // Once processing is complete remove the storage for Socket
        // and cleanup any associated storage.
        std::unique_lock<std::mutex>    lock(openSocketMutex);
        openSockets.erase(fd);
    });
}


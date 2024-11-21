#include "../V2/Stream.h"

#include "../V4/JobQueue.h"
#include "../V5/EventHandler.h"

#include <ThorsSocket/Server.h>
#include <ThorsSocket/SocketStream.h>
#include <ThorsSocket/SocketUtil.h>
#include <ThorsLogging/ThorsLogging.h>

#include <boost/coroutine2/all.hpp>

#include <iostream>
#include <map>
#include <exception>
#include <mutex>

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
 *
 * Notes:
 *      Socket/Server class has been replaced by TASock::SocketStream / TASock::Server
 */

enum class TaskYieldState        {RestoreRead, RestoreWrite, Remove};
struct TaskYieldAction
{
    TaskYieldState      state;
    int                 fd;
};
using CoRoutine     = boost::coroutines2::coroutine<TaskYieldAction>::pull_type;
using Yield         = boost::coroutines2::coroutine<TaskYieldAction>::push_type;
struct SocketInfo
{
    TASock::SocketStream    socket;
    CoRoutine               work;
};

class WebServer
{
    TASock::Server                      connection;
    bool                                finished;
    std::filesystem::path const&        contentDir;
    // State information that can be used by the threads.
    // Objects placed in a std::map are not moved once inserted so taking
    // a reference to them is safe and can be used by another thread.
    std::mutex openSocketMutex;
    std::map<int, SocketInfo>           openSockets;
    // A JobQueue that holds a pool of threads to execute inserted jobs asynchronously.
    NisServer::JobQueue                 jobQueue;
    NisServer::EventHandler             eventHandler;
    public:
        WebServer(std::size_t workerCount, TASock::ServerInit&& serverInit, std::filesystem::path const& contentDir);

        void run();
    private:
        void newConnectionHandler(int fd);
        void normalConnectionHandler(int fd);
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

        std::cout << "Nisse Proto 6\n";
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
    TASock::SocketStream newSocket = connection.accept();

    // Add the “newSocket” into the std::map object “openSockets”
    int fd = newSocket.getSocket().socketId();
    std::unique_lock<std::mutex>    lock(openSocketMutex);

    static CoRoutine    invalid{[](Yield&){}};

    auto [iter, ok] = openSockets.insert_or_assign(fd, SocketInfo{std::move(newSocket), std::move(invalid)});
    iter->second.work = CoRoutine{[fd, &contentDir = this->contentDir, &webServer = *this, &socket = iter->second.socket](Yield& yield)
    {
        std::cerr << "Job Running\n";
        socket.getSocket().setReadYield([&yield, fd](){yield(TaskYieldAction{TaskYieldState::RestoreRead, fd});return true;});
        socket.getSocket().setWriteYield([&yield, fd](){yield(TaskYieldAction{TaskYieldState::RestoreWrite, fd});return true;});
        handleConnection(socket, contentDir);
        yield(TaskYieldAction{TaskYieldState::Remove, fd});
    }};

    eventHandler.add(fd, [&](int fd){this->normalConnectionHandler(fd);});
}

void WebServer::normalConnectionHandler(int fd)
{
    std::cerr << "normalConnectionHandler\n";
    auto find = openSockets.find(fd);
    jobQueue.addJob([&webServer = *this, fd, &work = find->second.work](){
        TaskYieldAction action = work.get();
        switch (action.state)
        {
            case TaskYieldState::RestoreRead:
                webServer.eventHandler.restore(fd, true);
                break;
            case TaskYieldState::RestoreWrite:
                webServer.eventHandler.restore(fd, false);
                break;
            case TaskYieldState::Remove:
                //webServer.openSockets.erase(fd);
                break;
        }
    });
}


#include <iostream>
#include <sstream>
#include <exception>
#include <stdexcept>

#include <sys/socket.h>
#include <arpa/inet.h>

/*
 * Class Declarations:
 *
 *      HttpRequest:        An HTTP request object that has been read from a 'Socket'.
 *      HttpResponse:       An HTTP response object that can be written to a 'Socket' in
 *                          response to an HttpRequest.
 *      Socket:             A Unix socket connection that has been established.
 *                          It acts like a bi-directional communication channel.
 *                          It has an internal buffer to track requests.
 *      Server:             A Unix socket listening for incoming connections.
 *      WebServer:          A class to represent and manage incoming connections.
 */

class ErrorMessage
{
    std::stringstream   ss;
    public:
        template<typename T>
        ErrorMessage& operator<<(T const& data) {ss << data;return *this;}

        operator std::string() {return ss.str();}
};

class Socket;
class HttpRequest
{
    public:
        HttpRequest(Socket& socket);

        bool valid() const;
};

class HttpResponse
{
    public:
        HttpResponse(HttpRequest const& request);

        void send(Socket& socket);
};

class Socket
{
    int fd;
    public:
        Socket(int fd);
};

class Server
{
    static constexpr int backlog = 5;
    int fd;
    public:
        Server(int port);

        Socket accept();
};

class WebServer
{
    Server      connection;
    bool        finished;
    public:
        WebServer(int port);

        void run();
    private:
        void handleConnection(Socket& socket);
};

// The application body.
int main()
{
    static constexpr int port = 8080;
    try
    {
        std::cout << "Nisse Proto 1\n";
        WebServer   server(port);
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
WebServer::WebServer(int port)
    : connection{port}
    , finished{false}
{}

void WebServer::run()
{
    while (!finished)
    {
        Socket socket = connection.accept();

        handleConnection(socket);
    }
}

void WebServer::handleConnection(Socket& socket)
{
    HttpRequest request(socket);

    if (request.valid())
    {
        HttpResponse    response(request);
        response.send(socket);
    }
}

// Server
// ======
Server::Server(int port)
{
    fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        throw std::runtime_error{ErrorMessage{} << "Failed to create socket: " << errno << " " << strerror(errno)};
    }

    struct ::sockaddr_in        serverAddr{};
    serverAddr.sin_family       = AF_INET;
    serverAddr.sin_port         = htons(port);
    serverAddr.sin_addr.s_addr  = INADDR_ANY;

    int bindStatus = ::bind(fd, reinterpret_cast<struct ::sockaddr*>(&serverAddr), sizeof(serverAddr));
    if (bindStatus == -1) {
        throw std::runtime_error{ErrorMessage{} << "Failed to bind socket: " << errno << " " << strerror(errno)};
    }

    int listenStatus = ::listen(fd, backlog);
    if (listenStatus == -1) {
        throw std::runtime_error{ErrorMessage{} << "Failed to listen socket: " << errno << " " << strerror(errno)};
    }
}

Socket Server::accept()
{
    ::sockaddr_storage  serverStorage;
    ::socklen_t         addrSize   = sizeof(serverStorage);

    while (true)
    {
        int accept = ::accept(fd, reinterpret_cast<struct ::sockaddr*>(&serverStorage), &addrSize);
        if (accept == -1 && errno == EINTR) {
            continue;
        }
        if (accept == -1) {
            throw std::runtime_error{ErrorMessage{} << "Failed to accept socket: " << errno << " " << strerror(errno)};
        }
        return Socket{accept};
    }
}

// Socket
// ======
Socket::Socket(int fd)
    : fd(fd)
{}

// HttpRequest
// ===========
HttpRequest::HttpRequest(Socket& socket)
{}


bool HttpRequest::valid() const
{
    return true;
}

// HttpResponse
// ============
HttpResponse::HttpResponse(HttpRequest const& request)
{}

void HttpResponse::send(Socket& socket)
{}


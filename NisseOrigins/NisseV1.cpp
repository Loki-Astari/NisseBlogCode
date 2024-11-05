#include <iostream>
#include <exception>

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
    public:
};

class Server
{
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
{}

Socket Server::accept()
{
    return Socket{};
}

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


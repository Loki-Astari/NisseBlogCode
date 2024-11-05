#include <iostream>
#include <exception>

class Socket
{
    public:
};

class ServerConnection
{
    public:
        ServerConnection(int port);

        Socket accept();
};

class WebServer
{
    ServerConnection    connection;
    bool                finished;
    public:
        WebServer(int port);

        void run();
    private:
        void handleConnection(Socket& socket);
};

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


WebServer::WebServer(int port)
    : connection(port)
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
{}

ServerConnection::ServerConnection(int port)
{}

Socket ServerConnection::accept()
{
    return Socket{};
}


#include <iostream>
#include <exception>

class WebServer
{
    public:
        WebServer(int port);

        void run();
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
{}

void WebServer::run()
{}

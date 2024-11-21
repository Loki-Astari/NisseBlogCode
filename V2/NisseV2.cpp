#include <ThorsSocket/Server.h>
#include <ThorsSocket/SocketStream.h>
#include <ThorsSocket/SocketUtil.h>
#include <ThorsLogging/ThorsLogging.h>

#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>
#include <tuple>
#include <exception>
#include <stdexcept>
#include <filesystem>

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <arpa/inet.h>
#include <unistd.h>

/*
 * Class Declarations:
 *
 *      Message:            Object to build a string dynamically.
 *      ErrorStatus:        Error state that is reported back on each request.
 *      HttpRequest:        An HTTP request object that has been read from a 'Socket'.
 *      HttpResponse:       An HTTP response object that can be written to a 'Socket' in
 *                          response to an HttpRequest.
 *      WebServer:          A class to represent and manage incoming connections.
 *
 * Notes:
 *      Socket/Server class has been replaced by ThorsAnvil::ThorsSocket::SocketStream / ThorsAnvil::ThorsSocket::Server
 */

class Message
{
    std::stringstream   ss;
    public:
        template<typename T>
        Message& operator<<(T const& data) {ss << data;return *this;}

        operator std::string() {return ss.str();}
};

struct ErrorStatus
{
    ErrorStatus()
        : errorCode{200}
        , errorMessage{"OK"}
    {}

    int             errorCode;
    std::string     errorMessage;
    std::string     humanInformation;
};

class HttpRequest
{
    ErrorStatus     status;

    std::string     method;
    std::string     URI;
    std::string     version;

    public:
        HttpRequest(ThorsAnvil::ThorsSocket::SocketStream& socket);
        ErrorStatus const&  getStatus()         const   {return status;}
        std::string const&  getURI()            const   {return URI;}
        bool isValid() const {return status.errorCode == 200;}

    private:
        std::tuple<std::string, std::string>                splitHeader(std::string_view header);
        std::tuple<std::string, std::string, std::string>   splitFirstLine(std::string_view firstLine);
};

class HttpResponse
{
    HttpRequest const&  request;
    ErrorStatus         status;

    public:
        HttpResponse(HttpRequest const& request);

        bool isValid() const {return status.errorCode == 200;}
        void send(ThorsAnvil::ThorsSocket::SocketStream& socket, std::filesystem::path const& contentDir);
    private:
        std::filesystem::path getFilePath(std::filesystem::path const& contentDir);
};

class WebServer
{
    ThorsAnvil::ThorsSocket::Server connection;
    bool                            finished;
    std::filesystem::path const&    contentDir;
    public:
        WebServer(ThorsAnvil::ThorsSocket::ServerInit&& serverInit, std::filesystem::path const& contentDir);

        void run();
    private:
        void handleConnection(ThorsAnvil::ThorsSocket::SocketStream& socket);
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

        handleConnection(socket);
    }
}

void WebServer::handleConnection(ThorsAnvil::ThorsSocket::SocketStream& socket)
{
    // Note: The requestor can send multiple requests on the same connection.
    //       So while there is data to processes then loop over it.
    while (socket)
    {
        ThorsLog("WebServer", "handleConnection", "Parsing HTTP Request");
        HttpRequest     request(socket);
        HttpResponse    response(request);
        response.send(socket, contentDir);

        if (!response.isValid())
        {
            // If there was an issue with the request.
            // Anything on the stream is suspect so close it down.
            socket.getSocket().close();
            ThorsLog("WebServer", "handleConnection", "Manualy closing connection");
            // Note: This will break the loop.
        }
    }
    ThorsLog("WebServer", "handleConnection", "Request Complete");
}

HttpRequest::HttpRequest(ThorsAnvil::ThorsSocket::SocketStream& socket)
{
    using std::literals::operator""sv;
    using std::literals::operator""s;

    std::size_t     bodySize       = 0;
    std::string     firstLine;
    if (!std::getline(socket, firstLine)) {
        status.errorCode = 400;
        status.errorMessage = "Bad Request";
        status.humanInformation = Message{} << "No HTTP Message was sent";
        ThorsLog("HttpRequest", "HttpRequest", "Bad Request: No message data was sent");
        return;
    }
    std::tie(method, URI, version)  = splitFirstLine(firstLine);
    if (method != "GET"s) {
        status.errorCode = 405;
        status.errorMessage = "Method Not Allowed";
        status.humanInformation = Message{} << "HTTP method '" << method << "' is not supported";
        firstLine.resize(firstLine.size() - 2);
        ThorsLog("HttpRequest", "HttpRequest", "Bad Request: Not A GET: ", firstLine);
        return;
    }
    if (version != "HTTP/1.1") {
        status.errorCode = 400;
        status.errorMessage = "Bad Request";
        status.humanInformation = Message{} << "HTTP version '" << version << "' is not supported";
        ThorsLog("HttpRequest", "HttpRequest", "Bad Request: Not HTTP/1.1: ", firstLine);
        return;
    }

    std::string header;
    while (status.errorCode == 200 && std::getline(socket, header) && header != "\r\n"sv)
    {
        auto [name, value] = splitHeader(header);
        if (name == "content-length"s) {
            bodySize = std::stoi(value);
        }
    }
    if (status.errorCode != 200) {
        return;
    }

    socket.ignore(bodySize);
    ThorsLog("HttpRequest", "HttpRequest", "Request: ", method, " ", URI, " ", version, " Body: ", bodySize);
}

std::tuple<std::string, std::string, std::string> HttpRequest::splitFirstLine(std::string_view firstLine)
{
    auto sep1  = firstLine.find(' ');
    auto begin = std::begin(firstLine);
    if (sep1 == std::string_view::npos) {
        return {"", "", ""};
    }
    auto sep2 = firstLine.find(' ', sep1 + 1);
    if (sep2 == std::string_view::npos) {
        return {{begin, begin + sep1}, "", ""};
    }

    auto methodBegin = std::begin(firstLine);
    auto methodEnd   = methodBegin + sep1;
    auto uriBegin    = methodEnd + 2;
    auto uriEnd      = methodBegin + sep2;
    auto verBegin    = uriEnd + 1;
    auto verEnd      = std::max(verBegin, std::end(firstLine) - 2);

    return  {{methodBegin, methodEnd}, {uriBegin, uriEnd}, {verBegin, verEnd}};
}

std::tuple<std::string, std::string> HttpRequest::splitHeader(std::string_view header)
{
    auto sep = header.find(':');
    if (sep == std::string_view::npos) {
        status.errorCode = 400;
        status.errorMessage = "Bad Request";
        status.humanInformation = Message{} << "HTTP message header badly formatted '" << header << "'";
        ThorsLog("HttpRequest", "splitHeader", "Bad Header: ", header);
        return {std::string{header}, ""};
    }
    std::string     name{std::begin(header), std::begin(header) + sep};
    std::string     value{std::begin(header) + sep + 1, std::end(header)};

    return {name, value};
}

// HttpResponse
// ============
HttpResponse::HttpResponse(HttpRequest const& request)
    : request{request}
    , status{request.getStatus()}
{}

void HttpResponse::send(ThorsAnvil::ThorsSocket::SocketStream& socket, std::filesystem::path const& contentDir)
{
    std::filesystem::path   filePath    = getFilePath(contentDir);

    if (status.errorCode != 200)
    {
        socket << "HTTP/1.1 " << status.errorCode << " " << status.errorMessage << "\r\n"
               << "message: " << status.humanInformation << "\r\n"
               << "content-length: 0\r\n"
               << "\r\n"
               << std::flush;
        ThorsLog("HttpResponse", "send", status.errorCode, " ", status.errorMessage);
        return;
    }

    std::uintmax_t fileSize = std::filesystem::file_size(filePath);
    std::ifstream   file(filePath);

    socket  << "HTTP/1.1 200 OK\r\n"
            << "content-length: " << fileSize << "\r\n"
            << "\r\n"
            << file.rdbuf()
            << std::flush;

     ThorsLog("HttpResponse", "send", "200 OK");
}

std::filesystem::path HttpResponse::getFilePath(std::filesystem::path const& contentDir)
{
    if (status.errorCode != 200) {
        return {};
    }

    std::filesystem::path   uriPath{request.getURI()};
    std::filesystem::path   requestPath = std::filesystem::path{uriPath}.lexically_normal();

    if (requestPath.empty() || (*requestPath.begin()) == "..") {
        status.errorCode = 400;
        status.errorMessage = "Bad Request";
        status.humanInformation = Message{} << "Invalid Request Path: " << requestPath;
        ThorsLog("HttpResponse", "getFilePath", "Invalid request path: ", requestPath);
        return {};
    }

    std::error_code ec;
    std::filesystem::path   filePath = std::filesystem::canonical(std::filesystem::path{contentDir} /= requestPath, ec);
    if (!ec && std::filesystem::is_directory(filePath)) {
        filePath = std::filesystem::canonical(filePath /= "index.html", ec);
    }
    if (ec || !std::filesystem::is_regular_file(filePath)) {
        status.errorCode = 404;
        status.errorMessage = "Not Found";
        status.humanInformation = Message{} << "No file found at: " << uriPath;
        ThorsLog("HttpResponse", "getFilePath", "Invalid file path: ", filePath, " for URI ", uriPath);
        return {};
    }

    ThorsLog("HttpResponse", "getFilePath", "File: ", filePath);
    return filePath;
}


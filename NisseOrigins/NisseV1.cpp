#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
#include <tuple>
#include <exception>
#include <stdexcept>

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <arpa/inet.h>
#include <unistd.h>

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
    int             errorCode;
    std::string     errorMessage;
    std::string     humanInformation;

    std::string     method;
    std::string     URI;
    std::string     version;

    public:
        HttpRequest(Socket& socket);
        bool isValid() const {return errorCode == 200;}

    private:
        std::tuple<std::string, std::string>                splitHeader(std::string_view header);
        std::tuple<std::string, std::string, std::string>   splitFirstLine(std::string_view firstLine);
};

class HttpResponse
{
    public:
        HttpResponse(HttpRequest const& request);

        void send(Socket& socket);
};

class Socket
{
    int                 fd;
    std::vector<char>   buffer;
    std::string_view    currentLine;
    bool                moreData;
    public:
        Socket(int fd);
        ~Socket();

        Socket(Socket&& move)               noexcept;
        Socket& operator=(Socket&& move)    noexcept;
        void swap(Socket& other)            noexcept;

        friend void swap(Socket& lhs, Socket& rhs)  {lhs.swap(rhs);}

        Socket(Socket const&)               = delete;
        Socket& operator=(Socket const&)    = delete;

        std::string_view    getNextLine();
        void ignore(std::size_t size);

        bool isOpen()   const {return fd != 0;}
        bool hasData()  const {return !buffer.empty() || moreData;}
        void close();
    private:
        void removeCurrentLine();
        bool checkLineInBuffer();
        void readMoreData(std::size_t maxSize, bool required = false);
};

class Server
{
    static constexpr int backlog = 5;
    int fd;
    public:
        Server(int port);
        ~Server();

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
    // Note: The requestor can send multiple requests on the same connection.
    //       So while there is data to processes then loop over it.
    while (socket.hasData())
    {
        HttpRequest     request(socket);
        HttpResponse    response(request);
        response.send(socket);

        if (!request.isValid())
        {
            // If there was an issue with the request.
            // Anything on the stream is suspect so close it down.
            socket.close();
            // Note: This will break the loop.
        }
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

Server::~Server()
{
    int closeStatus = ::close(fd);
    if (closeStatus == -1) {
        std::cerr << "Failed to close Server: " << errno << " " << strerror(errno) << "\n";
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
    : fd{fd}
    , moreData{true}
{
    buffer.reserve(1000);
}

Socket::~Socket()
{
    close();
}

Socket::Socket(Socket&& move) noexcept
{
    close();
    swap(move);
}

Socket& Socket::operator=(Socket&& move) noexcept
{
    close();
    swap(move);
    return *this;
}

void Socket::swap(Socket& other) noexcept
{
    using std::swap;
    swap(fd,            other.fd);
    swap(buffer,        other.buffer);
    swap(currentLine,   other.currentLine);
    swap(moreData,      other.moreData);
}

void Socket::close()
{
    if (fd != 0)
    {
        int closeStatus = ::close(fd);
        if (closeStatus == -1) {
            std::cerr << "Failed to close socket: " << errno << " " << strerror(errno) << "\n";
        }
        fd = 0;
        buffer.clear();
        currentLine ="";
        moreData = false;
    }
}

std::string_view Socket::getNextLine()
{
    removeCurrentLine();

    if (checkLineInBuffer()) {
        return currentLine;
    }

    while (moreData)
    {
        readMoreData(500);
        if (checkLineInBuffer()) {
            return currentLine;
        }
    }

    currentLine = {std::begin(buffer), std::end(buffer)};
    return currentLine;
}

void Socket::ignore(std::size_t size)
{
    removeCurrentLine();
    currentLine = "";

    if (std::size(buffer) > size) {
        std::move(std::begin(buffer) + size, std::end(buffer), std::begin(buffer));
        buffer.resize(std::size(buffer) - size);
        return;
    }

    size -= std::size(buffer);
    buffer.clear();

    readMoreData(size, true);
    buffer.clear();
}

void Socket::removeCurrentLine()
{
    if (std::size(currentLine) == std::size(buffer)) {
        buffer.clear();
    }
    else {
        std::move(std::begin(buffer) + std::size(currentLine), std::end(buffer), std::begin(buffer));
        buffer.resize(std::size(buffer) - std::size(currentLine));
    }
}

bool Socket::checkLineInBuffer()
{
    std::string_view bufferView{std::begin(buffer), std::end(buffer)};
    std::size_t find = bufferView.find("\r\n");
    if (find != std::string_view::npos) {
        currentLine = {std::begin(buffer), std::begin(buffer) + find + 2};
        return true;
    }
    return false;
}

void Socket::readMoreData(std::size_t maxSize, bool required)
{
    std::size_t     currentSize = std::size(buffer);
    std::size_t     amountRead  = 0;
    buffer.resize(currentSize + maxSize);

    while (moreData && amountRead != maxSize)
    {
        int nextChunk = ::read(fd, &buffer[0] + currentSize + amountRead, maxSize - amountRead);
        if (nextChunk == -1 && errno == EINTR) {
            continue;
        }
        if (nextChunk == -1) {
            throw std::runtime_error(ErrorMessage{} << "Catastrophic read failure: " << errno << " " << strerror(errno));
        }
        if (nextChunk == 0) {
            // Stream closed.
            moreData = false;
        }
        amountRead += nextChunk;
        if (!required) {
            break;
        }
    }
    buffer.resize(currentSize + amountRead);
}


// HttpRequest
// ===========
HttpRequest::HttpRequest(Socket& socket)
    : errorCode{200}
    , errorMessage{"OK"}
{
    using std::literals::operator""sv;
    using std::literals::operator""s;

    std::size_t      bodySize       = 0;
    std::string_view firstLine      = socket.getNextLine();
    std::tie(method, URI, version)  = splitFirstLine(firstLine);
    if (method != "GET"s) {
        errorCode = 405;
        errorMessage = "Method Not Allowed";
        humanInformation = ErrorMessage{} << "HTTP method '" << method << "' is not supported";
        return;
    }
    if (version != "HTTP/1.1") {
        errorCode = 400;
        errorMessage = "Bad Request";
        humanInformation = ErrorMessage{} << "HTTP version '" << version << "' is not supported";
        return;
    }

    std::string_view header;
    while ((header = socket.getNextLine()) != "\r\n"sv && errorCode == 200)
    {
        auto [name, value] = splitHeader(header);
        if (name == "content-length"s) {
            bodySize = std::stoi(value);
        }
    }
    if (errorCode == 200) {
        socket.ignore(bodySize);
    }
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
        errorCode = 400;
        errorMessage = "Bad Request";
        humanInformation = ErrorMessage{} << "HTTP message header badly formatted '" << header << "'";
        return {std::string{header}, ""};
    }
    std::string     name{std::begin(header), std::begin(header) + sep};
    std::string     value{std::begin(header) + sep + 1, std::end(header)};

    return {name, value};
}

// HttpResponse
// ============
HttpResponse::HttpResponse(HttpRequest const& request)
{}

void HttpResponse::send(Socket& socket)
{}


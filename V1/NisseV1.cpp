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
 *      Socket:             A Unix socket connection that has been established.
 *                          It acts like a bi-directional communication channel.
 *                          It has an internal buffer to track requests.
 *      Server:             A Unix socket listening for incoming connections.
 *      WebServer:          A class to represent and manage incoming connections.
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

class Socket;
class HttpRequest
{
    ErrorStatus     status;

    std::string     method;
    std::string     URI;
    std::string     version;

    public:
        HttpRequest(Socket& socket);
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
        void send(Socket& socket, std::filesystem::path const& contentDir);
    private:
        std::filesystem::path getFilePath(std::filesystem::path const& contentDir);
};

class Socket
{
    static constexpr std::size_t    inputBufferGrowth = 500;
    static constexpr std::size_t    outputBufferMax   = 1000;
    int                 fd;
    std::vector<char>   buffer;
    std::vector<char>   outputBuffer;
    std::string_view    currentLine;
    bool                readAvail;
    bool                writeAvail;
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

        void sendMessage(std::string const& message);

        void sync();

        bool isOpen()   const {return fd != 0;}
        bool hasData()  const {return !buffer.empty() || readAvail;}
        void close();
    private:
        void removeCurrentLine();
        bool checkLineInBuffer();
        void readMoreData(std::size_t maxSize, bool required = false);
        void sendData(char const* buffer, std::size_t size);
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
    Server                          connection;
    bool                            finished;
    std::filesystem::path const&    contentDir;
    public:
        WebServer(int port, std::filesystem::path const& contentDir);

        void run();
    private:
        void handleConnection(Socket& socket);
};

// The application body.


int main(int argc, char* argv[])
{
    if (argc != 3)
    {
        std::cerr << "Usage: NisseV1 <port> <documentPath>" << "\n";
        return 1;
    }

    try
    {
        static const int port = std::stoi(argv[1]);
        static const std::filesystem::path  contentDir  = std::filesystem::canonical(argv[2]);

        std::cout << "Nisse Proto 1\n";
        WebServer   server(port, contentDir);
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
WebServer::WebServer(int port, std::filesystem::path const& contentDir)
    : connection{port}
    , finished{false}
    , contentDir{contentDir}
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
        std::clog << "  Parsing HTTP Request\n";
        HttpRequest     request(socket);
        HttpResponse    response(request);
        response.send(socket, contentDir);

        if (!response.isValid())
        {
            // If there was an issue with the request.
            // Anything on the stream is suspect so close it down.
            socket.close();
            std::clog << "  Manualy closing connection\n";
            // Note: This will break the loop.
        }
    }
    std::clog << "  Request Complete\n";
}

// Server
// ======
Server::Server(int port)
{
    fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        throw std::runtime_error{Message{} << "Failed to create socket: " << errno << " " << strerror(errno)};
    }

    struct ::sockaddr_in        serverAddr{};
    serverAddr.sin_family       = AF_INET;
    serverAddr.sin_port         = htons(port);
    serverAddr.sin_addr.s_addr  = INADDR_ANY;

    int bindStatus = ::bind(fd, reinterpret_cast<struct ::sockaddr*>(&serverAddr), sizeof(serverAddr));
    if (bindStatus == -1) {
        throw std::runtime_error{Message{} << "Failed to bind socket: " << errno << " " << strerror(errno)};
    }

    int listenStatus = ::listen(fd, backlog);
    if (listenStatus == -1) {
        throw std::runtime_error{Message{} << "Failed to listen socket: " << errno << " " << strerror(errno)};
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
        std::clog << "Accepted Connection\n";
        if (accept == -1) {
            throw std::runtime_error{Message{} << "Failed to accept socket: " << errno << " " << strerror(errno)};
        }
        return Socket{accept};
    }
}

// Socket
// ======
Socket::Socket(int fd)
    : fd{fd}
    , readAvail{true}
    , writeAvail{true}
{
    buffer.reserve(outputBufferMax);
    outputBuffer.reserve(outputBufferMax);
}

Socket::~Socket()
{
    close();
}

Socket::Socket(Socket&& move) noexcept
    : fd(-1)
    , readAvail{false}
    , writeAvail{false}
{
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
    swap(outputBuffer,  other.outputBuffer);
    swap(currentLine,   other.currentLine);
    swap(readAvail,     other.readAvail);
    swap(writeAvail,    other.writeAvail);
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
        outputBuffer.clear();
        currentLine ="";
        readAvail = false;
        writeAvail = false;
    }
}

std::string_view Socket::getNextLine()
{
    removeCurrentLine();

    if (checkLineInBuffer()) {
        return currentLine;
    }

    while (readAvail)
    {
        readMoreData(inputBufferGrowth);
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

    while (readAvail && amountRead != maxSize)
    {
        int nextChunk = ::read(fd, &buffer[0] + currentSize + amountRead, maxSize - amountRead);
        if (nextChunk == -1 && errno == EINTR) {
            continue;
        }
        if (nextChunk == -1 && errno == ECONNRESET) {
            readAvail = false;
            break;
        }
        if (nextChunk == -1) {
            throw std::runtime_error(Message{} << "Catastrophic read failure: " << errno << " " << strerror(errno));
        }
        if (nextChunk == 0) {
            // Stream closed.
            readAvail = false;
        }
        amountRead += nextChunk;
        if (!required) {
            break;
        }
    }
    buffer.resize(currentSize + amountRead);
}

void Socket::sendMessage(std::string const& message)
{
    if (!writeAvail) {
        return;
    }
    if (outputBuffer.size() + message.size() > outputBufferMax)
    {
        sync();
        sendData(&message[0], std::size(message));
    }
    else {
        std::copy(std::begin(message), std::end(message), std::back_inserter(outputBuffer));
    }
}

void Socket::sync()
{
    if (std::size(outputBuffer) > 0)
    {
        sendData(&outputBuffer[0], std::size(outputBuffer));
        outputBuffer.clear();
    }
}

void Socket::sendData(char const* data, std::size_t size)
{
    std::size_t sentData = 0;
    while (writeAvail && sentData != size)
    {
        ::ssize_t writeStatus = ::write(fd, data + sentData, size - sentData);
        if (writeStatus == -1 && errno == EINTR) {
            continue;
        }
        if (writeStatus == -1 && errno == ECONNRESET) {
            writeAvail = false;
            break;
        }
        if (writeStatus == -1) {
            throw std::runtime_error(Message{} << "Failed to write: " << fd << " Code: " << errno << " " << strerror(errno));
        }
        sentData += writeStatus;
    }
}

// HttpRequest
// ===========
HttpRequest::HttpRequest(Socket& socket)
{
    using std::literals::operator""sv;
    using std::literals::operator""s;

    std::size_t      bodySize       = 0;
    std::string_view firstLine      = socket.getNextLine();
    std::tie(method, URI, version)  = splitFirstLine(firstLine);
    if (method != "GET"s) {
        status.errorCode = 405;
        status.errorMessage = "Method Not Allowed";
        status.humanInformation = Message{} << "HTTP method '" << method << "' is not supported";
        firstLine.remove_suffix(2);
        std::clog << "  Bad Request: Not A GET: " << firstLine << "\n";
        return;
    }
    if (version != "HTTP/1.1") {
        status.errorCode = 400;
        status.errorMessage = "Bad Request";
        status.humanInformation = Message{} << "HTTP version '" << version << "' is not supported";
        std::clog << "  Bad Request: Not HTTP/1.1: " << firstLine << "\n";
        return;
    }

    std::string_view header;
    while (status.errorCode == 200 && (header = socket.getNextLine()) != "\r\n"sv)
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
    std::clog << "  Request: " << method << " " << URI << " " << version << " Body: " << bodySize << "\n";
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
        std::clog << "  Bad Header: " << header << "\n";
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

void HttpResponse::send(Socket& socket, std::filesystem::path const& contentDir)
{
    std::filesystem::path   filePath    = getFilePath(contentDir);

    if (status.errorCode != 200)
    {
        socket.sendMessage(Message{} << "HTTP/1.1 " << status.errorCode << " " << status.errorMessage << "\r\n");
        socket.sendMessage(Message{} << "message: " << status.humanInformation << "\r\n");
        socket.sendMessage("content-length: 0\r\n");
        socket.sendMessage("\r\n");
        socket.sync();
        std::clog << "  Send: " << status.errorCode << " " << status.errorMessage << "\n";
        return;
    }

    std::uintmax_t fileSize = std::filesystem::file_size(filePath);

    socket.sendMessage("HTTP/1.1 200 OK\r\n");
    socket.sendMessage(Message{} << "content-length: " << fileSize << "\r\n");
    socket.sendMessage("\r\n");

    std::ifstream   file(filePath);

    std::string line;
    while (std::getline(file, line))
    {
        socket.sendMessage(line);
        if (file) {
            socket.sendMessage("\n");
        }
    }
    std::clog << "  Send: 200 OK\n";
    socket.sync();
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
        std::clog << "  Invalid request path: " << requestPath << "\n";
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
        std::clog << "  Invalid file path: " << filePath << " for URI " << uriPath << "\n";
        return {};
    }

    std::clog << "  File: " << filePath << "\n";
    return filePath;
}


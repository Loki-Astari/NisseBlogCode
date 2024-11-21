#include "Stream.h"

#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <exception>

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <arpa/inet.h>
#include <unistd.h>

/*
 * Class Declarations:
 *
 *      Socket:             A Unix socket connection that has been established.
 *                          It acts like a bi-directional communication channel.
 *                          It has an internal buffer to track requests.
 *      Server:             A Unix socket listening for incoming connections.
 *      WebServer:          A class to represent and manage incoming connections.
 */

class Socket: public Stream
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

        std::string_view    getNextLine()   override;
        void ignore(std::size_t size)       override;

        void sendMessage(std::string const& message)    override;
        void sync()                                     override;

        bool isOpen()   const {return fd != 0;}
        bool hasData()  const   override {return !buffer.empty() || readAvail;}
        void close()            override;
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

        handleConnection(socket, contentDir);
    }
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
    // This function read "MoreData" onto the end of buffer.
    // Note: There may be data already in buffer so this appends it.
    //       We will read no more than "maxSize" more data into buffer.
    // The required flag indicates if we must read "maxSize" if true
    // the loop will continue until we get all the data otherwise the
    // function returns after any data is received.
    std::size_t     currentSize = std::size(buffer);
    std::size_t     amountRead  = 0;
    buffer.resize(currentSize + maxSize);

    while (readAvail && amountRead != maxSize)
    {
        int nextChunk = ::read(fd, &buffer[0] + currentSize + amountRead, maxSize - amountRead);
        if (nextChunk == -1 && errno == EINTR) {
            continue;           // An interrupt can be ignored. Simply try again.
        }
        if (nextChunk == -1 && errno == ECONNRESET) {
            readAvail = false;  // The client dropped the connection. Not a problem
            break;              // But no more data can be read from the socket.
        }
        if (nextChunk == -1) {
            buffer.resize(currentSize + amountRead);
            throw std::runtime_error(Message{} << "Catastrophic read failure: " << errno << " " << strerror(errno));
        }
        if (nextChunk == 0) {   // The connection was closed gracefully.
            readAvail = false;  // OS handled all the niceties.
        }
        amountRead += nextChunk;
        if (!required) {
            break;  // have some data. Lets exit and see if it is enough.
        }
    }
    // Make sure to set the buffer to the correct size.
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


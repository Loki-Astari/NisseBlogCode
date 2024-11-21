#include "Stream.h"

#include <iostream>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>
#include <tuple>
#include <filesystem>

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <arpa/inet.h>
#include <unistd.h>

/*
 * Class Declarations:
 *
 *      ErrorStatus:        Error state that is reported back on each request.
 *      HttpRequest:        An HTTP request object that has been read from a 'Stream'.
 *      HttpResponse:       An HTTP response object that can be written to a 'Stream' in
 *                          response to an HttpRequest.
 */

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
        HttpRequest(Stream& socket);
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
        void send(Stream& socket, std::filesystem::path const& contentDir);
    private:
        std::filesystem::path getFilePath(std::filesystem::path const& contentDir);
};

// HttpRequest
// ===========
HttpRequest::HttpRequest(Stream& socket)
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

void HttpResponse::send(Stream& socket, std::filesystem::path const& contentDir)
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

void handleConnection(Stream& socket, std::filesystem::path const& contentDir)
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


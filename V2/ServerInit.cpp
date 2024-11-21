#include "ServerInit.h"

namespace TASock    = ThorsAnvil::ThorsSocket;

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


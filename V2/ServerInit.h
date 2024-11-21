#ifndef SERVIER_INIT_H
#define SERVIER_INIT_H

#include <ThorsSocket/Server.h>
#include <filesystem>
#include <optional>

ThorsAnvil::ThorsSocket::ServerInit getServerInit(int port, std::optional<std::filesystem::path> certPath);

#endif

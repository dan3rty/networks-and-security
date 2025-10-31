#pragma once

#include "DataPacket.h"

namespace PacketProtocol
{
bool sendPacket(int sockFd, const DataPacket& packet);

bool receivePacket(int sockFd, DataPacket& outPacket);
} // namespace PacketProtocol
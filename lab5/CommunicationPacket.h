#pragma once

#include <cstdint>

constexpr int MAX_NAME_LENGTH = 128;

struct CommunicationPacket
{
	char name[MAX_NAME_LENGTH];
	int32_t number;
};
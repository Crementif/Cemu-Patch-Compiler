#pragma once
#include <string>
#include <string_view>

bool isForbiddenInstruction(const char* name);
std::string assembleInstructionToBytes(std::string_view instruction);

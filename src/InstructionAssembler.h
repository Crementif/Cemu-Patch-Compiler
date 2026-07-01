#pragma once
#include "common.h"

bool isForbiddenInstruction(const char* name);
std::string assembleInstructionToBytes(std::string_view instruction);

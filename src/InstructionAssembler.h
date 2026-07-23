#pragma once

void addHardcodedInstruction(std::string_view name);
void addHardcodedInstructions(std::string_view instructionList);
bool isForbiddenInstruction(const char* name);
std::string assembleInstructionToBytes(std::string_view instruction);


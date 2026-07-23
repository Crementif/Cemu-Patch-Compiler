#pragma once

std::string AsmUpdater_fixInstruction(std::string_view instructionText);
std::string AsmUpdater_fixClangLocalLabels(uint64_t unitHash,
                                           std::string instructionText);

#include "common.h"
#include "InstructionAssembler.h"
#include "StringParser.h"

struct DissectedInstruction
{
	std::string name;
	std::vector<std::string_view> operandStr;
};

static std::string_view trimWhitespaces(std::string_view sv)
{
	while (!sv.empty() && (sv.front() == ' ' || sv.front() == '\t'))
	{
		sv.remove_prefix(1);
	}
	while (!sv.empty() && (sv.back() == ' ' || sv.back() == '\t'))
	{
		sv.remove_suffix(1);
	}
	return sv;
}

bool dissectInstruction(std::string_view instructionText, DissectedInstruction& dissectedInstruction)
{
	// cut off comments
	size_t commentPos = instructionText.find('#');
	if (commentPos != std::string_view::npos)
	{
		instructionText = instructionText.substr(0, commentPos);
	}

	instructionText = trimWhitespaces(instructionText);
	if (instructionText.empty())
	{
		return false;
	}

	// parse name of instruction
	size_t nameEnd = instructionText.find_first_of(" \t");
	std::string_view namePart;
	std::string_view operandsPart;
	if (nameEnd == std::string_view::npos)
	{
		namePart = instructionText;
	}
	else
	{
		namePart = instructionText.substr(0, nameEnd);
		operandsPart = instructionText.substr(nameEnd);
	}

	dissectedInstruction.name.clear();
	for (char c : namePart)
	{
		dissectedInstruction.name.push_back((char)toupper((unsigned char)c));
	}

	// parse operands
	dissectedInstruction.operandStr.clear();
	if (!operandsPart.empty())
	{
		size_t start = 0;
		while (start < operandsPart.size())
		{
			size_t comma = operandsPart.find(',', start);
			std::string_view operand;
			if (comma == std::string_view::npos)
			{
				operand = operandsPart.substr(start);
				start = operandsPart.size();
			}
			else
			{
				operand = operandsPart.substr(start, comma - start);
				start = comma + 1;
			}
			dissectedInstruction.operandStr.push_back(trimWhitespaces(operand));
		}
	}

	return true;
}

struct DissectedMemOperand
{
	std::string_view offsetStr;
	std::string_view memRegisterStr;

	// dissect operands of format <offset_expression>(<register_text>)
	bool dissect(std::string_view operandText)
	{
		operandText = trimWhitespaces(operandText);
		if (operandText.empty())
		{
			return false;
		}

		if (operandText.back() != ')')
		{
			return false;
		}

		size_t openParen = operandText.find('(');
		if (openParen == std::string_view::npos)
		{
			return false;
		}

		offsetStr = trimWhitespaces(operandText.substr(0, openParen));
		memRegisterStr = trimWhitespaces(operandText.substr(openParen + 1, operandText.size() - openParen - 2));
		return true;
	}
};

static bool maskToMBME(uint32_t mask, int& mb, int& me)
{
	if (mask == 0)
		return false;

	int low = 0;
	while (low < 32 && ((mask >> low) & 1) == 0)
		low++;

	int high = 31;
	while (high >= 0 && ((mask >> high) & 1) == 0)
		high--;

	uint32_t expected = 0;
	for (int i = low; i <= high; i++)
		expected |= (1u << i);

	if (mask != expected)
		return false;

	mb = 31 - high;
	me = 31 - low;
	return true;
}

std::string AsmUpdater_fixInstruction(std::string_view instructionText)
{
	DissectedInstruction di;
	if (dissectInstruction(instructionText, di))
	{
		if (isForbiddenInstruction(di.name.c_str()))
		{
			std::string hexVal = assembleInstructionToBytes(instructionText);
			if (!hexVal.empty())
			{
				return std::format("\t.int {}", hexVal);
			}
		}

		if (di.name == "LA")
		{
			// replace LA with ADDI
			// in:		la r4, 123(r4)
			// out:		addi r4, r4, 123
			if (di.operandStr.size() != 2)
			{
				__debugbreak();
			}

			DissectedMemOperand dim;
			if (!dim.dissect(di.operandStr[1]))
			{
				__debugbreak();
			}

			return std::format("\taddi {}, {}, {}", di.operandStr[0], dim.memRegisterStr, dim.offsetStr);
		}

		if (di.name == "RLWINM" && di.operandStr.size() == 4)
		{
			// gcc will emit rlwinm with a bitmask as the 4th operand
			// convert mask to MB/ME form that the Cemu assembler expects
			StringTokenParser maskParser(di.operandStr[3]);
			uint32_t maskVal;
			if (maskParser.parseU32(maskVal))
			{
				int mb, me;
				if (maskToMBME(maskVal, mb, me))
				{
					return std::format("\trlwinm {}, {}, {}, {}, {}", di.operandStr[0], di.operandStr[1], di.operandStr[2], mb, me);
				}
			}
			printf("Warning: RLWINM with unsupported mask value '%.*s'\n", (int)di.operandStr[3].size(), di.operandStr[3].data());
			return std::string(instructionText);
		}

		// replace bnl with bge (the former alias is not supported)
		if (di.name == "BNL")
		{
			if (di.operandStr.size() != 2)
			{
				__debugbreak();
			}
			return std::format("\tbge {}, {}", di.operandStr[0], di.operandStr[1]);
		}

		// remove branch hints
		if (!di.name.empty())
		{
			char lastChar = di.name.back();
			if (lastChar == '+' || lastChar == '-')
			{
				di.name.pop_back();

				std::string newInstruction = std::format("\t{}", di.name);
				for (size_t i = 0; i < di.operandStr.size(); i++)
				{
					if (i == 0)
					{
						newInstruction.append(" ");
					}
					else
					{
						newInstruction.append(", ");
					}
					newInstruction.append(di.operandStr[i]);
				}
				return newInstruction;
			}
		}
	}
	return std::string(instructionText);
}

std::string AsmUpdater_changeRegisterSyntax(std::string& instructionText)
{
	// %r9, %f0
	std::regex registerMatchRegex("%((?:cr|r|f)\\d\\d?)", std::regex::ECMAScript);
	return std::regex_replace(instructionText, registerMatchRegex, "$1");
}

std::string AsmUpdater_fixDotLabels(uint64_t unitHash, std::string instructionText)
{
	// labels starting with a dot are private/local/weak labels that aren't meant to be globally visible
	// to avoid name conflict between multiple translation unit we add a file-specific prefix to each label
	std::string replacement = std::format("_{:016x}_$1", unitHash);

	// lis r9,.LC0@ha
	// .LC0:
	// .int 1056964608
	std::regex constantMatchRegex(R"(\.(LC\d+))", std::regex::ECMAScript);
	instructionText = std::regex_replace(instructionText, constantMatchRegex, replacement);

	// .L101:
	// 	b .L101
	std::regex branchingGotoLabelsMatchRegex(R"(\.(L\d+))", std::regex::ECMAScript);
	return std::regex_replace(instructionText, branchingGotoLabelsMatchRegex, replacement);
}
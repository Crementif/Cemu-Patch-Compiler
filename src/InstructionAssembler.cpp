#include "InstructionAssembler.h"
#include "common.h"

const char* forbiddenInstructions[] = {
	"MFCR",
	"MTCRF"
};

bool isForbiddenInstruction(const char* name)
{
	if (g_compilerMode != CompilerMode::GCC)
	{
		return false;
	}
	for (const auto& forbidden : forbiddenInstructions)
	{
		if (strcmp(name, forbidden) == 0)
			return true;
	}
	return false;
}

static bool runCommand(const std::string& cmdline)
{
	STARTUPINFOA siStartInfo;
	PROCESS_INFORMATION piProcInfo;
	ZeroMemory(&siStartInfo, sizeof(STARTUPINFOA));
	siStartInfo.cb = sizeof(STARTUPINFOA);
	ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));

	std::vector<char> cmdCopy(cmdline.begin(), cmdline.end());
	cmdCopy.push_back('\0');

	BOOL bSuccess = CreateProcessA(
		NULL,
		cmdCopy.data(),
		NULL,
		NULL,
		TRUE,
		0,
		NULL,
		NULL,
		&siStartInfo,
		&piProcInfo
	);

	if (!bSuccess)
	{
		return false;
	}

	CloseHandle(piProcInfo.hThread);
	WaitForSingleObject(piProcInfo.hProcess, INFINITE);
	DWORD exitCode;
	GetExitCodeProcess(piProcInfo.hProcess, &exitCode);
	CloseHandle(piProcInfo.hProcess);

	return exitCode == 0;
}

std::string assembleInstructionToBytes(std::string_view instruction)
{
	if (g_compilerMode != CompilerMode::GCC)
	{
		return "";
	}
	std::string instructionStr(instruction);
	std::error_code ec;

	// Write instruction to temp_assemble.s
	{
		std::ofstream out("temp_assemble.s", std::ios::binary);
		if (!out)
			return "";
		out << ".text\n\t" << instructionStr << "\n";
	}

	// Resolve objcopy path relative to g_gccPath
	std::filesystem::path gccPath(g_gccPath);
	std::string objcopyPath = (gccPath.parent_path() / "powerpc-eabi-objcopy.exe").string();

	// Compile using powerpc-eabi-gcc with target specific settings from main program
	std::string asCmd = g_gccPath + " -mcpu=750 -mbig-endian -m32 -mregnames -mcall-eabi -c temp_assemble.s -o temp_assemble.o";
	if (!runCommand(asCmd))
	{
		std::filesystem::remove("temp_assemble.s", ec);
		return "";
	}

	// Extract raw binary using powerpc-eabi-objcopy
	std::string objcopyCmd = objcopyPath + " -O binary --only-section=.text temp_assemble.o temp_assemble.bin";
	if (!runCommand(objcopyCmd))
	{
		std::filesystem::remove("temp_assemble.s", ec);
		std::filesystem::remove("temp_assemble.o", ec);
		return "";
	}

	// Read binary
	unsigned char bytes[4] = {0};
	{
		std::ifstream in("temp_assemble.bin", std::ios::binary);
		if (in)
		{
			in.read(reinterpret_cast<char*>(bytes), 4);
		}
	}

	// Clean up
	std::filesystem::remove("temp_assemble.s", ec);
	std::filesystem::remove("temp_assemble.o", ec);
	std::filesystem::remove("temp_assemble.bin", ec);

	uint32_t val = ((uint32_t)bytes[0] << 24) | ((uint32_t)bytes[1] << 16) | ((uint32_t)bytes[2] << 8) | (uint32_t)bytes[3];
	
	// Format as hex
	return std::format("0x{:08X}", val);
}

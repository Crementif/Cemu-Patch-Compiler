#include "InstructionAssembler.h"

#include "llvm/MC/MCAsmBackend.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCCodeEmitter.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCObjectFileInfo.h"
#include "llvm/MC/MCObjectWriter.h"
#include "llvm/MC/MCParser/MCAsmParser.h"
#include "llvm/MC/MCParser/MCTargetAsmParser.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/MCTargetOptions.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Object/ObjectFile.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/TargetParser/Triple.h"

static std::set<std::string> g_hardcodedInstructions = {
	"MFCR",
	"MTCRF",
	"MCRF",
	"BCLR",
	"BCCTR",
	"FRSQRTE",
	"FABS",
	"ADDC",
	"ADDC.",
	"ADDE",
	"ADDE.",
	"ADDZE",
	"ADDZE.",
	"ADDME",
	"ADDME.",
	"SUBC",
	"SUBC.",
	"SUBFZE",
	"SUBFZE.",
	"SUBFME",
	"SUBFME.",
	"NAND",
	"NAND.",
	"EQV",
	"EQV."
};

void addHardcodedInstruction(std::string_view name)
{
	std::string upper;
	for (char c : name)
	{
		if (!std::isspace(static_cast<unsigned char>(c)))
		{
			upper.push_back((char)toupper(static_cast<unsigned char>(c)));
		}
	}
	if (!upper.empty())
	{
		g_hardcodedInstructions.insert(upper);
	}
}

void addHardcodedInstructions(std::string_view instructionList)
{
	size_t start = 0;
	while (start < instructionList.size())
	{
		size_t pos = instructionList.find_first_of(",;\t\r\n ", start);
		std::string_view token = (pos == std::string_view::npos)
			                         ? instructionList.substr(start)
			                         : instructionList.substr(start, pos - start);
		if (!token.empty())
		{
			addHardcodedInstruction(token);
		}
		if (pos == std::string_view::npos)
			break;
		start = pos + 1;
	}
}

bool isForbiddenInstruction(const char* name)
{
	if (!name || !*name)
		return false;
	std::string upper;
	for (const char* p = name; *p; ++p)
	{
		upper.push_back((char)toupper(static_cast<unsigned char>(*p)));
	}
	return g_hardcodedInstructions.contains(upper);
}

static std::string getTextSectionBytes(llvm::MemoryBufferRef objectBuffer)
{
	auto objectOrErr = llvm::object::ObjectFile::createObjectFile(objectBuffer);
	if (!objectOrErr)
	{
		llvm::consumeError(objectOrErr.takeError());
		return "";
	}
	for (const auto& section : objectOrErr.get()->sections())
	{
		auto nameOrErr = section.getName();
		if (!nameOrErr || nameOrErr.get() != ".text")
			continue;
		auto contentsOrErr = section.getContents();
		if (!contentsOrErr)
		{
			llvm::consumeError(contentsOrErr.takeError());
			return "";
		}
		return contentsOrErr.get().str();
	}
	return "";
}

// Assembles the instruction with LLVM's integrated assembler into an in-memory object file, like llvm-mc does
static std::string assembleInstructionToBytesUncached(std::string_view instruction)
{
	static bool targetsInitialized = false;
	if (!targetsInitialized)
	{
		llvm::InitializeAllTargetInfos();
		llvm::InitializeAllTargetMCs();
		llvm::InitializeAllAsmParsers();
		targetsInitialized = true;
	}

	const llvm::Triple triple("powerpc-unknown-eabi");
	std::string error;
	const llvm::Target* target = llvm::TargetRegistry::lookupTarget(triple.getTriple(), error);
	if (!target)
		return "";

	llvm::MCTargetOptions mcOptions;
	std::unique_ptr<llvm::MCRegisterInfo> registerInfo(target->createMCRegInfo(triple.getTriple()));
	std::unique_ptr<llvm::MCAsmInfo> asmInfo(target->createMCAsmInfo(*registerInfo, triple.getTriple(), mcOptions));
	std::unique_ptr<llvm::MCInstrInfo> instrInfo(target->createMCInstrInfo());
	std::unique_ptr<llvm::MCSubtargetInfo> subtargetInfo(target->createMCSubtargetInfo(triple.getTriple(), "750", ""));

	// The assembler expects numeric register operands (r3 -> 3, cr0 -> 0), unlike the compiler output which uses
	// -ppc-asm-full-reg-names
	std::string instructionStr(instruction);
	instructionStr = std::regex_replace(instructionStr, std::regex(R"(\br([0-9]+)\b)"), "$1");
	instructionStr = std::regex_replace(instructionStr, std::regex(R"(\bf([0-9]+)\b)"), "$1");
	instructionStr = std::regex_replace(instructionStr, std::regex(R"(\bcr([0-7])\b)"), "$1");

	llvm::SourceMgr sourceMgr;
	sourceMgr.AddNewSourceBuffer(llvm::MemoryBuffer::getMemBufferCopy(instructionStr + "\n"), llvm::SMLoc());
	llvm::MCContext context(triple, asmInfo.get(), registerInfo.get(), subtargetInfo.get(), &sourceMgr, &mcOptions);
	std::unique_ptr<llvm::MCObjectFileInfo> objectFileInfo(target->createMCObjectFileInfo(context, false));
	context.setObjectFileInfo(objectFileInfo.get());

	llvm::SmallString<256> objectData;
	llvm::raw_svector_ostream objectStream(objectData);
	std::unique_ptr<llvm::MCAsmBackend> asmBackend(target->createMCAsmBackend(*subtargetInfo, *registerInfo, mcOptions));
	std::unique_ptr<llvm::MCCodeEmitter> codeEmitter(target->createMCCodeEmitter(*instrInfo, context));
	if (!asmBackend || !codeEmitter)
		return "";
	std::unique_ptr<llvm::MCObjectWriter> objectWriter = asmBackend->createObjectWriter(objectStream);
	std::unique_ptr<llvm::MCStreamer> streamer(target->createMCObjectStreamer(
		triple, context, std::move(asmBackend), std::move(objectWriter), std::move(codeEmitter), *subtargetInfo,
		false, false, false));

	std::unique_ptr<llvm::MCAsmParser> parser(llvm::createMCAsmParser(sourceMgr, context, *streamer, *asmInfo));
	std::unique_ptr<llvm::MCTargetAsmParser> targetParser(target->createMCAsmParser(*subtargetInfo, *parser, *instrInfo, mcOptions));
	if (!targetParser)
		return "";
	parser->setTargetParser(*targetParser);
	if (parser->Run(false))
		return "";

	std::string bytesStr = getTextSectionBytes(llvm::MemoryBufferRef(objectData.str(), "instruction"));
	if (bytesStr.size() != 4)
		return "";

	uint32_t val = ((uint32_t)(unsigned char)bytesStr[0] << 24) |
	               ((uint32_t)(unsigned char)bytesStr[1] << 16) |
	               ((uint32_t)(unsigned char)bytesStr[2] << 8) |
	               (uint32_t)(unsigned char)bytesStr[3];

	return std::format("0x{:08X}", val);
}

std::string assembleInstructionToBytes(std::string_view instruction)
{
	// the same few instructions repeat thousands of times
	static std::map<std::string, std::string, std::less<>> s_encodedInstructions;
	if (auto cached = s_encodedInstructions.find(instruction); cached != s_encodedInstructions.end())
		return cached->second;
	std::string encoded = assembleInstructionToBytesUncached(instruction);
	if (!encoded.empty())
		s_encodedInstructions.emplace(instruction, encoded);
	return encoded;
}

#include "InstructionAssembler.h"
#include "common.h"

#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/CompilerInvocation.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"
#include "clang/CodeGen/CodeGenAction.h"
#include "clang/Basic/DiagnosticOptions.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Object/ObjectFile.h"
#include "llvm/Object/ELFObjectFile.h"
#include "llvm/Support/MemoryBuffer.h"
#include "clang/Frontend/Utils.h"

const char* forbiddenInstructions[] = {
	"MFCR",
	"MTCRF",
	"MCRF"
};

bool isForbiddenInstruction(const char* name)
{
	for (const auto& forbidden : forbiddenInstructions)
	{
		if (strcmp(name, forbidden) == 0)
			return true;
	}
	return false;
}

static std::string getSectionBytes(const std::string& objectFilePath) {
	auto bufferOrErr = llvm::MemoryBuffer::getFile(objectFilePath);
	if (!bufferOrErr) return "";

	auto objectOrErr = llvm::object::ObjectFile::createObjectFile(bufferOrErr.get()->getMemBufferRef());
	if (!objectOrErr) return "";

	llvm::object::ObjectFile* obj = objectOrErr.get().get();
	for (const auto& section : obj->sections()) {
		auto nameOrErr = section.getName();
		if (nameOrErr && nameOrErr.get() == ".text") {
			auto contentsOrErr = section.getContents();
			if (contentsOrErr) {
				llvm::StringRef contents = contentsOrErr.get();
				return std::string(contents.data(), contents.size());
			}
		}
	}
	return "";
}

std::string assembleInstructionToBytes(std::string_view instruction)
{
	std::string instructionStr(instruction);

	// Strip register prefixes (r and cr) to make the instruction compatible with Clang's integrated assembler
	instructionStr = std::regex_replace(instructionStr, std::regex(R"(\br([0-9]+)\b)"), "$1");
	instructionStr = std::regex_replace(instructionStr, std::regex(R"(\bcr([0-7])\b)"), "$1");

	std::error_code ec;

	// Write instruction to temp_assemble.cpp as global inline assembly
	std::string tempCppPath = "temp_assemble.cpp";
	std::string tempOPath = "temp_assemble.o";
	{
		std::ofstream out(tempCppPath, std::ios::binary);
		if (!out)
			return "";
		out << "asm(R\"(" << instructionStr << ")\");\n";
	}

	// Initialize targets
	static bool targetsInitialized = false;
	if (!targetsInitialized) {
		llvm::InitializeAllTargets();
		llvm::InitializeAllTargetMCs();
		llvm::InitializeAllAsmPrinters();
		llvm::InitializeAllAsmParsers();
		targetsInitialized = true;
	}

	// Compile using embedded Clang
	clang::CompilerInstance clang;
	clang::DiagnosticOptions *diagOpts = new clang::DiagnosticOptions();
	clang::IntrusiveRefCntPtr<clang::DiagnosticsEngine> diags =
		clang::CompilerInstance::createDiagnostics(diagOpts, new clang::TextDiagnosticPrinter(llvm::errs(), diagOpts));
	clang.setDiagnostics(diags.get());

	std::vector<const char*> args = {
		"clang",
		"-target", "powerpc-eabi",
		"-mcpu=750",
		"-m32",
		"-mbig-endian",
		"-c",
		tempCppPath.c_str(),
		"-o", tempOPath.c_str()
	};

	clang::CreateInvocationOptions opts;
	opts.Diags = diags;
	std::shared_ptr<clang::CompilerInvocation> invocation =
		clang::createInvocation(args, opts);
	if (!invocation) {
		std::filesystem::remove(tempCppPath, ec);
		return "";
	}
	invocation->getFrontendOpts().OutputFile = tempOPath;
	clang.setInvocation(invocation);

	std::unique_ptr<clang::FrontendAction> action = std::make_unique<clang::EmitObjAction>();
	if (!clang.ExecuteAction(*action)) {
		std::filesystem::remove(tempCppPath, ec);
		std::filesystem::remove(tempOPath, ec);
		return "";
	}

	// Read the compiled bytes from .text section
	std::string bytesStr = getSectionBytes(tempOPath);

	// Clean up
	std::filesystem::remove(tempCppPath, ec);
	std::filesystem::remove(tempOPath, ec);

	if (bytesStr.size() < 4) {
		return "";
	}

	uint32_t val = ((uint32_t)(unsigned char)bytesStr[0] << 24) |
	               ((uint32_t)(unsigned char)bytesStr[1] << 16) |
	               ((uint32_t)(unsigned char)bytesStr[2] << 8) |
	               (uint32_t)(unsigned char)bytesStr[3];

	return std::format("0x{:08X}", val);
}

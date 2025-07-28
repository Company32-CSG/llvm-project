#ifndef LLVM_CLANG_TOOLS_EXTRA_CLANGD_C32_DOXYGEN_COMPLETION_HPP
#define LLVM_CLANG_TOOLS_EXTRA_CLANGD_C32_DOXYGEN_COMPLETION_HPP
#include "../CodeComplete.h"
#include "../ParsedAST.h"
#include "Compiler.h"
#include "Preamble.h"
#include "support/Path.h"

#include <string_view>

namespace clang::clangd::c32::doxygen {

struct CodeCompleteArgs
{
	PathRef fileName;

	const ParseInputs& parseInput;

	const PreambleData* preamble;

	CodeCompleteOptions opts;

	SpeculativeFuzzyFind* specFuzzyFind;

	std::string_view contents;

	size_t offset;

	const ParsedAST* ast;
};

/**
 * @brief Offer code completion for Doxygen comments.
 *
 * @param[in] args
 * 		\r CodeCompleteArgs used internally
 *
 * @return \r CodeCompleteResult
 */
CodeCompleteResult completion(const CodeCompleteArgs& args);

/**
 * @brief Check if \p contents starting at \p cursorOffset is inside of a Doxygen comment.
 *
 * @param[in] contents
 * 		The contents being parsed.
 *
 * @param[in] cursorOffset
 * 		The user's current cursor offset inside of \p contents.
 *
 * @retval true
 * 		The \p cursorOffset inside of \p contents is a Doxygen comment.
 *
 * @retval false
 * 		The \p cursorOffset inside of \p contents is not a Doxygen comment.
 */
bool inDoxygenComment(std::string_view contents, size_t cursorOffset);

bool shouldRunCompletion(std::string_view contents, size_t cursorOffset, std::string_view triggerCharacter);

} // namespace clang::clangd::c32::doxygen

#endif

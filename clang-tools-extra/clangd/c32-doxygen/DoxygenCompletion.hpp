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
	/// Name of the file being completed
	PathRef fileName;

	/// Parsing inputs
	const ParseInputs& parseInput;

	/// Preamble data
	const PreambleData* preamble;

	/// Code completion options
	CodeCompleteOptions opts;

	/// Speculative fuzzy find instance
	SpeculativeFuzzyFind* specFuzzyFind;

	/// Contents being parsed
	std::string_view contents;

	/// Current cursor offset inside of \m contents
	size_t offset;

	/// Parsed AST for the file being completed
	const ParsedAST* ast;
};

/**
 * @brief
 * 		Offer code completion for Doxygen comments.
 *
 * @param[in] args
 * 		%CodeCompleteArgs used internally
 *
 * @returns
 * 		%CodeCompleteResult struct with completion results.
 */
CodeCompleteResult completion(const CodeCompleteArgs& args);

/**
 * @brief
 * 		Check if \p contents starting at \p cursorOffset is inside of a Doxygen comment.
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

/**
 * @brief
 * 		Determine if we should run Doxygen completion based on the trigger character and context.
 *
 * @param[in] contents
 * 		The contents being parsed.
 *
 * @param[in] cursorOffset
 * 		The user's current cursor offset inside of \p contents.
 *
 * @param[in] triggerCharacter
 * 		The character that triggered the completion.
 *
 * @retval true
 * 		Doxygen completion should be run.
 *
 * @retval false
 * 		Doxygen completion should not be run.
 */
bool shouldRunCompletion(std::string_view contents, size_t cursorOffset, std::string_view triggerCharacter);

} // namespace clang::clangd::c32::doxygen

#endif

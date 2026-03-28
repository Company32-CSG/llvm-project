#ifndef LLVM_CLANG_TOOLS_EXTRA_CLANGD_C32_DOXYGEN_COMPLETION_HPP
#define LLVM_CLANG_TOOLS_EXTRA_CLANGD_C32_DOXYGEN_COMPLETION_HPP
#include "../CodeComplete.h"
#include "../ParsedAST.h"
#include "Compiler.h"
#include "Preamble.h"
#include "support/Path.h"

#include <string_view>

namespace clang::clangd::c32::doxygen {

struct CodeCompleteArgs {
  /// Name of the file being completed
  PathRef FileName;

  /// Parsing inputs
  const ParseInputs &ParseInput;

  /// Preamble data
  const PreambleData *Preamble;

  /// Code completion options
  CodeCompleteOptions Opts;

  /// Speculative fuzzy find instance
  SpeculativeFuzzyFind *SpecFuzzyFind;

  /// Contents being parsed
  std::string_view Contents;

  /// Current cursor offset inside of \m Contents
  size_t Offset;

  /// Parsed AST for the file being completed
  const ParsedAST *AST;
};

/**
 * @brief
 * 		Offer code completion for Doxygen comments.
 *
 * @param[in] Args
 * 		%CodeCompleteArgs used internally
 *
 * @returns
 * 		%CodeCompleteResult struct with completion results.
 */
CodeCompleteResult completion(const CodeCompleteArgs &Args);

/**
 * @brief
 * 		Check if \p Contents starting at \p CursorOffset is inside of a
 * Doxygen comment.
 *
 * @param[in] Contents
 * 		The contents being parsed.
 *
 * @param[in] CursorOffset
 * 		The user's current cursor offset inside of \p Contents.
 *
 * @retval true
 * 		The \p CursorOffset inside of \p Contents is a Doxygen comment.
 *
 * @retval false
 * 		The \p CursorOffset inside of \p Contents is not a Doxygen
 * comment.
 */
bool inDoxygenComment(std::string_view Contents, size_t CursorOffset);

/**
 * @brief
 * 		Determine if we should run Doxygen completion based on the
 * trigger character and context.
 *
 * @param[in] Contents
 * 		The contents being parsed.
 *
 * @param[in] CursorOffset
 * 		The user's current cursor offset inside of \p Contents.
 *
 * @param[in] TriggerCharacter
 * 		The character that triggered the completion.
 *
 * @retval true
 * 		Doxygen completion should be run.
 *
 * @retval false
 * 		Doxygen completion should not be run.
 */
bool shouldRunCompletion(std::string_view Contents, size_t CursorOffset,
                         std::string_view TriggerCharacter);

} // namespace clang::clangd::c32::doxygen

#endif

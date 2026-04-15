#ifndef LLVM_CLANG_TOOLS_EXTRA_CLANGD_C32_DOCCAM_COMPLETION_HPP
#define LLVM_CLANG_TOOLS_EXTRA_CLANGD_C32_DOCCAM_COMPLETION_HPP
#include "CodeComplete.h"
#include "Compiler.h"
#include "ParsedAST.h"
#include "Preamble.h"
#include "support/Path.h"

#include <string_view>

namespace clang::clangd::c32::doccam {

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
 * 		Run code completion for Doccam comments if the context and
 * trigger character are appropriate.
 *
 * @param[in] FileName
 * 		The name of the file being completed.
 *
 * @param[in] ParseInput
 * 		The inputs used to parse the file being completed.
 *
 * @param[in] Preamble
 * 		The preamble data for the file being completed.
 *
 * @param[in] Opts
 * 		Code completion options.
 *
 * @param[in] SpecFuzzyFind
 * 		Speculative fuzzy find instance.
 *
 * @param[in] Content
 * 		The contents being parsed.
 *
 * @param[in] Offset
 * 		The current cursor offset inside of \p Content.
 *
 * @returns
 * 		%CodeCompleteResult struct with completion results.
 */
std::optional<CodeCompleteResult> maybeCompleteDoccamComment(
    PathRef FileName, const PreambleData *Preamble,
    const ParseInputs &ParseInput, CodeCompleteOptions Opts,
    SpeculativeFuzzyFind *SpecFuzzyFind, StringRef Content, size_t Offset);

/**
 * @brief
 * 		Check if \p Contents starting at \p CursorOffset is inside of a
 * Doccam comment.
 *
 * @param[in] Contents
 * 		The contents being parsed.
 *
 * @param[in] CursorOffset
 * 		The user's current cursor offset inside of \p Contents.
 *
 * @retval true
 * 		The \p CursorOffset inside of \p Contents is a Doccam comment.
 *
 * @retval false
 * 		The \p CursorOffset inside of \p Contents is not a Doccam
 * comment.
 */
bool inDoccamComment(std::string_view Contents, size_t CursorOffset);

/**
 * @brief
 * 		Determine if we should run Doccam completion based on the
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
 * 		Doccam completion should be run.
 *
 * @retval false
 * 		Doccam completion should not be run.
 */
bool shouldRunCompletion(std::string_view Contents, size_t CursorOffset,
                         std::string_view TriggerCharacter);
} // namespace clang::clangd::c32::doccam

#endif

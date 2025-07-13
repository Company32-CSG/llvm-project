#ifndef LLVM_CLANG_TOOLS_EXTRA_CLANGD_C32_DOXYGEN_HPP
#define LLVM_CLANG_TOOLS_EXTRA_CLANGD_C32_DOXYGEN_HPP
#include "CodeComplete.h"
#include "llvm/ADT/ArrayRef.h"

#include <cstddef>
#include <string_view>
#include <vector>

namespace clang::clangd::c32::doxygen {

struct DoxygenTag
{
	// 'param', 'brief', 'example', etc.
	std::string_view name;

	// For completion documentation.
	std::string_view description;

	// Other names for this tag. (e.g., @md -> @markdown)
	std::vector<std::string_view> aliases;

	// Tag attributes put in the [] such as @param[in], @example[c], etc.
	std::vector<std::string_view> attributes;
};

/**
 * @brief Get all supported Doxygen tags.
 *
 * @returns Constant array of all supported `DoxygenTag` structs.
 */
const llvm::ArrayRef<DoxygenTag> getAllTags();

/**
 * @brief Get all supported Doxygen tag initiators
 *
 * @returns Constant array of all supported Doxygen tag initiators (e.g., `[` `@` `]`)
 */
const llvm::ArrayRef<std::string_view> getAllTagInitiators();

/**
 * @brief Build a `CodeCompleteResult` completion result for Doxygen tags. This provides Doxygen tag
 * 		support for things like @md _intellisense_ in @md _vscode_.
 *
 * @param[in] contents
 * 		Content the LSP got from our editor. This is guaranteed to be within a Doxygen comment
 * 		based on how we parse from the LSP server side of things.
 *
 * @param[in] offset
 *		The user's cursor offset within the comment.
 *
 * @returns Fully built `CodeCompleteResult` ready to be sent back to the editor.
 */
CodeCompleteResult tagCompletion(std::string_view contents, size_t offset);

/**
 * @brief Check if `contents` starting at `cursorOffset` is inside of a Doxygen comment.
 *
 * @param[in] contents
 * 		The contents being parsed.
 *
 * @param[in] cursorOffset
 * 		The user's current cursor offset inside of `contents`.
 *
 * @retval true
 * 		The `cursorOffset` inside of `contents` is a Doxygen comment.
 *
 * @retval false
 * 		The `cursorOffset` inside of `contents` is not a Doxygen comment.
 */
bool inDoxygenComment(std::string_view contents, size_t cursorOffset);

/**
 * @brief Check if the first character in `contents` is a supported Doxygen tag initiator.
 *
 * @param[in] contents
 * 		String to check.
 *
 * @retval true
 * 		The first character of `contents` is a Doxygen tag initiator (e.g., `@`)
 *
 * @retval false
 * 		The first character of `contents` did not match any supported Doxygen tag initiators.
 */
bool isDoxygenTagInitiator(std::string_view contents);

} // namespace clang::clangd::c32::doxygen

#endif

#ifndef LLVM_CLANG_TOOLS_EXTRA_CLANGD_DOXYGEN_UTILS_HPP
#define LLVM_CLANG_TOOLS_EXTRA_CLANGD_DOXYGEN_UTILS_HPP
#include <string_view>

namespace clang::clangd::c32::doxygen {

/**
 * @brief Check if `contents` is a line that starts with `prefix`.
 *
 * @param[in] contents
 * 		Content to check. This function will trim any spaces, newlines, etc. before comparing.
 *
 * @param[in] offset
 * 		Starting position to check from.
 *
 * @param[in] prefix
 * 		Prefix to compare the beginning of `contents` to.
 *
 * @retval true
 * 		`contents` starts with `prefix`
 *
 * @retval false
 * 		`contents` doesn't start with `prefix`
 */
bool lineStartsWith(std::string_view contents, size_t offset, std::string_view prefix);

/**
 * @brief Extract from `contents` until the first new line starting from `offset`.
 *
 * @param[in] contents
 * 		Contents to extract a line from.
 *
 * @param[in] offset
 * 		Starting position to extract from.
 *
 * @returns Pair containing the position where the new line was found and the content
 * 		extracted from `contents` backward to the newline.
 */
std::pair<size_t, std::string_view> extractLine(std::string_view contents, size_t offset);

} // namespace clang::clangd::c32::doxygen

#endif

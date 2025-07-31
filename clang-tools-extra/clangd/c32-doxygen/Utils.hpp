#ifndef LLVM_CLANG_TOOLS_EXTRA_CLANGD_C32_DOXYGEN_UTILS_HPP
#define LLVM_CLANG_TOOLS_EXTRA_CLANGD_C32_DOXYGEN_UTILS_HPP
#include <string_view>
#include <vector>

namespace clang::clangd::c32 {

/**
 * @brief Check if `contents` is a line that starts with `prefix`.
 *
 * @param[in] contents
 * 		Content to check. This function will trim any spaces, newlines, etc. before comparing.
 *
 * @param[in] cursorOffset
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
bool lineStartsWith(std::string_view contents, size_t cursorOffset, std::string_view prefix);

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

std::string indentLines(std::string_view input);

std::string canonicalizeWhitespace(std::string_view contents, bool preserveNewlines = false);

std::string_view::size_type findFirstSpace(std::string_view contents);

/**
 * @brief Trim leading whitespace. This function uses `std::isspace` under the hood.
 *
 * @param[in] contents
 * 		Contents to trim.
 *
 * @returns Copy of `contents` with leading whitespace trimmed.
 */
std::string ltrim(std::string_view contents);

/**
 * @brief Trim trailing whitespace. This function uses `std::isspace` under the hood.
 *
 * @param[in] contents
 * 		Contents to trim.
 *
 * @returns Copy of `contents` with trailing whitespace trimmed.
 */
std::string rtrim(std::string_view contents);

/**
 * @brief Trim both leading and trailing whitespace. This function uses `std::isspace` under the hood.
 *
 * @param[in] contents
 * 		Contents to trim.
 *
 * @returns Copy of `contents` with leading and trailing whitespace trimmed.
 */
std::string trim(std::string_view contents);

std::vector<std::string> split(std::string_view contents, std::string_view splitter);

std::string lowercase(std::string_view contents);
std::string uppercase(std::string_view contents);
std::string properNounCase(std::string_view contents);

} // namespace clang::clangd::c32

#endif

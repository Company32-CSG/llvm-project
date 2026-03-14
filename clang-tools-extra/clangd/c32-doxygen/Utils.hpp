#ifndef LLVM_CLANG_TOOLS_EXTRA_CLANGD_C32_DOXYGEN_UTILS_HPP
#define LLVM_CLANG_TOOLS_EXTRA_CLANGD_C32_DOXYGEN_UTILS_HPP
#include <string_view>
#include <vector>

namespace clang::clangd::c32 {

/**
 * @brief
 * 		Check if \p contents is a line that starts with \p prefix.
 *
 * @param[in] contents
 * 		Content to check. This function will trim any spaces, newlines, etc. before comparing.
 *
 * @param[in] cursorOffset
 * 		Starting position to check from.
 *
 * @param[in] prefix
 * 		Prefix to compare the beginning of \p contents to.
 *
 * @retval true
 * 		\p contents starts with \p prefix
 *
 * @retval false
 * 		\p contents doesn't start with \p prefix
 */
bool lineStartsWith(std::string_view contents, size_t cursorOffset, std::string_view prefix);

/**
 * @brief
 * 		Extract from \p contents until the first new line starting from \p offset.
 *
 * @param[in] contents
 * 		Contents to extract a line from.
 *
 * @param[in] offset
 * 		Starting position to extract from.
 *
 * @returns
 * 		Pair containing the position where the new line was found and the content
 * 		extracted from \p contents backward to the newline.
 */
std::pair<size_t, std::string_view> extractLine(std::string_view contents, size_t offset);

/**
 * @brief
 * 		Indent lines in the given input.
 *
 * @param[in] input
 * 		Input string to indent.
 *
 * @returns
 * 		Indented string.
 */
std::string indentLines(std::string_view input);

/**
 * @brief
 * 		Canonicalize whitespace in the given contents.
 *
 * @param[in] contents
 * 		Contents to canonicalize whitespace in.
 *
 * @param[in] preserveNewlines
 * 		Whether to preserve newlines or not.
 *
 * @returns
 * 		String with canonicalized whitespace.
 */
std::string canonicalizeWhitespace(std::string_view contents, bool preserveNewlines = false);

/**
 * @brief
 * 		Find the first space in the given contents.
 *
 * @param[in] contents
 * 		Contents to search for the first space.
 *
 * @returns
 * 		Position of the first space in \p contents, or %std::string_view::npos if no space was found.
 */
std::string_view::size_type findFirstSpace(std::string_view contents);

/**
 * @brief
 * 		Trim leading whitespace. This function uses %std::isspace under the hood.
 *
 * @param[in] contents
 * 		Contents to trim.
 *
 * @returns
 * 		Copy of \p contents with leading whitespace trimmed.
 */
std::string ltrim(std::string_view contents);

/**
 * @brief
 * 		Trim trailing whitespace. This function uses %std::isspace under the hood.
 *
 * @param[in] contents
 * 		Contents to trim.
 *
 * @returns
 * 		Copy of \p contents with trailing whitespace trimmed.
 */
std::string rtrim(std::string_view contents);

/**
 * @brief
 * 		Trim both leading and trailing whitespace. This function uses %std::isspace under the hood.
 *
 * @param[in] contents
 * 		Contents to trim.
 *
 * @returns
 * 		Copy of \p contents with leading and trailing whitespace trimmed.
 */
std::string trim(std::string_view contents);

/**
 * @brief
 * 		Split \p contents into a vector of strings using \p splitter as the delimiter.
 *
 * @param[in] contents
 * 		Contents to split.
 *
 * @param[in] splitter
 * 		Delimiter to use for splitting \p contents.
 *
 * @returns
 * 		Vector of strings resulting from splitting \p contents by \p splitter.
 */
std::vector<std::string> split(std::string_view contents, std::string_view splitter);

/**
 * @brief
 * 		Convert \p contents to lowercase.
 *
 * @param[in] contents
 * 		Contents to convert to lowercase.
 *
 * @returns
 * 		Copy of \p contents converted to lowercase.
 */
std::string lowercase(std::string_view contents);

/**
 * @brief
 * 		Convert \p contents to uppercase.
 *
 * @param[in] contents
 * 		Contents to convert to uppercase.
 *
 * @returns
 * 		Copy of \p contents converted to uppercase.
 */
std::string uppercase(std::string_view contents);

/**
 * @brief
 * 		Convert \p contents to proper noun case (first letter uppercase, rest lowercase).
 *
 * @param[in] contents
 * 		Contents to convert to proper noun case.
 *
 * @returns
 * 		Copy of \p contents converted to proper noun case.
 */
std::string properNounCase(std::string_view contents);

} // namespace clang::clangd::c32

#endif

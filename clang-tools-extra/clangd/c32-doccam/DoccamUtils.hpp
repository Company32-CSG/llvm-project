#ifndef LLVM_CLANG_TOOLS_EXTRA_CLANGD_C32_DOCCAM_UTILS_HPP
#define LLVM_CLANG_TOOLS_EXTRA_CLANGD_C32_DOCCAM_UTILS_HPP
#include "clang/Format/Format.h"

#include <string_view>
#include <vector>

namespace clang::clangd::c32::doccam {

/**
 * @brief
 * 		Format the given code using either the given style or the
 * default hover style based on the current configuration of
 * `Config::current().C32.Doccam.Hover.UseWorkspaceFormattingStyle`
 *
 * @param[in] Style
 * 		Style to format the code with.
 *
 * @param[in] Input
 * 		Code to format.
 *
 * @returns
 * 		Formatted code, or the original code if formatting failed.
 */
std::string formatCode(const format::FormatStyle &Style,
                       std::string_view Input);

/**
 * @brief
 * 		Escape HTML special characters in the given input string.
 *
 * @param[in] Input
 * 		String to escape HTML characters in.
 *
 * @returns
 * 		Copy of \p Input with HTML special characters escaped.
 */
std::string escapeHtml(std::string_view Input);

/**
 * @brief
 * 		Check if \p Contents is a line that starts with \p Prefix.
 *
 * @param[in] Contents
 * 		Content to check. This function will trim any spaces, newlines,
 * etc. before comparing.
 *
 * @param[in] CursorOffset
 * 		Starting position to check from.
 *
 * @param[in] Prefix
 * 		Prefix to compare the beginning of \p Contents to.
 *
 * @retval true
 * 		\p Contents starts with \p Prefix
 *
 * @retval false
 * 		\p Contents doesn't start with \p Prefix
 */
bool lineStartsWith(std::string_view Contents, size_t CursorOffset,
                    std::string_view Prefix);

/**
 * @brief
 * 		Extract from \p Contents until the first new line starting from
 * \p Offset.
 *
 * @param[in] Contents
 * 		Contents to extract a line from.
 *
 * @param[in] Offset
 * 		Starting position to extract from.
 *
 * @returns
 * 		Pair containing the position where the new line was found and
 * the content extracted from \p Contents backward to the newline.
 */
std::pair<size_t, std::string_view> extractLine(std::string_view Contents,
                                                size_t Offset);

/**
 * @brief
 * 		Indent lines in the given input.
 *
 * @param[in] Input
 * 		Input string to indent.
 *
 * @returns
 * 		Indented string.
 */
std::string indentLines(std::string_view Input);

/**
 * @brief
 * 		Canonicalize whitespace in the given contents.
 *
 * @param[in] Contents
 * 		Contents to canonicalize whitespace in.
 *
 * @param[in] PreserveNewlines
 * 		Whether to preserve newlines or not.
 *
 * @returns
 * 		String with canonicalized whitespace.
 */
std::string canonicalizeWhitespace(std::string_view Contents,
                                   bool PreserveNewlines = false);

/**
 * @brief
 * 		Find the first space in the given contents.
 *
 * @param[in] Contents
 * 		Contents to search for the first space.
 *
 * @returns
 * 		Position of the first space in \p Contents, or
 * %std::string_view::npos if no space was found.
 */
std::string_view::size_type findFirstSpace(std::string_view Contents);

/**
 * @brief
 * 		Trim leading whitespace. This function uses %std::isspace under
 * the hood.
 *
 * @param[in] Contents
 * 		Contents to trim.
 *
 * @returns
 * 		Copy of \p Contents with leading whitespace trimmed.
 */
std::string ltrim(std::string_view Contents);

/**
 * @brief
 * 		Trim trailing whitespace. This function uses %std::isspace under
 * the hood.
 *
 * @param[in] Contents
 * 		Contents to trim.
 *
 * @returns
 * 		Copy of \p Contents with trailing whitespace trimmed.
 */
std::string rtrim(std::string_view Contents);

/**
 * @brief
 * 		Trim both leading and trailing whitespace. This function uses
 * %std::isspace under the hood.
 *
 * @param[in] Contents
 * 		Contents to trim.
 *
 * @returns
 * 		Copy of \p Contents with leading and trailing whitespace
 * trimmed.
 */
std::string trim(std::string_view Contents);

/**
 * @brief
 * 		Split \p Contents into a vector of strings using \p Splitter as
 * the delimiter.
 *
 * @param[in] Contents
 * 		Contents to split.
 *
 * @param[in] Splitter
 * 		Delimiter to use for splitting \p Contents.
 *
 * @returns
 * 		Vector of strings resulting from splitting \p Contents by \p
 * Splitter.
 */
std::vector<std::string> split(std::string_view Contents,
                               std::string_view Splitter);

/**
 * @brief
 * 		Convert \p Contents to lowercase.
 *
 * @param[in] Contents
 * 		Contents to convert to lowercase.
 *
 * @returns
 * 		Copy of \p Contents converted to lowercase.
 */
std::string lowercase(std::string_view Contents);

/**
 * @brief
 * 		Convert \p Contents to uppercase.
 *
 * @param[in] Contents
 * 		Contents to convert to uppercase.
 *
 * @returns
 * 		Copy of \p Contents converted to uppercase.
 */
std::string uppercase(std::string_view Contents);

/**
 * @brief
 * 		Convert \p Contents to proper noun case (first letter uppercase,
 * rest lowercase).
 *
 * @param[in] Contents
 * 		Contents to convert to proper noun case.
 *
 * @returns
 * 		Copy of \p Contents converted to proper noun case.
 */
std::string properNounCase(std::string_view Contents);

} // namespace clang::clangd::c32::doccam

#endif

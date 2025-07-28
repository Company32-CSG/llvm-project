#ifndef LLVM_CLANG_TOOLS_EXTRA_CLANGD_C32_DOXYGEN_HPP
#define LLVM_CLANG_TOOLS_EXTRA_CLANGD_C32_DOXYGEN_HPP
#include "../CodeComplete.h"

#include "llvm/ADT/ArrayRef.h"

#include <cstddef>
#include <string_view>
#include <vector>

namespace clang::clangd::c32::doxygen {

enum class TagType
{
	/// Concrete type for `^@brief`
	Brief,

	/// Concrete type for `^@deprecated`
	Deprecated,

	/// Concrete type for `^@example`, `^@usage`
	Example,

	/// Concrete type for `^@member`, `^@m`
	Member,

	/// Concrete type for `^@p`
	P,

	/// Concrete type for `^@param`
	Param,

	/// Concrete type for `^@ref`
	Ref,

	/// Concrete type for `^@returns`, `^@return`
	Returns,

	/// Concrete type for `^@retval`, `^@ret`
	Retval,

	/// Concrete type for `^@warning`, `^@ret`
	Warning,

	/// Generic doxygen tag with no special handling
	Custom
};

enum class TagContext
{
	/// Any type of tag
	Any,

	/// Block tags only
	Block,

	/// Inline tags only
	Inline
};

struct DoxygenTag
{
	/// Concrete `TagType` identifier for this tag
	TagType type;

	/// Denotes whether the tag is meant to be parsed inline rather than its own section
	bool isInline;

	/// 'brief', 'example', 'param', etc.
	std::string_view name;

	/// For completion documentation.
	std::string_view description;

	/// Other names for this tag. (e.g., `^@ret` -> `^@retval`)
	std::vector<std::string_view> aliases;

	/// Tag attributes put in the `[]` such as `^@param[in]`, `^@example[c]`, etc.
	std::vector<std::string_view> attributes;
};

/**
 * @brief Get all supported Doxygen tags.
 *
 * @returns Constant array of all supported `DoxygenTag` structs.
 */
const llvm::ArrayRef<DoxygenTag> getAllTags();

/**
 * @brief Return a list of all supported Doxygen tag names including aliases.
 *
 * @returns Constant array of all supported Doxygen tag names and their aliases.
 */
const std::vector<std::string_view> getAllTagNames();

/**
 * @brief Get all supported Doxygen tag initiators
 *
 * @returns Constant array of all supported Doxygen tag initiators (e.g., `[` `^@` `]`)
 */
const llvm::ArrayRef<char> getAllTagInitiators();

// /**
//  * @brief Build a `CodeCompleteResult` completion result for Doxygen tags. This provides Doxygen tag
//  * 		support for things like ___intellisense___ in ___vscode___.
//  *
//  * @param[in] contents
//  * 		Content the LSP got from our editor. This is guaranteed to be within a Doxygen comment
//  * 		based on how we parse from the LSP server side of things.
//  *
//  * @param[in] offset
//  *		The user's cursor offset within the comment.
//  *
//  * @returns Fully built `CodeCompleteResult` ready to be sent back to the editor.
//  */
// CodeCompleteResult tagCompletion(std::string_view contents, size_t offset);

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

/**
 * @brief Check if the character `c` is a supported Doxygen tag initiator.
 *
 * @param[in] c
 * 		Character to check.
 *
 * @retval true
 * 		The character `c` is a Doxygen tag initiator (e.g., `@`)
 *
 * @retval false
 * 		The character `c` did not match any supported Doxygen tag initiators.
 */
bool isDoxygenTagInitiator(char c);

const DoxygenTag* getDoxygenTagByName(std::string_view name);

std::pair<size_t, char> findClosestTagInitiator(std::string_view contents);

// std::pair<size_t, char> rfindClosestTagInitiator(std::string_view contents, size_t offset);

/**
 * @brief Checks if the character at `offset` has been escaped.
 *
 * @param[in] contents
 * 		Contents to check if the character before `offset` is escaped.
 *
 * @param[in] offset
 * 		Offset into `contents` to check.
 *
 * @retval true
 * 		The character at `offset` is being escaped.
 *
 * @retval false
 * 		The character at `offset` is ___not___ being escaped.
 *
 * @example[c] {
 *		for (size_t i = 0U; i < sv.size(); i++)
 *		{
 *			for (const auto ini : getAllTagInitiators())
 *			{
 *				if (isEscaping(sv, i))
 *					continue;
 *
 *				if (sv[i] == ini)
 *					return i;
 *			}
 *		}
 * }
 */
bool isEscaping(std::string_view contents, size_t offset);

/**
 * @brief Check if the character `c` is an escape character.
 *
 * @param[in] c
 * 		Character to compare against the Doxygen escape character.
 *
 * @returns Whether `c` is an escape character
 */
bool isDoxygenEscape(const char c);

std::string unescape(std::string_view sv);

bool tagStartsLine(std::string_view sv, size_t pos);

struct MatchedTag
{
	/// Tag initiator type that was found
	char initiator;

	/// Tag name that was matched to
	std::string name;

	/// Number of bytes consumed of the string that was passed in
	size_t consumed;

	/// Reference to the tag that was found
	const DoxygenTag* tag;
};

std::optional<MatchedTag> getTag(std::string_view sv, size_t pos, TagContext context);

bool isTagTerminator(char c);

} // namespace clang::clangd::c32::doxygen

#endif

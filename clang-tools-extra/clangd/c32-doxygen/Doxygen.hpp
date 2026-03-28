#ifndef LLVM_CLANG_TOOLS_EXTRA_CLANGD_C32_DOXYGEN_HPP
#define LLVM_CLANG_TOOLS_EXTRA_CLANGD_C32_DOXYGEN_HPP
#include "llvm/ADT/ArrayRef.h"

#include <cstddef>
#include <string_view>
#include <vector>

namespace clang::clangd::c32::doxygen {

enum class TagType {
  /// Concrete type for `^@a`
  A,

  /// Concrete type for `^@b`
  B,

  /// Concrete type for `^@brief`
  Brief,

  /// Concrete type for `^@c`
  C,

  /// Concrete type for `^@code` ... `^@end`
  Code,

  /// Concrete type for `^@end`
  End,

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

  /// Concrete type for `^@throw`, `^@throws`
  Throw,

  /// Concrete type for `^@tparam`
  TParam,

  /// Concrete type for `^@version`
  Version,

  /// Concrete type for `^@warning`, `^@warn`
  Warning,

  /// Generic doxygen tag with no special handling
  Custom
};

enum class TagContext {
  /// Any type of tag
  Any,

  /// Block tags only
  Block,

  /// Inline tags only
  Inline
};

struct TagParsingFlags {
  /// Denotes whether the tag is meant to be parsed inline rather than its own
  /// section.
  bool Inline : 1U;

  /// The parser will continue to consume when line breaks (blank lines) are
  /// encountered.
  bool AllowLineBreaks : 1U;

  /// Indicates the parser should consume the following (terminating) tag too.
  bool HasTerminatingTag : 1U;

  TagParsingFlags(bool Inline = false, bool AllowLineBreaks = false,
                  bool HasTerminatingTag = false)
      : Inline(Inline), AllowLineBreaks(AllowLineBreaks),
        HasTerminatingTag(HasTerminatingTag) {}
};

struct DoxygenTag {
  /// Concrete \r TagType identifier for this tag
  TagType Type;

  /// How to parse this tag
  TagParsingFlags Flags;

  /// `brief`, `example`, `param`, etc.
  std::string_view Name;

  /// For completion documentation.
  std::string_view Description;

  /// Other names for this tag. (e.g., `^@ret` -> `^@retval`)
  std::vector<std::string_view> Aliases;

  /// Tag attributes put in the `[]` such as `^@param[in]`, `^@example[c]`, etc.
  std::vector<std::string_view> Attributes;
};

/**
 * @brief
 * 		Get all supported Doxygen tags.
 *
 * @returns
 * 		Constant array of all supported \r DoxygenTag structs.
 */
const llvm::ArrayRef<DoxygenTag> getAllTags();

/**
 * @brief
 * 		Return a list of all supported Doxygen tag names including
 * aliases.
 *
 * @returns
 * 		Constant array of all supported Doxygen tag names and their
 * aliases.
 */
const std::vector<std::string_view> getAllTagNames();

/**
 * @brief
 * 		Get all supported Doxygen tag initiators
 *
 * @returns
 * 		Constant array of all supported Doxygen tag initiators (e.g.,
 * `[` `^@` `]`)
 */
const llvm::ArrayRef<char> getAllTagInitiators();

/**
 * @brief
 * 		Get a Doxygen tag by its concrete \r TagType.
 *
 * @param[in] Type
 * 		Concrete \r TagType to search for.
 *
 * @returns
 * 		Pointer to the \r DoxygenTag that matches \p Type, or nullptr
 */
const DoxygenTag *getTagByType(TagType Type);

/**
 * @brief
 * 		Check if \p contents starting at \p cursorOffset is inside of a
 * Doxygen comment.
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
 * 		The \p cursorOffset inside of \p contents is not a Doxygen
 * comment.
 */
bool inDoxygenComment(std::string_view Contents, size_t CursorOffset);

/**
 * @brief
 * 		Check if the first character in \p contents is a supported
 * Doxygen tag initiator.
 *
 * @param[in] contents
 * 		String to check.
 *
 * @retval true
 * 		The first character of \p contents is a Doxygen tag initiator
 * (e.g., `@`)
 *
 * @retval false
 * 		The first character of \p contents did not match any supported
 * Doxygen tag initiators.
 */
bool isDoxygenTagInitiator(std::string_view Contents);

/**
 * @brief
 * 		Check if the character \p C is a supported Doxygen tag
 * initiator.
 *
 * @param[in] C
 * 		Character to check.
 *
 * @retval true
 * 		The character \p C is a Doxygen tag initiator (e.g., `@`)
 *
 * @retval false
 * 		The character \p C did not match any supported Doxygen tag
 * initiators.
 */
bool isDoxygenTagInitiator(char C);

/**
 * @brief
 * 		Get a Doxygen tag by its name or alias.
 *
 * @param[in] Name
 * 		Name or alias of the Doxygen tag to search for.
 *
 * @returns
 * 		Pointer to the \r DoxygenTag that matches \p Name, or nullptr
 */
const DoxygenTag *getDoxygenTagByName(std::string_view Name);

/**
 * @brief
 * 		Find the closest Doxygen tag initiator in \p Contents.
 *
 * @param[in] Contents
 * 		Contents to search for the closest Doxygen tag initiator.
 *
 * @returns
 * 		Pair containing the offset of the closest Doxygen tag initiator
 * and the initiator character.
 */
std::pair<size_t, char> findClosestTagInitiator(std::string_view Contents);

// std::pair<size_t, char> rfindClosestTagInitiator(std::string_view contents,
// size_t offset);

/**
 * @brief
 * 		Checks if the character at \p Offset has been escaped.
 *
 * @param[in] Contents
 * 		Contents to check if the character before \p Offset is escaped.
 *
 * @param[in] Offset
 * 		Offset into \p Contents to check.
 *
 * @retval true
 * 		The character at \p Offset is being escaped.
 *
 * @retval false
 * 		The character at \p Offset is \a not being escaped.
 *
 * @example[c]
 *		for (size_t I = 0U; I < sv.size(); I++)
 *		{
 *			for (const auto ini : getAllTagInitiators())
 *			{
 *				if (isEscaping(sv, I))
 *					continue;
 *
 *				if (sv[I] == ini)
 *					return I;
 *			}
 *		}
 * @end
 */
bool isEscaping(std::string_view Contents, size_t Offset);

/**
 * @brief
 * 		Check if the character \p C is an escape character.
 *
 * @param[in] C
 * 		Character to compare against the Doxygen escape character.
 *
 * @retval true
 * 		The character \p C is a Doxygen escape character.
 *
 * @retval false
 * 		The character \p C is not a Doxygen escape character.
 */
bool isDoxygenEscape(const char C);

/**
 * @brief
 * 		Unescape a string by removing Doxygen escape characters.
 *
 * @param[in] SV
 * 		String view to unescape.
 *
 * @returns
 * 		Unescaped string.
 */
std::string unescape(std::string_view SV);

/**
 * @brief
 * 		Check if a Doxygen tag at \p Pos in \p SV starts a new line.
 *
 * @param[in] SV
 * 		String view to check.
 *
 * @param[in] Pos
 * 		Position of the tag in \p SV.
 *
 * @retval true
 * 		The tag at \p Pos starts a new line.
 *
 * @retval false
 * 		The tag at \p Pos does not start a new line.
 */
bool tagStartsLine(std::string_view SV, size_t Pos);

struct MatchedTag {
  /// Tag initiator type that was found
  char INI;

  /// Tag name that was matched to
  std::string Name;

  /// Number of bytes consumed of the string that was passed in
  size_t Consumed;

  /// Reference to the tag that was found
  const DoxygenTag *Tag;
};

/**
 * @brief
 * 		Attempt to match a Doxygen tag at \p Pos in \p SV.
 *
 * @param[in] SV
 * 		String view to check.
 *
 * @param[in] Pos
 * 		Position in \p SV to check for a Doxygen tag.
 *
 * @param[in] Context
 * 		Tag context to limit the search to (e.g., Block tags only).
 *
 * @returns
 * 		\r MatchedTag struct if a tag was found, or %std::nullopt if no
 * tag was found.
 */
std::optional<MatchedTag> getTag(std::string_view SV, size_t Pos,
                                 TagContext Context);

/**
 * @brief
 * 		Check if character \p C is a valid tag terminator.
 *
 * @param[in] C
 * 		Character to check.
 *
 * @param[in] IsInline
 * 		Whether the tag being checked is an inline tag.
 *
 * @retval true
 * 		\p C is a valid tag terminator.
 *
 * @retval false
 * 		\p C is not a valid tag terminator.
 */
bool isTagTerminator(char C, bool IsInline);

} // namespace clang::clangd::c32::doxygen

#endif

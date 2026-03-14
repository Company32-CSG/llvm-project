#ifndef LLVM_CLANG_TOOLS_EXTRA_CLANGD_C32_DOXYGEN_PARSER_HPP
#define LLVM_CLANG_TOOLS_EXTRA_CLANGD_C32_DOXYGEN_PARSER_HPP
#include "clang/Format/Format.h"

#include <vector>

namespace clang::clangd::c32::doxygen {

struct ParameterTag
{
	enum class Specifier : uint8_t
	{
		/// No specifier
		None = (0),

		/// `[in]`
		In = (1 << 0),

		/// `[out]`
		Out = (1 << 1),

		/// `[in:opt]`, `[out:optional]`, etc.
		Optional = (1 << 2)
	};

	/// Parameter name
	std::string name;

	/// Description of the parameter
	std::string description;

	/// Data access specifiers (e.g., `[in]`, `[out]`, `[in:opt]`, etc.)
	Specifier specifiers;

	/// Resolved data type, set when a symbol matching this parameter is found for \m name
	std::optional<std::string> type;

	/// Resolved underlying data type, set when a symbol matching this parameter is found and has an underlying type
	std::optional<std::string> typeAka;
};

struct CodeExampleTag
{
	/// Name of the opening tag (e.g., "example", "code", etc.) since multiple tags map to this struct.
	std::string name;

	/// Programming language of the code example
	std::string lang;

	/// Code content of the example
	std::string code;
};

struct CustomTag
{
	/// Name of the custom tag
	std::string name;

	/// Body content of the custom tag
	std::string body;
};

struct ParsedDoxygen
{
	/// Parsed from `^@brief`
	std::string brief;

	/// Parsed from `^@warning`
	std::vector<std::string> warnings;

	/// Parsed from `^@deprecated`
	std::string deprecated;

	std::vector<std::string> untaggedLines;

	std::vector<CustomTag> customTags;

	/// Parsed from `^@param`
	std::vector<ParameterTag> parameters;

	/// Parsed from `^@tparam`
	std::map<std::string, std::string> tparams;

	/// Parsed from `^@returns`
	std::string returns;

	/// Parsed from `^@retval`
	std::map<std::string, std::string> retvals;

	/// Parsed from `^@throw`
	std::map<std::string, std::string> throws;

	/// Parsed from `^@version`
	std::pair<std::string, std::string> version;

	/// Parsed from `^@example` and `^@code`
	std::vector<CodeExampleTag> codeExamples;
};

/**
 * @brief
 * 		Parse Doxygen comments from \p contents.
 *
 * @param[in] contents
 * 		Contents to parse Doxygen comments from.
 *
 * @param[in] style
 * 		Format style to use when parsing (for indentation, etc.)
 *
 * @returns
 * 		%ParsedDoxygen struct with parsed Doxygen comments.
 */
ParsedDoxygen parse(std::string_view contents, const format::FormatStyle& style);

// MARK: - Specifier Operators

/**
 * @brief
 * 		Bitwise OR operator (`|`) for %ParameterTag::Specifier enum.
 *
 * @param[in] lhs
 * 		Left-hand side specifier.
 *
 * @param[in] rhs
 * 		Right-hand side specifier.
 *
 * @returns
 * 		Result of the bitwise OR operation.
 */
inline ParameterTag::Specifier
operator|(ParameterTag::Specifier lhs, ParameterTag::Specifier rhs)
{
	return static_cast<ParameterTag::Specifier>(static_cast<uint8_t>(lhs) | static_cast<uint8_t>(rhs));
}

/**
 * @brief
 * 		Bitwise OR assignment operator (`|=`) for %ParameterTag::Specifier enum.
 *
 * @param[in,out] lhs
 * 		Left-hand side specifier to be modified.
 *
 * @param[in] rhs
 * 		Right-hand side specifier.
 *
 * @returns
 * 		Modified left-hand side specifier after the bitwise OR operation.
 */
inline ParameterTag::Specifier&
operator|=(ParameterTag::Specifier& lhs, ParameterTag::Specifier rhs)
{
	lhs = rhs | lhs;

	return lhs;
}

/**
 * @brief
 * 		Bitwise AND operator (`&`) for %ParameterTag::Specifier enum.
 *
 * @param[in] lhs
 * 		Left-hand side specifier.
 *
 * @param[in] rhs
 * 		Right-hand side specifier.
 *
 * @returns
 * 		Result of the bitwise AND operation.
 */
inline ParameterTag::Specifier
operator&(ParameterTag::Specifier lhs, ParameterTag::Specifier rhs)
{
	return static_cast<ParameterTag::Specifier>(static_cast<uint8_t>(lhs) & static_cast<uint8_t>(rhs));
}

} // namespace clang::clangd::c32::doxygen

#endif

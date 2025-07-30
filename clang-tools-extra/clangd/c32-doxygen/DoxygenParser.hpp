#ifndef LLVM_CLANG_TOOLS_EXTRA_CLANGD_C32_DOXYGEN_PARSER_HPP
#define LLVM_CLANG_TOOLS_EXTRA_CLANGD_C32_DOXYGEN_PARSER_HPP
#include "../Hover.h"

#include <vector>

namespace clang::clangd::c32::doxygen {

struct ParameterTag
{
	enum class Specifier : uint8_t
	{
		/// No specifier
		None = (0),

		/// [in]
		In = (1 << 0),

		/// [out]
		Out = (1 << 1),

		/// [in:optional], [out:optional]
		Optional = (1 << 2)
	};

	/// Parameter name
	std::string name;

	/// Description of the parameter
	std::string description;

	/// Data access specifiers (e.g., `[in]`, `[out]`, `[in:optional]`, etc.)
	Specifier specifiers;

	/// Resolved data type, set when a symbol matching this parameter is found
	std::optional<std::string> type;

	/// Resolved underlying data type, set when a symbol matching this parameter is found and has an underlying type
	std::optional<std::string> typeAka;
};

struct CodeTag
{
	std::string lang;
	std::string code;
};

struct ExampleTag
{
	std::string lang;
	std::string code;
};

struct CustomTag
{
	std::string name;
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

	/// Parsed from `^@example`
	std::vector<ExampleTag> examples;

	/// Parsed from `^@code` ... `^@endcode`
	std::vector<CodeTag> codeBlocks;
};

ParsedDoxygen parse(const HoverInfo& info);

// MARK: - Specifier Operators

inline ParameterTag::Specifier
operator|(ParameterTag::Specifier lhs, ParameterTag::Specifier rhs)
{
	return static_cast<ParameterTag::Specifier>(static_cast<uint8_t>(lhs) | static_cast<uint8_t>(rhs));
}

inline ParameterTag::Specifier&
operator|=(ParameterTag::Specifier& lhs, ParameterTag::Specifier rhs)
{
	lhs = rhs | lhs;

	return lhs;
}

inline ParameterTag::Specifier
operator&(ParameterTag::Specifier lhs, ParameterTag::Specifier rhs)
{
	return static_cast<ParameterTag::Specifier>(static_cast<uint8_t>(lhs) & static_cast<uint8_t>(rhs));
}

} // namespace clang::clangd::c32::doxygen

#endif

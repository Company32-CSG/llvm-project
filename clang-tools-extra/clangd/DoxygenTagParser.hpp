#ifndef LLVM_CLANG_TOOLS_EXTRA_CLANGD_DOXY_TAG_PARSER_H
#define LLVM_CLANG_TOOLS_EXTRA_CLANGD_DOXY_TAG_PARSER_H

#include "Hover.h"
#include "support/Markup.h"
#include "llvm/ADT/StringRef.h"

#include <cstdint>
#include <string>
#include <vector>

namespace clang::clangd::c32 {

// MARK: - ParamSpecifier

enum class ParamSpecifier : uint8_t
{
	/// No specifier
	NONE = (0),

	/// [in]
	IN = (1 << 0),

	/// [out]
	OUT = (1 << 1),

	/// [in:optional], [out:optional]
	OPTIONAL = (1 << 2)
};

/**
 * @brief Logically OR two `ParamSpecifier` enum values together
 *
 * @param[in] lhs
 * 		Left hand side `ParamSpecifier`
 *
 * @param[in] rhs
 * 		Right hand side `ParamSpecifier`
 *
 * @return `ParamSpecifier`
 */
inline ParamSpecifier
operator|(ParamSpecifier lhs, ParamSpecifier rhs)
{
	return static_cast<ParamSpecifier>(static_cast<uint8_t>(lhs) | static_cast<uint8_t>(rhs));
}

inline ParamSpecifier&
operator|=(ParamSpecifier& lhs, ParamSpecifier rhs)
{
	lhs = rhs | lhs;

	return lhs;
}

/**
 * @brief Logically AND two `ParamSpecifier` enum values together
 *
 * @param[in] lhs
 * 		Left hand side `ParamSpecifier`
 *
 * @param[in] rhs
 * 		Right hand side `ParamSpecifier`
 *
 * @return `ParamSpecifier`
 */
inline ParamSpecifier
operator&(ParamSpecifier lhs, ParamSpecifier rhs)
{
	return static_cast<ParamSpecifier>(static_cast<uint8_t>(lhs) & static_cast<uint8_t>(rhs));
}

// MARK: - Doxygen Tags

enum class DoxygenTag
{
	NoTag,
	Generic,
	Md,
	Markdown,
	Brief,
	Param,
	Returns,
	Retval,
	Result,
	Note,
	Remark,
	Remarks,
	Example,
	Deprecated,
	Warning
};

struct DoxygenGenericTag
{
	int headingSize;
	std::string name;
	std::string content;
};

struct DoxygenParamTag
{
	std::string name;
	std::string desc;

	std::optional<std::string> type;
	std::optional<std::string> typeAka;

	ParamSpecifier specifiers;
};

struct DoxygenExampleTag
{
	std::string language;
	std::string contents;
};

// MARK: - Doxygen Data

struct DoxygenData
{
	std::string brief;
	std::vector<std::string> warnings;
	std::string deprecation;
	std::optional<DoxygenExampleTag> example;
	std::vector<DoxygenParamTag> params;
	std::string returns;
	std::map<std::string, std::string> retvals;
	std::vector<DoxygenGenericTag> genericTags;
	std::vector<std::string> rawMarkdown;
	std::vector<std::string> userLines;
};

// MARK: - Doxygen Parser

void parseDoxygenTags(const HoverInfo* hoverInfo, llvm::StringRef comment, markup::Document& output);

} // namespace clang::clangd::c32

#endif

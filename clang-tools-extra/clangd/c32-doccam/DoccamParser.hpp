#ifndef LLVM_CLANG_TOOLS_EXTRA_CLANGD_C32_DOCCAM_PARSER_HPP
#define LLVM_CLANG_TOOLS_EXTRA_CLANGD_C32_DOCCAM_PARSER_HPP
#include "clang/Format/Format.h"

#include <string_view>
#include <vector>

namespace clang::clangd::c32::markdown {
class Document;
} // namespace clang::clangd::c32::markdown

namespace clang::clangd::c32::doccam {

struct ParameterTag {
  enum class Specifier : uint8_t {
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
  std::string Name;

  /// Description of the parameter
  std::string Description;

  /// Data access specifiers (e.g., `[in]`, `[out]`, `[in:opt]`, etc.)
  Specifier Specifiers;

  /// Resolved data type, set when a symbol matching this parameter is found for
  /// \m Name
  std::optional<std::string> Type;

  /// Resolved underlying data type, set when a symbol matching this parameter
  /// is found and has an underlying type
  std::optional<std::string> TypeAKA;
};

struct CodeExampleTag {
  /// Name of the opening tag (e.g., "example", "code", etc.) since multiple
  /// tags map to this struct.
  std::string Name;

  /// Programming language of the code example
  std::string Lang;

  /// Code content of the example
  std::string Code;
};

struct CustomTag {
  /// Name of the custom tag
  std::string Name;

  /// Body content of the custom tag
  std::string Body;
};

struct ParsedDoccam {
  /// Parsed from `^@brief`
  std::string Brief;

  /// Parsed from `^@warning`
  std::vector<std::string> Warnings;

  /// Parsed from `^@deprecated`
  std::string Deprecated;

  std::vector<std::string> UntaggedLines;

  std::vector<CustomTag> CustomTags;

  /// Parsed from `^@param`
  std::vector<ParameterTag> Parameters;

  /// Parsed from `^@tparam`
  std::map<std::string, std::string> TParams;

  /// Parsed from `^@returns`
  std::string Returns;

  /// Parsed from `^@retval`
  std::map<std::string, std::string> Retvals;

  /// Parsed from `^@throw`
  std::map<std::string, std::string> Throws;

  /// Parsed from `^@version`
  std::pair<std::string, std::string> Version;

  /// Parsed from `^@example` and `^@code`
  std::vector<CodeExampleTag> CodeExamples;
};

/**
 * @brief
 * 		Parse Doccam comments from \p Contents.
 *
 * @param[in] Contents
 * 		Contents to parse Doccam comments from.
 *
 * @param[in] Style
 * 		Format style to use when parsing (for indentation, etc.)
 *
 * @returns
 * 		%ParsedDoccam struct with parsed Doccam comments.
 */
ParsedDoccam parse(std::string_view Contents, const format::FormatStyle &Style);

// MARK: - Specifier Operators

/**
 * @brief
 * 		Bitwise OR operator (`|`) for %ParameterTag::Specifier enum.
 *
 * @param[in] LHS
 * 		Left-hand side specifier.
 *
 * @param[in] RHS
 * 		Right-hand side specifier.
 *
 * @returns
 * 		Result of the bitwise OR operation.
 */
inline ParameterTag::Specifier operator|(ParameterTag::Specifier LHS,
                                         ParameterTag::Specifier RHS) {
  return static_cast<ParameterTag::Specifier>(static_cast<uint8_t>(LHS) |
                                              static_cast<uint8_t>(RHS));
}

/**
 * @brief
 * 		Bitwise OR assignment operator (`|=`) for
 * %ParameterTag::Specifier enum.
 *
 * @param[in,out] LHS
 * 		Left-hand side specifier to be modified.
 *
 * @param[in] RHS
 * 		Right-hand side specifier.
 *
 * @returns
 * 		Modified left-hand side specifier after the bitwise OR
 * operation.
 */
inline ParameterTag::Specifier &operator|=(ParameterTag::Specifier &LHS,
                                           ParameterTag::Specifier RHS) {
  LHS = RHS | LHS;

  return LHS;
}

/**
 * @brief
 * 		Bitwise AND operator (`&`) for %ParameterTag::Specifier enum.
 *
 * @param[in] LHS
 * 		Left-hand side specifier.
 *
 * @param[in] RHS
 * 		Right-hand side specifier.
 *
 * @returns
 * 		Result of the bitwise AND operation.
 */
inline ParameterTag::Specifier operator&(ParameterTag::Specifier LHS,
                                         ParameterTag::Specifier RHS) {
  return static_cast<ParameterTag::Specifier>(static_cast<uint8_t>(LHS) &
                                              static_cast<uint8_t>(RHS));
}

/// Render a raw documentation string through the c32 doccam parser into a
/// c32::markdown::Document suitable for code completion and signature help.
void renderDocumentation(std::string_view Raw, c32::markdown::Document &Out);

} // namespace clang::clangd::c32::doccam

#endif

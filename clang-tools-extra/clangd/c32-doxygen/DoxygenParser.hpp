#ifndef LLVM_CLANG_TOOLS_EXTRA_CLANGD_C32_DOXYGEN_PARSER_HPP
#define LLVM_CLANG_TOOLS_EXTRA_CLANGD_C32_DOXYGEN_PARSER_HPP

#include <optional>
#include <string_view>

namespace clang::clangd::c32::doxygen {

/** Ancillary data needed to parse a Doxygen comment. */
struct ParsingContext
{
	/// Function, macro, or template parameter
	/// - void foo()
	struct Param
	{
		std::optional<std::string_view> type;

		std::optional<std::string_view> name;

		std::optional<std::string_view> defaultValue;
	};
};

} // namespace clang::clangd::c32::doxygen

#endif

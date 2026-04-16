#ifndef LLVM_CLANG_TOOLS_EXTRA_CLANGD_C32_DOCCAM_MARKDOWN_STYLE_HPP
#define LLVM_CLANG_TOOLS_EXTRA_CLANGD_C32_DOCCAM_MARKDOWN_STYLE_HPP
#include <optional>
#include <string>
#include <string_view>

namespace clang::clangd::c32::doccam::markdown {

// MARK: - Styling

///////////////////////////////////////////////////////////
// Font Styles ////////////////////////////////////////////

enum class FontStyle : unsigned
{
	None		  = 0U,
	Bold		  = 1U << 0,
	Italic		  = 1U << 1,
	Strikethrough = 1U << 2,
};

inline FontStyle
operator|(FontStyle L, FontStyle R)
{
	return static_cast<FontStyle>(static_cast<unsigned>(L) | static_cast<unsigned>(R));
}

inline FontStyle
operator&(FontStyle L, FontStyle R)
{
	return static_cast<FontStyle>(static_cast<unsigned>(L) & static_cast<unsigned>(R));
}

inline FontStyle&
operator|=(FontStyle& L, FontStyle R)
{
	L = L | R;
	return L;
}

inline std::string
fontStyleApply(std::string_view S, FontStyle Font)
{
	std::string Out;

	if (FontStyle::None != (Font & FontStyle::Bold))
		Out += "**";

	if (FontStyle::None != (Font & FontStyle::Italic))
		Out += "*";

	if (FontStyle::None != (Font & FontStyle::Strikethrough))
		Out += "~~";

	Out += S;

	if (FontStyle::None != (Font & FontStyle::Strikethrough))
		Out += "~~";

	if (FontStyle::None != (Font & FontStyle::Italic))
		Out += "*";

	if (FontStyle::None != (Font & FontStyle::Bold))
		Out += "**";

	return Out;
}

///////////////////////////////////////////////////////////
// Tint Styles ////////////////////////////////////////////

enum class TintStyle
{
	None,

	Muted,
	Accent,

	Info,
	Warning,
	Error,
	Success,
};

///////////////////////////////////////////////////////////
// Semantic Styling ///////////////////////////////////////

enum class SemanticStyle
{
	None,

	SymbolArray,
	SymbolBoolean,
	SymbolClass,
	SymbolColor,
	SymbolConstant,
	SymbolConstructor,
	SymbolEnum,
	SymbolEnumMember,
	SymbolEvent,
	SymbolField,
	SymbolFile,
	SymbolFolder,
	SymbolInterface,
	SymbolKey,
	SymbolKeyword,
	SymbolMethod,
	SymbolModule,
	SymbolNamespace,
	SymbolNull,
	SymbolNumber,
	SymbolObject,
	SymbolOperator,
	SymbolPackage,
	SymbolProperty,
	SymbolReference,
	SymbolSnippet,
	SymbolString,
	SymbolStruct,
	SymbolText,
	SymbolTypeParameter,
	SymbolUnit,
	SymbolVariable,
};

///////////////////////////////////////////////////////////
// Markdown Symbols ///////////////////////////////////////

enum class MarkdownSymbolStyle
{
	/// Prefer vscode's built-in "codicon" symbol set, which uses codicon
	/// characters for doccam's special characters and symbols.
	Codicon,

	/// Prefer the "emoji" symbol set, which uses emoji characters for
	/// doccam's special characters and symbols.
	Emoji,

	/// Prefer the "glyph" symbol set, which uses glyph characters for
	/// doccam's special characters and symbols.
	Glyph,
};

struct MarkdownSymbol
{
	enum class ID
	{
		Unknown,

		Question,
		Warning,
		Success,
		Error,
		Bug,
		Notebook,

		///////////////////////////////////////////////////////////////////////
		// Semantic symbols (e.g., function, variable, class icons in hover) //

		SymbolArray,
		SymbolBoolean,
		SymbolClass,
		SymbolColor,
		SymbolConstant,
		SymbolConstructor,
		SymbolDestructor,
		SymbolEnum,
		SymbolEnumMember,
		SymbolEvent,
		SymbolField,
		SymbolFile,
		SymbolFolder,
		SymbolIncludeDirective,
		SymbolInterface,
		SymbolKey,
		SymbolKeyword,
		SymbolMacro,
		SymbolMethod,
		SymbolMisc,
		SymbolModule,
		SymbolNamespace,
		SymbolNull,
		SymbolNumber,
		SymbolObject,
		SymbolOperator,
		SymbolPackage,
		SymbolParameter,
		SymbolProperty,
		SymbolReference,
		SymbolRuler,
		SymbolSnippet,
		SymbolString,
		SymbolStruct,
		SymbolText,
		SymbolTypeParameter,
		SymbolUnion,
		SymbolUnit,
		SymbolValue,
		SymbolVariable,

		///////////////////////////////////////////////////////////////////////
	};

	/// Unique identifier for the character
	ID Identifier;

	/// VSCode's codicon variant of the character (e.g., \r ID::Warning produces
	/// `$(warning)`)
	std::optional<std::string_view> Codicon;

	/// Emoji variant of the character (e.g., \r ID::Warning produces `⚠️`)
	std::optional<std::string_view> Emoji;

	/// Glyph variant of the character (e.g., \r ID::Warning produces `⚠︎`)
	std::optional<std::string_view> Glyph;
};

const MarkdownSymbol* markdownSymbolLookup(MarkdownSymbol::ID Identifier);

std::string_view markdownSymbolResolve(const MarkdownSymbol::ID& Identifier, const MarkdownSymbolStyle& Style);

///////////////////////////////////////////////////////////

struct TextStyle
{
	FontStyle	  Font	   = FontStyle::None;
	TintStyle	  Tint	   = TintStyle::None;
	SemanticStyle Semantic = SemanticStyle::None;

	inline bool
	isDefault() const
	{
		return Font == FontStyle::None && Tint == TintStyle::None &&
			Semantic == SemanticStyle::None;
	}
};

} // namespace clang::clangd::c32::doccam::markdown

#endif

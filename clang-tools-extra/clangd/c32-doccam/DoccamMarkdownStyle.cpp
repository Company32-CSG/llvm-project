#include "c32-doccam/DoccamMarkdownStyle.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace clang::clangd::c32::doccam::markdown {

static constexpr MarkdownSymbol MarkdownSymbolTable[] = {
	{ MarkdownSymbol::ID::Unknown, "⍰", "⍰", "⍰" },

	{ MarkdownSymbol::ID::Question, "❔", "❔", "$(question)" },
	{ MarkdownSymbol::ID::Warning, "⚠️", "⚠︎", "$(warning)" },
	{ MarkdownSymbol::ID::Success, "✅", "✓", "$(check)" },
	{ MarkdownSymbol::ID::Error, "❌", "✗", "$(error)" },
	{ MarkdownSymbol::ID::Bug, "🐞", "", "$(bug)" },
	{ MarkdownSymbol::ID::Notebook, "📓", "⑆", "$(notebook)" },

	///////////////////////////////////////////////////////////////////////
	// Semantic symbols (e.g., function, variable, class icons in hover) //

	{ MarkdownSymbol::ID::SymbolArray, "[]", "[]", "$(symbol-array)" },
	{ MarkdownSymbol::ID::SymbolBoolean, "↔️", "⧑", "$(symbol-boolean)" },
	{ MarkdownSymbol::ID::SymbolClass, "🏷️", "Ⓒ", "$(symbol-class)" },
	{ MarkdownSymbol::ID::SymbolColor, "🎨", "✎", "$(symbol-color)" },
	{ MarkdownSymbol::ID::SymbolConstant, "🟰", "=", "$(symbol-constant)" },
	{ MarkdownSymbol::ID::SymbolConstructor, "🏗️", "⛠", "$(symbol-constructor)" },
	{ MarkdownSymbol::ID::SymbolDestructor, "🗑️", "⌫", "$(symbol-destructor)" },
	{ MarkdownSymbol::ID::SymbolEnum, "🔢", "⊂", "$(symbol-enum)" },
	{ MarkdownSymbol::ID::SymbolEnumMember, "🔹", "⪽", "$(symbol-enum-member)" },
	{ MarkdownSymbol::ID::SymbolEvent, "⚡️", "⌁", "$(symbol-event)" },
	{ MarkdownSymbol::ID::SymbolField, "≔", "≔", "$(symbol-field)" },
	{ MarkdownSymbol::ID::SymbolFile, "📄", "⍞", "$(symbol-file)" },
	{ MarkdownSymbol::ID::SymbolFolder, "📁", "▤", "$(symbol-folder)" },
	{ MarkdownSymbol::ID::SymbolIncludeDirective, "#️⃣", "#", "$(file-code)" },
	{ MarkdownSymbol::ID::SymbolInterface, "🔌", "⊷", "$(symbol-interface)" },
	{ MarkdownSymbol::ID::SymbolKey, "🔑", "⚿", "$(symbol-key)" },
	{ MarkdownSymbol::ID::SymbolKeyword, "🔤", "⌘", "$(symbol-keyword)" },
	{ MarkdownSymbol::ID::SymbolMacro, "𝒎", "𝒎", "𝒎" },
	{ MarkdownSymbol::ID::SymbolMethod, "𝒇", "𝒇", "$(symbol-method)" },
	{ MarkdownSymbol::ID::SymbolMisc, "🛠️", "⚒", "$(symbol-misc)" },
	{ MarkdownSymbol::ID::SymbolModule, "📦", "⧮", "$(symbol-module)" },
	{ MarkdownSymbol::ID::SymbolNamespace, "🌐", "⫚", "$(symbol-namespace)" },
	{ MarkdownSymbol::ID::SymbolNull, "∅", "∅", "$(symbol-null)" },
	{ MarkdownSymbol::ID::SymbolNumber, "🔢", "#", "$(symbol-number)" },
	{ MarkdownSymbol::ID::SymbolObject, "⏺️", "⌬", "$(symbol-object)" },
	{ MarkdownSymbol::ID::SymbolOperator, "➗", "±", "$(symbol-operator)" },
	{ MarkdownSymbol::ID::SymbolPackage, "📦", "▥", "$(symbol-package)" },
	{ MarkdownSymbol::ID::SymbolParameter, "𝒑", "𝒑", "$(symbol-parameter)" },
	{ MarkdownSymbol::ID::SymbolProperty, "🔧", "≓", "$(symbol-property)" },
	{ MarkdownSymbol::ID::SymbolReference, "🔗", "※", "$(symbol-reference)" },
	{ MarkdownSymbol::ID::SymbolRuler, "📏", "⩶", "$(symbol-ruler)" },
	{ MarkdownSymbol::ID::SymbolSnippet, "✂️", "✂", "$(symbol-snippet)" },
	{ MarkdownSymbol::ID::SymbolString, "🔤", "\"\"", "$(symbol-string)" },
	{ MarkdownSymbol::ID::SymbolStruct, "⚎", "⚎", "$(symbol-struct)" },
	{ MarkdownSymbol::ID::SymbolText, "📝", "¶", "$(symbol-text)" },
	{ MarkdownSymbol::ID::SymbolTypeParameter, "𝑻", "𝑻", "$(symbol-type-parameter)" },
	{ MarkdownSymbol::ID::SymbolUnion, "∪", "∪", "∪" },
	{ MarkdownSymbol::ID::SymbolUnit, "⚖️", "𐄷", "$(symbol-unit)" },
	{ MarkdownSymbol::ID::SymbolValue, "𝒗", "𝒗", "$(symbol-value)" },
	{ MarkdownSymbol::ID::SymbolVariable, "𝒙", "𝒙", "$(symbol-variable)" },

	///////////////////////////////////////////////////////////////////////
};

// MARK: - Functions

const MarkdownSymbol*
markdownSymbolLookup(MarkdownSymbol::ID Identifier)
{
	for (const auto& Entry : MarkdownSymbolTable)
	{
		if (Entry.Identifier == Identifier)
			return &Entry;
	}

	return nullptr;
}

std::string_view
markdownSymbolResolve(const MarkdownSymbol::ID& Identifier, const MarkdownSymbolStyle& Style)
{
	const auto* Sym = markdownSymbolLookup(Identifier);

	if (nullptr == Sym)
		return {};

	switch (Style)
	{
		case MarkdownSymbolStyle::Codicon:
			return Sym->Codicon.value_or(Sym->Glyph.value_or(Sym->Emoji.value_or("")));
		case MarkdownSymbolStyle::Glyph:
			return Sym->Glyph.value_or(Sym->Codicon.value_or(Sym->Emoji.value_or("")));
		case MarkdownSymbolStyle::Emoji:
			return Sym->Emoji.value_or(Sym->Codicon.value_or(Sym->Glyph.value_or("")));
	}

	return {};
}

} // namespace clang::clangd::c32::doccam::markdown

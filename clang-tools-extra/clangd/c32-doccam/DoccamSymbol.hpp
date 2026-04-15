#ifndef LLVM_CLANG_TOOLS_EXTRA_CLANGD_C32_DOCCAM_SYMBOL_HPP
#define LLVM_CLANG_TOOLS_EXTRA_CLANGD_C32_DOCCAM_SYMBOL_HPP
#include "clang/Index/IndexSymbol.h"

#include <optional>
#include <string_view>

namespace clang::clangd::c32::doccam {

enum class SymbolStyle {
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

struct Symbol {
  enum class ID {
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

const Symbol *symbolLookup(Symbol::ID Identifier);

const Symbol *symbolLookup(const clang::index::SymbolKind &Kind);

std::string_view symbolResolve(Symbol::ID Identifier, const SymbolStyle &Style);

std::string_view symbolResolve(const clang::index::SymbolKind &Kind,
                               const SymbolStyle &Style);

}; // namespace clang::clangd::c32::doccam

#endif

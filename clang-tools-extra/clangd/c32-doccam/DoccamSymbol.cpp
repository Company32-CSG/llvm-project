#include "c32-doccam/DoccamSymbol.hpp"

#include "clang/Index/IndexSymbol.h"

#include <string_view>

namespace clang::clangd::c32::doccam {

static constexpr Symbol SymbolTable[] = {
    {Symbol::ID::Unknown, "⍰", "⍰", "⍰"},

    {Symbol::ID::Question, "❔", "❔", "$(question)"},
    {Symbol::ID::Warning, "⚠️", "⚠︎", "$(warning)"},
    {Symbol::ID::Success, "✅", "✓", "$(check)"},
    {Symbol::ID::Error, "❌", "✗", "$(error)"},
    {Symbol::ID::Bug, "🐞", "", "$(bug)"},
    {Symbol::ID::Notebook, "📓", "⑆", "$(notebook)"},

    ///////////////////////////////////////////////////////////////////////
    // Semantic symbols (e.g., function, variable, class icons in hover) //

    {Symbol::ID::SymbolArray, "[]", "[]", "$(symbol-array)"},
    {Symbol::ID::SymbolBoolean, "↔️", "⧑", "$(symbol-boolean)"},
    {Symbol::ID::SymbolClass, "🏷️", "Ⓒ", "$(symbol-class)"},
    {Symbol::ID::SymbolColor, "🎨", "✎", "$(symbol-color)"},
    {Symbol::ID::SymbolConstant, "🟰", "=", "$(symbol-constant)"},
    {Symbol::ID::SymbolConstructor, "🏗️", "⛠", "$(symbol-constructor)"},
    {Symbol::ID::SymbolDestructor, "🗑️", "⌫", "$(symbol-destructor)"},
    {Symbol::ID::SymbolEnum, "🔢", "⊂", "$(symbol-enum)"},
    {Symbol::ID::SymbolEnumMember, "🔹", "⪽", "$(symbol-enum-member)"},
    {Symbol::ID::SymbolEvent, "⚡️", "⌁", "$(symbol-event)"},
    {Symbol::ID::SymbolField, "≔", "≔", "$(symbol-field)"},
    {Symbol::ID::SymbolFile, "📄", "⍞", "$(symbol-file)"},
    {Symbol::ID::SymbolFolder, "📁", "▤", "$(symbol-folder)"},
    {Symbol::ID::SymbolIncludeDirective, "#️⃣", "#", "$(file-code)"},
    {Symbol::ID::SymbolInterface, "🔌", "⊷", "$(symbol-interface)"},
    {Symbol::ID::SymbolKey, "🔑", "⚿", "$(symbol-key)"},
    {Symbol::ID::SymbolKeyword, "🔤", "⌘", "$(symbol-keyword)"},
    {Symbol::ID::SymbolMacro, "𝒎", "𝒎", "𝒎"},
    {Symbol::ID::SymbolMethod, "𝒇", "𝒇", "$(symbol-method)"},
    {Symbol::ID::SymbolMisc, "🛠️", "⚒", "$(symbol-misc)"},
    {Symbol::ID::SymbolModule, "📦", "⧮", "$(symbol-module)"},
    {Symbol::ID::SymbolNamespace, "🌐", "⫚", "$(symbol-namespace)"},
    {Symbol::ID::SymbolNull, "∅", "∅", "$(symbol-null)"},
    {Symbol::ID::SymbolNumber, "🔢", "#", "$(symbol-number)"},
    {Symbol::ID::SymbolObject, "⏺️", "⌬", "$(symbol-object)"},
    {Symbol::ID::SymbolOperator, "➗", "±", "$(symbol-operator)"},
    {Symbol::ID::SymbolPackage, "📦", "▥", "$(symbol-package)"},
    {Symbol::ID::SymbolParameter, "𝒑", "𝒑", "$(symbol-parameter)"},
    {Symbol::ID::SymbolProperty, "🔧", "≓", "$(symbol-property)"},
    {Symbol::ID::SymbolReference, "🔗", "※", "$(symbol-reference)"},
    {Symbol::ID::SymbolRuler, "📏", "⩶", "$(symbol-ruler)"},
    {Symbol::ID::SymbolSnippet, "✂️", "✂", "$(symbol-snippet)"},
    {Symbol::ID::SymbolString, "🔤", "\"\"", "$(symbol-string)"},
    {Symbol::ID::SymbolStruct, "⚎", "⚎", "$(symbol-struct)"},
    {Symbol::ID::SymbolText, "📝", "¶", "$(symbol-text)"},
    {Symbol::ID::SymbolTypeParameter, "𝑻", "𝑻", "$(symbol-type-parameter)"},
    {Symbol::ID::SymbolUnion, "∪", "∪", "∪"},
    {Symbol::ID::SymbolUnit, "⚖️", "𐄷", "$(symbol-unit)"},
    {Symbol::ID::SymbolValue, "𝒗", "𝒗", "$(symbol-value)"},
    {Symbol::ID::SymbolVariable, "𝒙", "𝒙", "$(symbol-variable)"},

    ///////////////////////////////////////////////////////////////////////
};

// MARK: - Functions

const Symbol *symbolLookup(Symbol::ID Identifier) {
  for (const auto &Entry : SymbolTable) {
    if (Entry.Identifier == Identifier)
      return &Entry;
  }

  return nullptr;
}

const Symbol *symbolLookup(const clang::index::SymbolKind &Kind) {
  switch (Kind) {
  case clang::index::SymbolKind::Module:
    return symbolLookup(Symbol::ID::SymbolModule);

  case clang::index::SymbolKind::Namespace:
  case clang::index::SymbolKind::NamespaceAlias:
    return symbolLookup(Symbol::ID::SymbolNamespace);

  case clang::index::SymbolKind::Macro:
    return symbolLookup(Symbol::ID::SymbolMacro);

  case clang::index::SymbolKind::Enum:
    return symbolLookup(Symbol::ID::SymbolEnum);

  case clang::index::SymbolKind::EnumConstant:
    return symbolLookup(Symbol::ID::SymbolEnumMember);

  case clang::index::SymbolKind::Struct:
    return symbolLookup(Symbol::ID::SymbolStruct);

  case clang::index::SymbolKind::Class:
    return symbolLookup(Symbol::ID::SymbolClass);

  case clang::index::SymbolKind::Protocol:
    return symbolLookup(Symbol::ID::SymbolInterface);

  case clang::index::SymbolKind::Extension:
    return symbolLookup(Symbol::ID::SymbolMisc);

  case clang::index::SymbolKind::Union:
    return symbolLookup(Symbol::ID::SymbolUnion);

  case clang::index::SymbolKind::TypeAlias:
    return symbolLookup(Symbol::ID::SymbolMisc);

  case clang::index::SymbolKind::Function:
  case clang::index::SymbolKind::InstanceMethod:
  case clang::index::SymbolKind::ClassMethod:
  case clang::index::SymbolKind::StaticMethod:
    return symbolLookup(Symbol::ID::SymbolMethod);

  case clang::index::SymbolKind ::InstanceProperty:
  case clang::index ::SymbolKind ::ClassProperty:
  case clang ::index ::SymbolKind ::StaticProperty:
    return symbolLookup(Symbol ::ID ::SymbolProperty);

  case clang ::index ::SymbolKind ::Variable:
    return symbolLookup(Symbol ::ID ::SymbolVariable);

  case clang ::index ::SymbolKind ::Field:
    return symbolLookup(Symbol ::ID ::SymbolField);

  case clang ::index ::SymbolKind ::Constructor:
    return symbolLookup(Symbol ::ID ::SymbolConstructor);

  case clang ::index ::SymbolKind ::Destructor:
    return symbolLookup(Symbol ::ID ::SymbolDestructor);

  case clang ::index ::SymbolKind ::ConversionFunction:
    return symbolLookup(Symbol ::ID ::SymbolMethod);

  case clang ::index ::SymbolKind ::Parameter:
    return symbolLookup(Symbol ::ID ::SymbolParameter);
  default:
    return symbolLookup(Symbol::ID::Unknown);
  }
}

std::string_view symbolResolve(Symbol::ID Identifier,
                               const SymbolStyle &Style) {
  const auto *Sym = symbolLookup(Identifier);

  if (nullptr == Sym)
    return {};

  switch (Style) {
  case SymbolStyle::Codicon:
    return Sym->Codicon.value_or(Sym->Glyph.value_or(Sym->Emoji.value_or("")));
  case SymbolStyle::Glyph:
    return Sym->Glyph.value_or(Sym->Codicon.value_or(Sym->Emoji.value_or("")));
  case SymbolStyle::Emoji:
    return Sym->Emoji.value_or(Sym->Codicon.value_or(Sym->Glyph.value_or("")));
  }

  return {};
}

std::string_view symbolResolve(const clang::index::SymbolKind &Kind,
                               const SymbolStyle &Style) {
  switch (Kind) {
  case clang::index::SymbolKind::Module:
    return symbolResolve(Symbol::ID::SymbolModule, Style);

  case clang::index::SymbolKind::Namespace:
    return symbolResolve(Symbol::ID::SymbolNamespace, Style);

  case clang::index::SymbolKind::NamespaceAlias:
    return symbolResolve(Symbol::ID::SymbolNamespace, Style);

  case clang::index::SymbolKind::Macro:
    return symbolResolve(Symbol::ID::SymbolMacro, Style);

  case clang::index::SymbolKind::Enum:
    return symbolResolve(Symbol::ID::SymbolEnum, Style);

  case clang::index::SymbolKind::EnumConstant:
    return symbolResolve(Symbol::ID::SymbolEnumMember, Style);

  case clang::index::SymbolKind::Struct:
    return symbolResolve(Symbol::ID::SymbolStruct, Style);

  case clang::index::SymbolKind::Class:
    return symbolResolve(Symbol::ID::SymbolClass, Style);

  case clang::index::SymbolKind::Protocol:
    return symbolResolve(Symbol::ID::SymbolInterface, Style);

  case clang::index::SymbolKind::Extension:
    return symbolResolve(Symbol::ID::SymbolMisc, Style);

  case clang::index::SymbolKind::Union:
    return symbolResolve(Symbol::ID::SymbolUnion, Style);

  case clang::index::SymbolKind::TypeAlias:
    return symbolResolve(Symbol::ID::SymbolMisc, Style);

  case clang::index::SymbolKind::Function:
    return symbolResolve(Symbol::ID::SymbolMethod, Style);

  case clang::index::SymbolKind::InstanceMethod:
    return symbolResolve(Symbol::ID::SymbolMethod, Style);

  case clang::index::SymbolKind::ClassMethod:
    return symbolResolve(Symbol::ID::SymbolMethod, Style);

  case clang::index::SymbolKind::StaticMethod:
    return symbolResolve(Symbol::ID::SymbolMethod, Style);

  case clang::index::SymbolKind::InstanceProperty:
    return symbolResolve(Symbol::ID::SymbolProperty, Style);

  case clang::index::SymbolKind::ClassProperty:
    return symbolResolve(Symbol::ID::SymbolProperty, Style);

  case clang::index::SymbolKind::StaticProperty:
    return symbolResolve(Symbol::ID::SymbolProperty, Style);

  case clang::index::SymbolKind::Variable:
    return symbolResolve(Symbol::ID::SymbolVariable, Style);

  case clang::index::SymbolKind::Field:
    return symbolResolve(Symbol::ID::SymbolField, Style);

  case clang::index::SymbolKind::Constructor:
    return symbolResolve(Symbol::ID::SymbolConstructor, Style);

  case clang::index::SymbolKind::Destructor:
    return symbolResolve(Symbol::ID::SymbolDestructor, Style);

  case clang::index::SymbolKind::ConversionFunction:
    return symbolResolve(Symbol::ID::SymbolMethod, Style);

  case clang::index::SymbolKind::Parameter:
    return symbolResolve(Symbol::ID::SymbolParameter, Style);

  case clang::index::SymbolKind::Using:
    return symbolResolve(Symbol::ID::SymbolMisc, Style);

  case clang::index::SymbolKind::TemplateTypeParm:
    return symbolResolve(Symbol::ID::SymbolTypeParameter, Style);

  case clang::index::SymbolKind::TemplateTemplateParm:
    return symbolResolve(Symbol::ID::SymbolTypeParameter, Style);

  case clang::index::SymbolKind::NonTypeTemplateParm:
    return symbolResolve(Symbol::ID::SymbolParameter, Style);

  case clang::index::SymbolKind::Concept:
    return symbolResolve(Symbol::ID::SymbolMisc, Style);

  case clang::index::SymbolKind::IncludeDirective:
    return symbolResolve(Symbol::ID::SymbolIncludeDirective, Style);

  default:
    return symbolResolve(Symbol::ID::Unknown, Style);
  }
}

} // namespace clang::clangd::c32::doccam

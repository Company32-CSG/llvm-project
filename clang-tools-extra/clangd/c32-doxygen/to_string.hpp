#ifndef LLVM_CLANG_TOOLS_EXTRA_CLANGD_C32_TO_STRING_HPP
#define LLVM_CLANG_TOOLS_EXTRA_CLANGD_C32_TO_STRING_HPP
#include "c32-doxygen/Doxygen.hpp"
#include "c32-doxygen/DoxygenParser.hpp"
#include "clang/Index/IndexSymbol.h"

#include <string>

// NOLINTBEGIN

namespace std {

// MARK: - SymbolKind

std::string to_string(clang::index::SymbolKind Value) {
  switch (Value) {
  case clang::index::SymbolKind::Module:
    return "Module";

  case clang::index::SymbolKind::Namespace:
    return "Namespace";

  case clang::index::SymbolKind::NamespaceAlias:
    return "Namespace Alias";

  case clang::index::SymbolKind::Macro:
    return "Macro";

  case clang::index::SymbolKind::Enum:
    return "Enum";

  case clang::index::SymbolKind::EnumConstant:
    return "Enum Constant";

  case clang::index::SymbolKind::Struct:
    return "Struct";

  case clang::index::SymbolKind::Class:
    return "Class";

  case clang::index::SymbolKind::Protocol:
    return "Protocol";

  case clang::index::SymbolKind::Extension:
    return "Extension";

  case clang::index::SymbolKind::Union:
    return "Union";

  case clang::index::SymbolKind::TypeAlias:
    return "Type Alias";

  case clang::index::SymbolKind::Function:
    return "Function";

  case clang::index::SymbolKind::InstanceMethod:
    return "Instance Method";

  case clang::index::SymbolKind::ClassMethod:
    return "Class Method";

  case clang::index::SymbolKind::StaticMethod:
    return "Static Method";

  case clang::index::SymbolKind::InstanceProperty:
    return "Instance Property";

  case clang::index::SymbolKind::ClassProperty:
    return "Class Property";

  case clang::index::SymbolKind::StaticProperty:
    return "Static Property";

  case clang::index::SymbolKind::Variable:
    return "Variable";

  case clang::index::SymbolKind::Field:
    return "Field";

  case clang::index::SymbolKind::Constructor:
    return "Constructor";

  case clang::index::SymbolKind::Destructor:
    return "Destructor";

  case clang::index::SymbolKind::ConversionFunction:
    return "Conversion Function";

  case clang::index::SymbolKind::Parameter:
    return "Parameter";

  case clang::index::SymbolKind::Using:
    return "Using";

  case clang::index::SymbolKind::TemplateTypeParm:
    return "Template Type Parm";

  case clang::index::SymbolKind::TemplateTemplateParm:
    return "Template Template Parm";

  case clang::index::SymbolKind::NonTypeTemplateParm:
    return "Non-Type Template Parm";

  case clang::index::SymbolKind::Concept:
    return "Concept";

  default:
    return "Unknown";
  }
}

// MARK: - ParameterTag::Specifier

std::string
to_string(clang::clangd::c32::doxygen::ParameterTag::Specifier Value,
          bool AsSymbol = true) {
  using namespace clang::clangd::c32::doxygen;

  bool In =
      (ParameterTag::Specifier::None != (Value & ParameterTag::Specifier::In));
  bool Out =
      (ParameterTag::Specifier::None != (Value & ParameterTag::Specifier::Out));
  bool Opt = (ParameterTag::Specifier::None !=
              (Value & ParameterTag::Specifier::Optional));

  if (AsSymbol) {
    if (In && Out)
      return Opt ? "⇳" : "↕︎";
    else if (In)
      return Opt ? "⇣" : "↓";
    else if (Out)
      return Opt ? "⇡" : "↑";
  } else {
    if (In && Out)
      return Opt ? "I/O?" : "I/O";
    else if (In)
      return Opt ? "I?" : "I";
    else if (Out)
      return Opt ? "O?" : "O";
  }

  return "";
}

// MARK: - TagType

std::string to_string(clang::clangd::c32::doxygen::TagType Value) {
  using namespace clang::clangd::c32::doxygen;

  switch (Value) {
  case TagType::Brief:
    return "Brief";

  case TagType::Code:
    return "Code";

  case TagType::Custom:
    return "Custom";

  case TagType::Deprecated:
    return "Deprecated";

  case TagType::End:
    return "End";

  case TagType::Example:
    return "Example";

  case TagType::Member:
    return "Member";

  case TagType::P:
    return "P";

  case TagType::Param:
    return "Param";

  case TagType::Ref:
    return "Ref";

  case TagType::Returns:
    return "Returns";

  case TagType::Retval:
    return "Retval";

  case TagType::Warning:
    return "Warning";

  default:
    return "Unknown";
  }
}

} // namespace std

// NOLINTEND

#endif

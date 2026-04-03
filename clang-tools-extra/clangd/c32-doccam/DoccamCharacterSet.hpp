#ifndef LLVM_CLANG_TOOLS_EXTRA_CLANGD_C32_DOCCAM_CHARACTER_SET_HPP
#define LLVM_CLANG_TOOLS_EXTRA_CLANGD_C32_DOCCAM_CHARACTER_SET_HPP

#include <string>
#include <vector>

namespace clang::clangd::c32::doccam {

struct CharacterSet {
  enum class ID {

    /// Used
    HoveringOver,

    /// Used for warnings in documentation
    Warning
  };

  /// Unique identifier for the character
  ID Identifier;

  /// Text variant of the character (e.g., "warning" instead of "⚠️" or "⚠︎")
  std::string_view Text;

  /// Emoji variant of the character (e.g., "⚠️" instead of "warning")
  std::string_view Emoji;

  /// Glyph variant of the character (e.g., "⚠︎" instead of "warning" or "⚠️")
  std::string_view Glyph;

  CharacterSet(ID Identifier, std::string_view Text, std::string_view Emoji,
               std::string_view Glyph)
      : Identifier(Identifier), Text(Text), Emoji(Emoji), Glyph(Glyph) {}
};

const std::vector<CharacterSet> CharacterSetTable = {
    CharacterSet(CharacterSet::ID::HoveringOver, "Hovering Over:", "👁️", "👁︎"),
};

} // namespace clang::clangd::c32::doccam

#endif

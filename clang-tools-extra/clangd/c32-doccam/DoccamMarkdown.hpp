#ifndef LLVM_CLANG_TOOLS_EXTRA_CLANGD_C32_DOCCAM_MARKDOWN_HPP
#define LLVM_CLANG_TOOLS_EXTRA_CLANGD_C32_DOCCAM_MARKDOWN_HPP
#include "c32-doccam/DoccamContext.hpp"
#include "c32-doccam/DoccamSymbol.hpp"
#include "c32-doccam/DoccamUtils.hpp"

#include "llvm/Support/raw_ostream.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace clang::clangd::c32::doccam::markdown {

// MARK: - Styling

///////////////////////////////////////////////////////////
// Font Styles ////////////////////////////////////////////

enum class FontStyle : unsigned {
  None = 0U,
  Bold = 1U << 0,
  Italic = 1U << 1,
  Strikethrough = 1U << 2,
};

inline FontStyle operator|(FontStyle L, FontStyle R) {
  return static_cast<FontStyle>(static_cast<unsigned>(L) |
                                static_cast<unsigned>(R));
}

inline FontStyle operator&(FontStyle L, FontStyle R) {
  return static_cast<FontStyle>(static_cast<unsigned>(L) &
                                static_cast<unsigned>(R));
}

inline FontStyle &operator|=(FontStyle &L, FontStyle R) {
  L = L | R;
  return L;
}

inline std::string fontStyleApply(std::string_view S, FontStyle Font) {
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

enum class TintStyle {
  None,

  Muted,
  Accent,

  Info,
  Warning,
  Error,
  Success,
};

inline std::string_view tintStyleToVSCodeCSS(TintStyle Style) {
  switch (Style) {
  case TintStyle::Muted:
    return "var(--vscode-doccam-muted)";
  case TintStyle::Accent:
    return "var(--vscode-doccam-accent)";
  case TintStyle::Info:
    return "var(--vscode-doccam-info)";
  case TintStyle::Warning:
    return "var(--vscode-doccam-warning)";
  case TintStyle::Error:
    return "var(--vscode-doccam-error)";
  case TintStyle::Success:
    return "var(--vscode-doccam-success)";
  case TintStyle::None:
  default:
    return {};
  }
}

///////////////////////////////////////////////////////////
// Semantic Styling ///////////////////////////////////////

enum class SemanticStyle {
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

inline std::string_view semanticStyleToVSCodeCSS(SemanticStyle Style) {
  switch (Style) {
  case SemanticStyle::SymbolArray:
    return "var(--vscode-symbolIcon-arrayForeground)";
  case SemanticStyle::SymbolBoolean:
    return "var(--vscode-symbolIcon-booleanForeground)";
  case SemanticStyle::SymbolClass:
    return "var(--vscode-symbolIcon-classForeground)";
  case SemanticStyle::SymbolColor:
    return "var(--vscode-symbolIcon-colorForeground)";
  case SemanticStyle::SymbolConstant:
    return "var(--vscode-symbolIcon-constantForeground)";
  case SemanticStyle::SymbolConstructor:
    return "var(--vscode-symbolIcon-constructorForeground)";
  case SemanticStyle::SymbolEnum:
    return "var(--vscode-symbolIcon-enumForeground)";
  case SemanticStyle::SymbolEnumMember:
    return "var(--vscode-symbolIcon-enumMemberForeground)";
  case SemanticStyle::SymbolEvent:
    return "var(--vscode-symbolIcon-eventForeground)";
  case SemanticStyle::SymbolField:
    return "var(--vscode-symbolIcon-fieldForeground)";
  case SemanticStyle::SymbolFile:
    return "var(--vscode-symbolIcon-fileForeground)";
  case SemanticStyle::SymbolFolder:
    return "var(--vscode-symbolIcon-folderForeground)";
  case SemanticStyle::SymbolInterface:
    return "var(--vscode-symbolIcon-interfaceForeground)";
  case SemanticStyle::SymbolKey:
    return "var(--vscode-symbolIcon-keyForeground)";
  case SemanticStyle::SymbolKeyword:
    return "var(--vscode-symbolIcon-keywordForeground)";
  case SemanticStyle::SymbolMethod:
    return "var(--vscode-symbolIcon-methodForeground)";
  case SemanticStyle::SymbolModule:
    return "var(--vscode-symbolIcon-moduleForeground)";
  case SemanticStyle::SymbolNamespace:
    return "var(--vscode-symbolIcon-namespaceForeground)";
  case SemanticStyle::SymbolNull:
    return "var(--vscode-symbolIcon-nullForeground)";
  case SemanticStyle::SymbolNumber:
    return "var(--vscode-symbolIcon-numberForeground)";
  case SemanticStyle::SymbolObject:
    return "var(--vscode-symbolIcon-objectForeground)";
  case SemanticStyle::SymbolOperator:
    return "var(--vscode-symbolIcon-operatorForeground)";
  case SemanticStyle::SymbolPackage:
    return "var(--vscode-symbolIcon-packageForeground)";
  case SemanticStyle::SymbolProperty:
    return "var(--vscode-symbolIcon-propertyForeground)";
  case SemanticStyle::SymbolReference:
    return "var(--vscode-symbolIcon-referenceForeground)";
  case SemanticStyle::SymbolSnippet:
    return "var(--vscode-symbolIcon-snippetForeground)";
  case SemanticStyle::SymbolString:
    return "var(--vscode-symbolIcon-stringForeground)";
  case SemanticStyle::SymbolStruct:
    return "var(--vscode-symbolIcon-structForeground)";
  case SemanticStyle::SymbolText:
    return "var(--vscode-symbolIcon-textForeground)";
  case SemanticStyle::SymbolTypeParameter:
    return "var(--vscode-symbolIcon-typeParameterForeground)";
  case SemanticStyle::SymbolUnit:
    return "var(--vscode-symbolIcon-unitForeground)";
  case SemanticStyle::SymbolVariable:
    return "var(--vscode-symbolIcon-variableForeground)";
  case SemanticStyle::None:
  default:
    return {};
  }
}

inline SemanticStyle
semanticStyleFromSymbolKind(const clang::index::SymbolKind &Kind) {
  switch (Kind) {
  case clang::index::SymbolKind::Module:
    return SemanticStyle::SymbolModule;

  case clang::index::SymbolKind::Namespace:
  case clang::index::SymbolKind::NamespaceAlias:
    return SemanticStyle::SymbolNamespace;

  case clang::index::SymbolKind::Macro:
    return SemanticStyle::SymbolConstant;

  case clang::index::SymbolKind::Enum:
    return SemanticStyle::SymbolEnum;

  case clang::index::SymbolKind::EnumConstant:
    return SemanticStyle::SymbolEnumMember;

  case clang::index::SymbolKind::Struct:
    return SemanticStyle::SymbolStruct;

  case clang::index::SymbolKind::Class:
    return SemanticStyle::SymbolClass;

  case clang::index::SymbolKind::Protocol:
    return SemanticStyle::SymbolInterface;

  case clang::index::SymbolKind::Extension:
    return SemanticStyle::SymbolClass;

  case clang::index::SymbolKind::Union:
    return SemanticStyle::SymbolStruct;

  case clang::index::SymbolKind::TypeAlias:
    return SemanticStyle::SymbolInterface;

  case clang::index::SymbolKind::Function:
  case clang::index::SymbolKind::InstanceMethod:
  case clang::index::SymbolKind::ClassMethod:
  case clang::index::SymbolKind::StaticMethod:
    return SemanticStyle::SymbolMethod;

  case clang::index::SymbolKind::InstanceProperty:
  case clang::index::SymbolKind::ClassProperty:
  case clang::index::SymbolKind::StaticProperty:
    return SemanticStyle::SymbolProperty;

  case clang::index::SymbolKind::Variable:
    return SemanticStyle::SymbolVariable;

  case clang::index::SymbolKind::Field:
    return SemanticStyle::SymbolField;

  default:
    return SemanticStyle::None;
  }
}

///////////////////////////////////////////////////////////

struct TextStyle {
  FontStyle Font = FontStyle::None;
  TintStyle Tint = TintStyle::None;
  SemanticStyle Semantic = SemanticStyle::None;

  inline bool isDefault() const {
    return Font == FontStyle::None && Tint == TintStyle::None &&
           Semantic == SemanticStyle::None;
  }
};

// MARK: - Rendering

/**
 * Table column alignment specifier
 */
enum class Align {
  /// Left column alignment (`:----`)
  Left,

  /// Right column alignment (`----:`)
  Right,

  /// Center column alignment (`:----:`)
  Center
};

/**
 * Base renderer interface for rendering the markup content.
 */
struct Renderer {
  virtual void emitHeader(unsigned int Level) = 0;

  virtual void emitText(std::string_view S, const TextStyle &Style) = 0;

  virtual void emitCode(std::string_view S, const TextStyle &Style) = 0;

  virtual void emitLink(std::string_view Text, std::string_view Url,
                        const TextStyle &Style) = 0;

  virtual void emitSymbol(Symbol::ID ID, const TextStyle &Style) = 0;

  virtual void emitSpace() = 0;

  virtual void emitBlankLine() = 0;

  virtual void emitNewLine() = 0;
};

/**
 * Options for rendering the markup content.
 */
struct RenderOptions {
  bool UseHtml = false;
  c32::doccam::SymbolStyle PreferredSymbolStyle;

  RenderOptions() = default;

  RenderOptions(const DoccamContext &Ctx) {
    UseHtml = Ctx.UseHtml;
    PreferredSymbolStyle = Ctx.PreferredSymbolStyle;
  }
};

/**
 * Markdown renderer implementation of `Renderer`
 */
class MarkdownRenderer : public Renderer {
private:
  llvm::raw_ostream &Out;
  const RenderOptions &Options;

  bool HasInlineContent = false;
  bool LastWasWhitespace = false;

private:
  void markInlineContent() {
    HasInlineContent = true;
    LastWasWhitespace = false;
  }

  void resetInlineState() {
    HasInlineContent = false;
    LastWasWhitespace = false;
  }

  void emitStyledInline(std::string_view S, const TextStyle &Style) {
    if (S.empty())
      return;

    const std::string_view TintCSS = tintStyleToVSCodeCSS(Style.Tint);
    const std::string_view SemanticCSS =
        semanticStyleToVSCodeCSS(Style.Semantic);

    if (Options.UseHtml && (!TintCSS.empty() || !SemanticCSS.empty())) {
      const std::string Escaped = escapeHtml(S);
      const std::string Wrapped = fontStyleApply(Escaped, Style.Font);

      if (!TintCSS.empty()) {
        Out << "<span style='color:" << TintCSS << ";'>" << Wrapped
            << "</span>";
      } else {
        Out << "<span style='color:" << SemanticCSS << ";'>" << Wrapped
            << "</span>";
      }
    } else {
      Out << fontStyleApply(S, Style.Font);
    }

    markInlineContent();
  }

public:
  MarkdownRenderer(llvm::raw_ostream &Out, const RenderOptions &Options)
      : Out(Out), Options(Options) {}

  void emitHeader(unsigned int Level) override {
    static constexpr const char *ATX[] = {"#",    "##",    "###",
                                          "####", "#####", "######"};

    Level = std::clamp(Level, 1U, 6U);
    Out << ATX[Level - 1U] << " ";
  }

  void emitText(std::string_view S, const TextStyle &Style) override {
    emitStyledInline(S, Style);
  }

  void emitCode(std::string_view S, const TextStyle &Style) override {
    std::string Wrapped;
    Wrapped += "`";
    Wrapped += S;
    Wrapped += "`";

    emitStyledInline(Wrapped, Style);
  }

  void emitLink(std::string_view Text, std::string_view Url,
                const TextStyle &Style) override {
    std::string Wrapped;
    Wrapped += "[";
    Wrapped += Text;
    Wrapped += "](";
    Wrapped += Url;
    Wrapped += ")";

    emitStyledInline(Wrapped, Style);
  }

  void emitSymbol(Symbol::ID ID, const TextStyle &Style) override {
    emitStyledInline(symbolResolve(ID, Options.PreferredSymbolStyle), Style);
  }

  void emitSpace() override {
    if (!HasInlineContent || LastWasWhitespace)
      return;

    Out << " ";
    LastWasWhitespace = true;
  }

  void emitBlankLine() override {
    Out << "\n\n";
    resetInlineState();
  }

  void emitNewLine() override {
    Out << "\n";
    resetInlineState();
  }
};

class PlaintextRenderer : public Renderer {
private:
  llvm::raw_ostream &Out;
  const RenderOptions &Options;

  bool HasInlineContent = false;
  bool LastWasWhitespace = false;

private:
  void markInlineContent() {
    HasInlineContent = true;
    LastWasWhitespace = false;
  }

  void resetInlineState() {
    HasInlineContent = false;
    LastWasWhitespace = false;
  }

  void emitStyledInline(std::string_view S, const TextStyle &Style) {
    if (S.empty())
      return;

    Out << S;
    markInlineContent();
  }

public:
  PlaintextRenderer(llvm::raw_ostream &Out, const RenderOptions &Options)
      : Out(Out), Options(Options) {}

  void emitHeader(unsigned int Level) override { (void)Level; }

  void emitText(std::string_view S, const TextStyle &Style) override {
    emitStyledInline(S, Style);
  }

  void emitCode(std::string_view S, const TextStyle &Style) override {
    std::string Wrapped;
    Wrapped += "`";
    Wrapped += S;
    Wrapped += "`";
    emitStyledInline(Wrapped, Style);
  }

  void emitLink(std::string_view Text, std::string_view Url,
                const TextStyle &Style) override {
    std::string Wrapped(Text);
    Wrapped += " <";
    Wrapped += Url;
    Wrapped += ">";
    emitStyledInline(Wrapped, Style);
  }

  void emitSymbol(Symbol::ID ID, const TextStyle &Style) override {
    auto Preferred = Options.PreferredSymbolStyle;

    if (Preferred == c32::doccam::SymbolStyle::Codicon)
      Preferred = c32::doccam::SymbolStyle::Glyph;

    emitStyledInline(symbolResolve(ID, Preferred), Style);
  }

  void emitSpace() override {
    if (!HasInlineContent || LastWasWhitespace)
      return;

    Out << " ";
    LastWasWhitespace = true;
  }

  void emitBlankLine() override {
    Out << "\n\n";
    resetInlineState();
  }

  void emitNewLine() override {
    Out << "\n";
    resetInlineState();
  }
};

// MARK: - Document

template <typename Base> class ChunkContainer {
private:
  enum class ChunkKind {
    Text,
    Code,
    Link,
    Symbol,
    Space,
    Newline,
    Blankline,
  };

  struct Chunk {
    ChunkKind Kind;

    std::string Text;
    std::string Url;
    Symbol::ID SymbolID = Symbol::ID::Warning;
    TextStyle Style;

    bool CanonicalizeInlineWhitespace = true;

    explicit Chunk(ChunkKind Kind) : Kind(Kind) {}

    Chunk(ChunkKind Kind, std::string Text,
          bool CanonicalizeInlineWhitespace = true)
        : Kind(Kind), Text(std::move(Text)),
          CanonicalizeInlineWhitespace(CanonicalizeInlineWhitespace) {}

    Chunk(ChunkKind Kind, std::string Text, std::string Url,
          bool CanonicalizeInlineWhitespace = true)
        : Kind(Kind), Text(std::move(Text)), Url(std::move(Url)),
          CanonicalizeInlineWhitespace(CanonicalizeInlineWhitespace) {}

    bool stylable() const {
      return ChunkKind::Text == Kind || ChunkKind::Code == Kind ||
             ChunkKind::Link == Kind || ChunkKind::Symbol == Kind;
    }
  };

  std::vector<Chunk> Chunks;

private:
  Chunk &lastStylableChunk() {
    assert(!Chunks.empty());
    assert(Chunks.back().stylable());
    return Chunks.back();
  }

  static void applyTint(TextStyle &Style, TintStyle Tint) { Style.Tint = Tint; }

  static void applyFont(TextStyle &Style, FontStyle Font) {
    Style.Font |= Font;
  }

  static void applySemantic(TextStyle &Style, SemanticStyle Semantic) {
    Style.Semantic = Semantic;
  }

  void applyToAllStylable(void (*Fn)(TextStyle &, TintStyle), TintStyle Value) {
    for (auto &C : Chunks) {
      if (C.stylable())
        Fn(C.Style, Value);
    }
  }

  void applyToAllStylable(void (*Fn)(TextStyle &, FontStyle), FontStyle Value) {
    for (auto &C : Chunks) {
      if (C.stylable())
        Fn(C.Style, Value);
    }
  }

  void applyToAllStylable(void (*Fn)(TextStyle &, SemanticStyle),
                          SemanticStyle Value) {
    for (auto &C : Chunks) {
      if (C.stylable())
        Fn(C.Style, Value);
    }
  }

protected:
  void renderChunks(Renderer &R) const {
    for (const auto &C : Chunks) {
      switch (C.Kind) {
      case ChunkKind::Text:
        R.emitText(C.CanonicalizeInlineWhitespace
                       ? canonicalizeWhitespace(C.Text, false)
                       : std::string_view(C.Text),
                   C.Style);
        break;

      case ChunkKind::Code:
        R.emitCode(C.CanonicalizeInlineWhitespace
                       ? canonicalizeWhitespace(C.Text, false)
                       : std::string_view(C.Text),
                   C.Style);
        break;

      case ChunkKind::Link:
        R.emitLink(C.CanonicalizeInlineWhitespace
                       ? canonicalizeWhitespace(C.Text, false)
                       : std::string_view(C.Text),
                   C.Url, C.Style);
        break;

      case ChunkKind::Symbol:
        R.emitSymbol(C.SymbolID, C.Style);
        break;

      case ChunkKind::Space:
        R.emitSpace();
        break;

      case ChunkKind::Newline:
        R.emitNewLine();
        break;

      case ChunkKind::Blankline:
        R.emitBlankLine();
        break;
      }
    }
  }

public:
  Base &text(std::string_view S) {
    Chunks.emplace_back(ChunkKind::Text, std::string(S), true);
    return static_cast<Base &>(*this);
  }

  Base &rawText(std::string_view S) {
    Chunks.emplace_back(ChunkKind::Text, std::string(S), false);
    return static_cast<Base &>(*this);
  }

  Base &code(std::string_view S) {
    Chunks.emplace_back(ChunkKind::Code, std::string(S), true);
    return static_cast<Base &>(*this);
  }

  Base &rawCode(std::string_view S) {
    Chunks.emplace_back(ChunkKind::Code, std::string(S), false);
    return static_cast<Base &>(*this);
  }

  Base &link(std::string_view Text, std::string_view Url) {
    Chunks.emplace_back(ChunkKind::Link, std::string(Text), std::string(Url),
                        true);
    return static_cast<Base &>(*this);
  }

  Base &rawLink(std::string_view Text, std::string_view Url) {
    Chunks.emplace_back(ChunkKind::Link, std::string(Text), std::string(Url),
                        false);
    return static_cast<Base &>(*this);
  }

  Base &symbol(Symbol::ID ID) {
    Chunks.emplace_back(ChunkKind::Symbol);
    Chunks.back().SymbolID = ID;
    return static_cast<Base &>(*this);
  }

  Base &space() {
    Chunks.emplace_back(ChunkKind::Space);
    return static_cast<Base &>(*this);
  }

  Base &newline() {
    Chunks.emplace_back(ChunkKind::Newline);
    return static_cast<Base &>(*this);
  }

  Base &blankline() {
    Chunks.emplace_back(ChunkKind::Blankline);
    return static_cast<Base &>(*this);
  }

  // Apply to most recent stylable chunk
  Base &tint(TintStyle Tint) {
    lastStylableChunk().Style.Tint = Tint;
    return static_cast<Base &>(*this);
  }

  Base &style(FontStyle Font) {
    lastStylableChunk().Style.Font |= Font;
    return static_cast<Base &>(*this);
  }

  Base &semantic(SemanticStyle Semantic) {
    lastStylableChunk().Style.Semantic = Semantic;
    return static_cast<Base &>(*this);
  }

  Base &bold() { return style(FontStyle::Bold); }

  Base &italic() { return style(FontStyle::Italic); }

  Base &strikethrough() { return style(FontStyle::Strikethrough); }

  // Apply to all current stylable chunks in this container
  Base &tintAll(TintStyle Tint) {
    applyToAllStylable(&applyTint, Tint);
    return static_cast<Base &>(*this);
  }

  Base &styleAll(FontStyle Font) {
    applyToAllStylable(&applyFont, Font);
    return static_cast<Base &>(*this);
  }

  Base &semanticAll(SemanticStyle Semantic) {
    applyToAllStylable(&applySemantic, Semantic);
    return static_cast<Base &>(*this);
  }
};

struct Block {
  virtual ~Block() = default;
  virtual void render(Renderer &) const = 0;
};

class Heading : public Block, public ChunkContainer<Heading> {
private:
  unsigned int Level;

public:
  explicit Heading(unsigned int L) : Level(std::clamp(L, 1U, 6U)) {}

  void render(Renderer &R) const override {
    R.emitBlankLine();
    R.emitHeader(Level);
    renderChunks(R);
    R.emitNewLine();
  }
};

class Paragraph : public Block, public ChunkContainer<Paragraph> {
public:
  void render(Renderer &R) const override {
    R.emitNewLine();
    renderChunks(R);
    R.emitNewLine();
  }
};

class List : public Block {
public:
  class Item : public ChunkContainer<Item> {
  public:
    void render(Renderer &R) const { renderChunks(R); }
  };

private:
  std::vector<Item> Items;
  bool Ordered = false;
  bool Compact = false;

public:
  explicit List(bool Ordered = false) : Ordered(Ordered) {}

  Item &item() {
    Items.emplace_back();
    return Items.back();
  }

  List &compact() {
    Compact = true;
    return *this;
  }

  void render(Renderer &R) const override {
    std::size_t Index = 1U;

    R.emitNewLine();

    if (Compact && 1U == Items.size()) {
      Items.front().render(R);
      R.emitNewLine();
      return;
    }

    for (const auto &Item : Items) {
      if (Ordered)
        R.emitText(std::to_string(Index++) + ". ", {});
      else
        R.emitText("- ", {});

      Item.render(R);
      R.emitNewLine();
    }
  }
};

class Table : public Block {
public:
  class Cell : public ChunkContainer<Cell> {
  public:
    void render(Renderer &R) const { renderChunks(R); }
  };

  class RowBuilder {
  private:
    const std::vector<std::pair<std::string, Align>> &Columns;
    std::vector<Cell> &Cells;

  public:
    RowBuilder(const std::vector<std::pair<std::string, Align>> &Columns,
               std::vector<Cell> &Cells)
        : Columns(Columns), Cells(Cells) {
      assert(Cells.size() == Columns.size());
    }

    Cell &operator[](std::string_view Name) {
      for (std::size_t I = 0U; I < Columns.size(); I++) {
        if (Columns[I].first == Name)
          return Cells[I];
      }

      llvm::errs() << "Table::RowBuilder::operator[] Unknown column name '"
                   << Name << "'\n";
      assert(false && "Table::RowBuilder::operator[] Unknown column name");
      return Cells[0U];
    }
  };

private:
  std::vector<std::pair<std::string, Align>> Columns;
  std::vector<std::vector<Cell>> Rows;

public:
  Table &column(std::string Name, Align Alignment = Align::Left) {
    Columns.emplace_back(std::move(Name), Alignment);
    return *this;
  }

  RowBuilder row() {
    assert(!Columns.empty() && "Table::row() No columns!");
    Rows.emplace_back(Columns.size());
    return RowBuilder(Columns, Rows.back());
  }

  void render(Renderer &R) const override {
    R.emitNewLine();

    R.emitText("| ", {});
    for (const auto &[Column, _] : Columns) {
      R.emitText(Column, {});
      R.emitText(" | ", {});
    }
    R.emitNewLine();

    R.emitText("|", {});
    for (const auto &[_, Alignment] : Columns) {
      switch (Alignment) {
      case Align::Left:
        R.emitText(":---|", {});
        break;
      case Align::Center:
        R.emitText(":---:|", {});
        break;
      case Align::Right:
        R.emitText("---:|", {});
        break;
      }
    }
    R.emitNewLine();

    for (const auto &Row : Rows) {
      R.emitText("| ", {});

      for (const auto &Cell : Row) {
        Cell.render(R);
        R.emitText(" | ", {});
      }

      R.emitNewLine();
    }
  }
};

class Line : public Block {
private:
  std::size_t Thickness = 1U;

public:
  explicit Line(std::size_t Thickness = 1U) : Thickness(Thickness) {}

  void thickness(std::size_t Value) { Thickness = Value; }

  void render(Renderer &R) const override {
    if (0U == Thickness)
      return;

    R.emitBlankLine();

    for (std::size_t I = 0U; I < Thickness; I++) {
      R.emitText("---", {});
      R.emitNewLine();
    }
  }
};

class CodeBlock : public Block {
private:
  std::string Lang;
  std::string Block;

public:
  CodeBlock(std::string Lang, std::string Code)
      : Lang(std::move(Lang)), Block(std::move(Code)) {}

  void render(Renderer &R) const override {
    R.emitNewLine();
    R.emitText("```" + Lang, {});
    R.emitNewLine();
    R.emitText(Block, {});
    R.emitNewLine();
    R.emitText("```", {});
    R.emitNewLine();
  }
};

class BlockQuote : public Block, public ChunkContainer<BlockQuote> {
public:
  void render(Renderer &R) const override {
    R.emitNewLine();
    R.emitText("> ", {});
    renderChunks(R);
    R.emitNewLine();
  }
};

class Document : public Block {
private:
  std::vector<std::unique_ptr<Block>> Blocks;

  void render(Renderer &R) const override {
    for (const auto &B : Blocks)
      B->render(R);
  }

public:
  Document() = default;

  explicit Document(std::string_view Contents) { paragraph().text(Contents); }

  void append(Document &&Other) {
    for (auto &B : Other.Blocks)
      Blocks.push_back(std::move(B));

    Other.Blocks.clear();
  }

  Heading &heading(unsigned int Level) {
    auto RenderBlock = std::make_unique<Heading>(Level);
    auto &Ref = *RenderBlock;
    Blocks.push_back(std::move(RenderBlock));
    return Ref;
  }

  Paragraph &paragraph() {
    auto RenderBlock = std::make_unique<Paragraph>();
    auto &Ref = *RenderBlock;
    Blocks.push_back(std::move(RenderBlock));
    return Ref;
  }

  List &list(bool Ordered = false) {
    auto RenderBlock = std::make_unique<List>(Ordered);
    auto &Ref = *RenderBlock;
    Blocks.push_back(std::move(RenderBlock));
    return Ref;
  }

  Table &table() {
    auto RenderBlock = std::make_unique<Table>();
    auto &Ref = *RenderBlock;
    Blocks.push_back(std::move(RenderBlock));
    return Ref;
  }

  Line &line(std::size_t Thickness = 1U) {
    auto RenderBlock = std::make_unique<Line>(Thickness);
    auto &Ref = *RenderBlock;
    Blocks.push_back(std::move(RenderBlock));
    return Ref;
  }

  CodeBlock &codeBlock(std::string Lang, std::string Code) {
    auto RenderBlock =
        std::make_unique<CodeBlock>(std::move(Lang), std::move(Code));
    auto &Ref = *RenderBlock;
    Blocks.push_back(std::move(RenderBlock));
    return Ref;
  }

  BlockQuote &blockQuote() {
    auto RenderBlock = std::make_unique<BlockQuote>();
    auto &Ref = *RenderBlock;
    Blocks.push_back(std::move(RenderBlock));
    return Ref;
  }

  std::string markdown(const RenderOptions &Options) const {
    std::string Buffer;
    llvm::raw_string_ostream Out(Buffer);
    MarkdownRenderer R(Out, Options);

    render(R);
    Out.flush();
    return Buffer;
  }

  std::string plaintext(const RenderOptions &Options) const {
    std::string Buffer;
    llvm::raw_string_ostream Out(Buffer);
    PlaintextRenderer R(Out, Options);

    render(R);
    Out.flush();
    return Buffer;
  }
};

} // namespace clang::clangd::c32::doccam::markdown

#endif

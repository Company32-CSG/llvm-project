#ifndef LLVM_CLANG_TOOLS_EXTRA_CLANGD_C32_GFM_MARKDOWN_HPP
#define LLVM_CLANG_TOOLS_EXTRA_CLANGD_C32_GFM_MARKDOWN_HPP
#include "Utils.hpp"

#include "llvm/Support/raw_ostream.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace clang::clangd::c32::markdown {

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

  virtual void emitText(std::string_view S) = 0;

  virtual void emitCode(std::string_view S) = 0;

  virtual void emitLink(std::string_view Text, std::string_view Url) = 0;

  virtual void emitSpace() = 0;

  virtual void emitBlankLine() = 0;

  virtual void emitNewLine() = 0;

  virtual void emitBold() = 0;

  virtual void emitItalic() = 0;

  virtual void emitStrikethrough() = 0;
};

/**
 * Markdown renderer implementation of `Renderer`
 */
struct MarkdownRenderer : Renderer {
  llvm::raw_ostream &Out;

  MarkdownRenderer(llvm::raw_ostream &Out) : Out(Out) {}

  void emitHeader(unsigned int Level) override {
    constexpr const char *ATX[] = {"#", "##", "###", "####", "#####", "######"};

    /* Level has been checked before render */
    Level -= 1U;

    Out << ATX[Level] << " ";
  }

  void emitText(std::string_view S) override { Out << S; }

  void emitCode(std::string_view S) override { Out << "`" << S << "`"; }

  void emitLink(std::string_view Text, std::string_view Url) override {
    Out << "[" << Text << "]" << "(" << Url << ")";
  }

  void emitSpace() override { Out << " "; }

  void emitBlankLine() override { Out << "  \n  \n"; }

  void emitNewLine() override { Out << "  \n"; }

  void emitBold() override { Out << "**"; }

  void emitItalic() override { Out << "*"; }

  void emitStrikethrough() override { Out << "~~"; }
};

/**
 * Plaintext renderer implementation of `Renderer`
 */
struct PlaintextRenderer : Renderer {
  llvm::raw_ostream &Out;

  PlaintextRenderer(llvm::raw_ostream &Out) : Out(Out) {}

  void emitHeader(unsigned int Level) override { (void)(Level); }

  void emitText(std::string_view S) override { Out << S; }

  void emitCode(std::string_view S) override { Out << "`" << S << "`"; }

  void emitLink(std::string_view Text, std::string_view Url) override {
    Out << "[" << Text << "]" << "(" << Url << ")";
  }

  void emitSpace() override { Out << " "; }

  void emitBlankLine() override { Out << "\n\n"; }

  void emitNewLine() override { Out << "\n"; }

  void emitBold() override {}

  void emitItalic() override {}

  void emitStrikethrough() override {}
};

/**
 * @brief
 * 		Basic \c ChunkContainer::Chunk container that is used to build
 * up markup content and optionally customize the font with functions like
 * `bold()` and `italic()`.
 *
 * The example below will produce:
 * 		- `Markdown` **Example Text**
 * 		- `Plaintext` Example Text
 *
 * @example[c]
 * 		Document doc;
 *
 * 		doc.paragraph()
 * 				.text("Example Text")
 * 				.bold();
 * @end
 */
template <typename Base> struct ChunkContainer {
private:
  enum class ChunkKind { Text, Code, Link, Space, Newline, Blankline };

  struct Chunk {
    ChunkKind Kind;

    std::string Text;
    std::string Url;

    bool IsBold = false;
    bool IsItalic = false;
    bool IsStrikethrough = false;

    Chunk(ChunkKind Kind, std::string Text = "", std::string Url = "")
        : Kind(Kind), Text(Text), Url(Url) {}

    inline bool stylable() const {
      return ChunkKind::Space != Kind && ChunkKind::Newline != Kind &&
             ChunkKind::Blankline != Kind;
    }
  };

  std::vector<Chunk> Chunks;

public:
  Base &text(std::string S) {
    Chunks.push_back({ChunkKind::Text, S});

    return static_cast<Base &>(*this);
  }

  Base &code(std::string S) {
    Chunks.push_back({ChunkKind::Code, S});

    return static_cast<Base &>(*this);
  }

  Base &link(std::string Text, std::string Url) {
    Chunks.push_back({ChunkKind::Link, Text, Url});

    return static_cast<Base &>(*this);
  }

  Base &space() {
    Chunks.push_back({ChunkKind::Space});

    return static_cast<Base &>(*this);
  }

  Base &newline() {
    Chunks.push_back({ChunkKind::Newline});

    return static_cast<Base &>(*this);
  }

  Base &blankline() {
    Chunks.push_back({ChunkKind::Blankline});

    return static_cast<Base &>(*this);
  }

  Base &bold() {
    assert(!Chunks.empty());

    Chunks.back().IsBold = true;

    return static_cast<Base &>(*this);
  }

  Base &italic() {
    assert(!Chunks.empty());

    Chunks.back().IsItalic = true;

    return static_cast<Base &>(*this);
  }

  Base &strikethrough() {
    assert(!Chunks.empty());

    Chunks.back().IsStrikethrough = true;

    return static_cast<Base &>(*this);
  }

  void renderChunks(Renderer &R) const {
    for (const auto &C : Chunks) {
      auto Text = trim(C.Text);

      if (C.stylable()) {
        if (C.IsBold)
          R.emitBold();

        if (C.IsItalic)
          R.emitItalic();

        if (C.IsStrikethrough)
          R.emitStrikethrough();
      }

      switch (C.Kind) {
      case ChunkKind::Text:
        R.emitText(Text);
        break;

      case ChunkKind::Code:
        R.emitCode(Text);
        break;

      case ChunkKind::Link:
        R.emitLink(Text, C.Url);
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

      if (C.stylable()) {
        if (C.IsStrikethrough)
          R.emitStrikethrough();

        if (C.IsItalic)
          R.emitItalic();

        if (C.IsBold)
          R.emitBold();
      }
    }
  }
};

/**
 * @brief
 * 		Base container for all markup content. Each document consists
 * 		of multiple %Block based classes.
 *
 * The current types are:
 * 		- **`Document`**
 * 		- `Heading`
 * 		- `Paragraph`
 * 		- `List`
 * 		- `Table`
 * 		- `CodeBlock`
 * 		- `BlockQuote`
 */
struct Block {
  virtual ~Block() = default;
  virtual void render(Renderer &) const = 0;
};

/**
 * @brief
 * 		Heading for a %Document.
 *
 * Use %Document::heading() to create headings.
 */
class Heading : public Block, public ChunkContainer<Heading> {
private:
  unsigned int Level;

public:
  Heading(unsigned int L) {
    if (L < 1U)
      L = 0U;

    Level = std::min(L, 6U);
  }

  void render(Renderer &R) const override {
    R.emitBlankLine();

    /// Emit our ATX heading (#)
    R.emitHeader(Level);

    /// Render the chunks for this heading
    renderChunks(R);
  }
};

/**
 * @brief
 * 		Paragraph for a %Document.
 *
 * Use %Document::paragraph() to create paragraphs.
 */
class Paragraph : public Block, public ChunkContainer<Paragraph> {
  void render(Renderer &R) const override {
    R.emitNewLine();

    /// Render the chunks for this paragraph
    renderChunks(R);

    /// Finish the paragraph with blank line to separate from other content
    R.emitNewLine();
  }
};

/**
 * @brief
 * 		Ordered (numerical) or unordered (bullet) lists for a %Document.
 *
 * Use %Document::list() and %Document::list().item() to add lists and items.
 */
class List : public Block, public ChunkContainer<List> {
private:
  std::vector<ChunkContainer> Items;
  bool Ordered;
  bool Compact;

public:
  List(bool Ordered = false) : Ordered(Ordered), Compact(false) {}

  void render(Renderer &R) const override {
    size_t Index = 1U;

    /// Render any title content
    renderChunks(R);

    /// Start our list
    R.emitNewLine();

    if (Compact && 1U == Items.size()) {
      Items.front().renderChunks(R);
    } else {
      for (const auto &Item : Items) {
        if (Ordered)
          R.emitText(std::to_string(Index++) + ". ");
        else
          R.emitText("- ");

        Item.renderChunks(R);

        R.emitNewLine();
      }
    }

    /// End our list
    R.emitNewLine();
  }

  List &item() {
    Items.emplace_back();

    return static_cast<List &>(Items.back());
  }

  List &compact() {
    Compact = true;

    return *this;
  }
};

/**
 * @brief
 * 		Structured table for a %Document.
 *
 * Use %Document::table() and `Document::row()[0U].text("...")`, etc. to add
 * tables and rows to tables.
 */
class Table : public Block {
public:
  class Cell : public Block, public ChunkContainer<Cell> {
    void render(Renderer &R) const override { renderChunks(R); }
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
      for (size_t I = 0U; I < Columns.size(); I++) {
        const auto &[column, _] = Columns[I];

        if (Name == column)
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
  Table() = default;

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

    /// Emit the leading 'wall' of the column headers
    R.emitText("| ");

    /// Emit headers: ' | First | Second | '
    for (const auto &[column, _] : Columns) {
      R.emitText(column);
      R.emitText(" | ");
    }

    R.emitNewLine();

    /// Emit the leading 'wall' of the column-row divider
    R.emitText("|");

    /// Emit divider with proper alignment for each column
    for (const auto &[_, alignment] : Columns) {
      switch (alignment) {
      case Align::Left:
        R.emitText(":---|");
        break;

      case Align::Center:
        R.emitText(":---:|");
        break;

      case Align::Right:
        R.emitText("---:|");
        break;
      }
    }

    /// Emit the content for each row
    for (const auto &Rows : Rows) {
      R.emitNewLine();

      /// Emit the leading 'wall' of each row
      R.emitText("| ");

      for (const auto &RowColumn : Rows) {
        RowColumn.renderChunks(R);

        R.emitText(" | ");
      }
    }

    R.emitNewLine();
  }
};

/**
 * @brief
 * 		Thematic break (line) for a %Document.
 *
 * Use %Document::line() to add a thematic break to the document.
 */
class Line : public Block {
private:
  size_t Thickness;

public:
  Line(size_t Thickness = 1U) : Thickness(Thickness) {}

  /**
   * Update a line's thickness prior to rendering. The line can be omitted
   * by setting \p value to `0`.
   *
   * @param[in] value
   * 		The line's thickness. Setting to `0` will omit the line.
   */
  void thickness(size_t Value) { Thickness = Value; }

  void render(Renderer &R) const override {
    if (0U == Thickness)
      return;

    R.emitBlankLine();

    for (size_t I = 1U; I <= Thickness; I++) {
      R.emitText("---");

      if (I + 1U < Thickness)
        R.emitNewLine();
    }
  }
};

/**
 * @brief
 * 		Syntax highlighted code block for %Document.
 *
 * Use %Document::codeBlock() to add code with syntax highlighting to the
 * document.
 */
class CodeBlock : public Block {
private:
  std::string Lang;
  std::string Block;

public:
  CodeBlock(std::string Lang, std::string Code) : Lang(Lang), Block(Code) {}

  void render(Renderer &R) const override {
    R.emitNewLine();
    R.emitText("```" + std::string(Lang));
    R.emitNewLine();
    R.emitText(Block);
    R.emitNewLine();
    R.emitText("```");
    R.emitNewLine();
  }
};

/**
 * @brief
 * 		Block quotes %Document.
 *
 * Use %Document::blockQuote() to add block quotes to the document.
 */
class BlockQuote : public Block, public ChunkContainer<BlockQuote> {
public:
  void render(Renderer &R) const override {
    R.emitNewLine();

    R.emitText("> ");

    renderChunks(R);

    R.emitNewLine();
  }
};

/**
 * @brief
 * 		Base document to create markup content.
 *
 * @example[c]
 * 		Document Doc;
 *
 * 		Doc.heading(3)
 * 				.text("My Header");
 *
 * 		Doc.line();
 *
 * 		Doc.paragraph()
 * 				.text("Example Text")
 * 				.bold()
 * 				.text("Event more text in this paragraph");
 *
 *		Doc.paragraph()
 * 				.text("This text will be bold, italic, and have
 *a strikethrough!") .bold() .italic() .strikethrough();
 *
 *		Doc.line();
 *
 * 		Doc.codeBlock("c", " int example_function() { printf(\"I will
 *have syntax highlighting!\"); } ");
 * @end
 */
class Document : public Block {
private:
  std::vector<std::shared_ptr<Block>> Blocks;

  void render(Renderer &R) const override {
    for (const auto &B : Blocks) {
      B->render(R);
    }
  }

public:
  Document() {}

  Document(std::string Contents) {
    auto RenderBlock = std::make_unique<Paragraph>();

    Paragraph &Ref = *RenderBlock;

    Ref.text(Contents);

    Blocks.push_back(std::move(RenderBlock));
  }

  void append(const Document &Document) {
    for (const auto &B : Document.Blocks) {
      Blocks.push_back(B);
    }
  }

  Heading &heading(unsigned int Level) {
    auto RenderBlock = std::make_unique<Heading>(Level);

    Heading &Ref = *RenderBlock;

    Blocks.push_back(std::move(RenderBlock));

    return Ref;
  }

  Paragraph &paragraph() {
    auto RenderBlock = std::make_unique<Paragraph>();

    Paragraph &Ref = *RenderBlock;

    Blocks.push_back(std::move(RenderBlock));

    return Ref;
  }

  List &list(bool Ordered = false) {
    auto RenderBlock = std::make_unique<List>(Ordered);

    List &Ref = *RenderBlock;

    Blocks.push_back(std::move(RenderBlock));

    return Ref;
  }

  Table &table() {
    auto RenderBlock = std::make_unique<Table>();

    Table &Ref = *RenderBlock;

    Blocks.push_back(std::move(RenderBlock));

    return Ref;
  }

  Line &line(size_t Thickness = 1U) {
    auto RenderBlock = std::make_unique<Line>(Thickness);

    Line &Ref = *RenderBlock;

    Blocks.push_back(std::move(RenderBlock));

    return Ref;
  }

  CodeBlock &codeBlock(std::string Lang, std::string Code) {
    auto RenderBlock = std::make_unique<CodeBlock>(Lang, Code);

    CodeBlock &Ref = *RenderBlock;

    Blocks.push_back(std::move(RenderBlock));

    return Ref;
  }

  BlockQuote &blockQuote() {
    auto RenderBlock = std::make_unique<BlockQuote>();

    BlockQuote &Ref = *RenderBlock;

    Blocks.push_back(std::move(RenderBlock));

    return Ref;
  }

  std::string markdown() const {
    std::string Buffer;
    llvm::raw_string_ostream Out(Buffer);
    MarkdownRenderer R(Out);

    render(R);

    return Buffer;
  }

  std::string plaintext() const {
    std::string Buffer;
    llvm::raw_string_ostream Out(Buffer);
    PlaintextRenderer R(Out);

    render(R);

    return Buffer;
  }
};

} // namespace clang::clangd::c32::markdown

#endif

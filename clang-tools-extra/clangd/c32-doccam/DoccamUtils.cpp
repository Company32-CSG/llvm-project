#include "c32-doccam/DoccamUtils.hpp"

#include "Config.h"

#include "clang/Format/Format.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <string>
#include <string_view>

namespace clang::clangd::c32::doccam {

/* ------------------------------------------------------------ */

std::string formatCode(const format::FormatStyle &Style,
                       std::string_view Input) {
  auto &Cfg = Config::current();

  auto EffectiveStyle = Style;

  if (!Cfg.C32.Doccam.Hover.UseWorkspaceFormattingStyle) {
    EffectiveStyle = format::getGNUStyle();
  }

  auto Replacements =
      format::reformat(EffectiveStyle, Input, tooling::Range(0, Input.size()));

  if (auto Formatted = tooling::applyAllReplacements(Input, Replacements))
    return *Formatted;

  return std::string(Input);
}

std::string escapeHtml(std::string_view Input) {
  std::string R;

  R.reserve(Input.size());

  for (const char C : Input) {
    switch (C) {
    case '&':
      R += "&amp;";
      break;
    case '<':
      R += "&lt;";
      break;
    case '>':
      R += "&gt;";
      break;
    case '"':
      R += "&quot;";
      break;
    case '\'':
      R += "&apos;";
      break;
    default:
      R += C;
      break;
    }
  }

  return R;
}

bool lineStartsWith(std::string_view Contents, size_t CursorOffset,
                    std::string_view Prefix) {
  if (CursorOffset > Contents.size())
    return false;

  size_t LineStart = Contents.rfind('\n', CursorOffset);

  if (std::string::npos == LineStart)
    LineStart = 0U; // Might be beginning of file, no '\n' found
  else
    LineStart += 1U; // Skip '\n' character

  if (CursorOffset < LineStart)
    return false;

  auto Trimmed =
      std::string_view(Contents).substr(LineStart, CursorOffset - LineStart);

  while (!Trimmed.empty() && std::isspace(Trimmed.front()))
    Trimmed.remove_prefix(1U);

  return Trimmed.size() >= Prefix.size() &&
         Prefix == Trimmed.substr(0U, Prefix.size());
}

std::pair<size_t, std::string_view> extractLine(std::string_view Contents,
                                                size_t Offset) {
  if (Offset > Contents.size())
    return {std::string_view::npos, ""};

  size_t LineStart = Contents.rfind('\n', 0U == Offset ? 0U : (Offset - 1U));

  if (std::string_view::npos == LineStart)
    LineStart = 0U; // Might be beginning of file, no '\n' found
  else
    LineStart += 1U; // Skip '\n' character

  if (Offset < LineStart)
    return {std::string_view::npos, ""};

  return {LineStart, Contents.substr(LineStart, Offset - LineStart)};
}

std::string indentLines(std::string_view Input) {
  // Indent first line
  std::string R = "\t";

  for (auto C : Input) {
    R += C;

    if ('\n' == C)
      R += '\t';
  }

  return R;
}

std::string canonicalizeWhitespace(std::string_view Contents,
                                   bool PreserveNewlines) {
  std::string R;

  R.reserve(Contents.size());

  bool InSpace = false;
  size_t NewlineCount = 0;

  for (const char C : Contents) {
    unsigned const char UC = static_cast<unsigned char>(C);

    /* Preserve new lines */
    if ('\n' == C && PreserveNewlines) {
      NewlineCount += 1U;
      InSpace = false;

      /* Allow up to TWO newlines in a row (one blank line) */
      if (NewlineCount <= 2U)
        R.push_back('\n');

      /* Skip any further newlines */
      continue;
    }

    /* If not a newline, reset counter */
    if ('\n' != C)
      NewlineCount = 0;

    /* Collapse other whitespace to single ' ' */
    if (std::isspace(UC)) {
      /* Skip '\n' here (we handled it above) so this is spaces/tabs/etc. */
      if (InSpace)
        continue;

      InSpace = true;

      R.push_back(' ');
      continue;
    }

    /* Normal character: emit and clear flags */
    InSpace = false;

    R.push_back(C);
  }

  /* Trim trailing space (but leave trailing newline(s)) */
  if (!R.empty() && ' ' == R.back())
    R.pop_back();

  return R;
}

std::string_view::size_type findFirstSpace(std::string_view Contents) {
  const auto *ITR = std::find_if(Contents.begin(), Contents.end(), [](char C) {
    return std::isspace(static_cast<unsigned char>(C));
  });

  return ITR == Contents.end() ? std::string_view::npos
                               : std::distance(Contents.begin(), ITR);
}

std::string ltrim(std::string_view Contents) {
  size_t N = Contents.size();
  size_t I = 0U;

  while (I < N && std::isspace(static_cast<unsigned char>(Contents[I])))
    I += 1U;

  return std::string(Contents.substr(I));
}

std::string rtrim(std::string_view Contents) {
  size_t N = Contents.size();

  while (N > 0U && std::isspace(static_cast<unsigned char>(Contents[N - 1U])))
    N -= 1U;

  return std::string(Contents.substr(0U, N));
}

std::string trim(std::string_view Contents) {
  size_t Start = 0U;
  size_t End = Contents.size();

  while (Start < End &&
         std::isspace(static_cast<unsigned char>(Contents[Start])))
    Start++;

  while (End > Start &&
         std::isspace(static_cast<unsigned char>(Contents[End - 1U])))
    End--;

  return std::string(Contents.substr(Start, End - Start));
}

std::vector<std::string> split(std::string_view Contents,
                               std::string_view Splitter) {
  std::vector<std::string> R;

  if (Contents.empty() || Splitter.empty() || Splitter.size() > Contents.size())
    return {};

  size_t Pos;

  while (1) {
    Pos = Contents.find(Splitter);

    if (std::string_view::npos == Pos)
      break;

    auto Sub = Contents.substr(0U, Pos);

    R.push_back(std::string(Sub));

    Contents.remove_prefix(Pos + Splitter.size());
  }

  /* There might be a piece remaining after the last split */

  if (!Contents.empty())
    R.push_back(std::string(Contents));

  return R;
}

std::string lowercase(std::string_view Contents) {
  std::string R(Contents.size(), '\0');

  std::transform(Contents.begin(), Contents.end(), R.begin(), ::tolower);

  return R;
}

std::string uppercase(std::string_view Contents) {
  std::string R(Contents.size(), '\0');

  std::transform(Contents.begin(), Contents.end(), R.begin(), ::toupper);

  return R;
}

std::string properNounCase(std::string_view Contents) {
  std::string R(Contents.size(), '\0');

  std::transform(Contents.begin(), Contents.end(), R.begin(), ::tolower);

  if (!R.empty())
    R[0U] = ::toupper(R[0U]);

  return R;
}

} // namespace clang::clangd::c32::doccam

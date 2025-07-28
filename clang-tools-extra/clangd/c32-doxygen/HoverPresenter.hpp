#ifndef LLVM_CLANG_TOOLS_EXTRA_CLANGD_C32_HOVER_PRESENTER_HPP
#define LLVM_CLANG_TOOLS_EXTRA_CLANGD_C32_HOVER_PRESENTER_HPP
#include "DoxygenParser.hpp"
#include "Markdown.hpp"

namespace clang::clangd::c32::hover {

void presentForFunction(doxygen::ParsedDoxygen& doxygen, markdown::Document& output);

} // namespace clang::clangd::c32::hover

#endif

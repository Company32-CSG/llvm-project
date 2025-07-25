//===-- CallInterface.cpp -- Procedure call interface ---------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "flang/Lower/CallInterface.h"
#include "flang/Evaluate/fold.h"
#include "flang/Lower/Bridge.h"
#include "flang/Lower/Mangler.h"
#include "flang/Lower/OpenACC.h"
#include "flang/Lower/PFTBuilder.h"
#include "flang/Lower/StatementContext.h"
#include "flang/Lower/Support/Utils.h"
#include "flang/Optimizer/Builder/Character.h"
#include "flang/Optimizer/Builder/FIRBuilder.h"
#include "flang/Optimizer/Builder/Todo.h"
#include "flang/Optimizer/Dialect/FIRDialect.h"
#include "flang/Optimizer/Dialect/FIROpsSupport.h"
#include "flang/Optimizer/Support/InternalNames.h"
#include "flang/Optimizer/Support/Utils.h"
#include "flang/Semantics/symbol.h"
#include "flang/Semantics/tools.h"
#include "flang/Support/Fortran.h"
#include <optional>

static mlir::FunctionType
getProcedureType(const Fortran::evaluate::characteristics::Procedure &proc,
                 Fortran::lower::AbstractConverter &converter);

mlir::Type Fortran::lower::getUntypedBoxProcType(mlir::MLIRContext *context) {
  llvm::SmallVector<mlir::Type> resultTys;
  llvm::SmallVector<mlir::Type> inputTys;
  auto untypedFunc = mlir::FunctionType::get(context, inputTys, resultTys);
  return fir::BoxProcType::get(context, untypedFunc);
}

/// Return the type of a dummy procedure given its characteristic (if it has
/// one).
static mlir::Type getProcedureDesignatorType(
    const Fortran::evaluate::characteristics::Procedure *,
    Fortran::lower::AbstractConverter &converter) {
  // TODO: Get actual function type of the dummy procedure, at least when an
  // interface is given. The result type should be available even if the arity
  // and type of the arguments is not.
  // In general, that is a nice to have but we cannot guarantee to find the
  // function type that will match the one of the calls, we may not even know
  // how many arguments the dummy procedure accepts (e.g. if a procedure
  // pointer is only transiting through the current procedure without being
  // called), so a function type cast must always be inserted.
  return Fortran::lower::getUntypedBoxProcType(&converter.getMLIRContext());
}

//===----------------------------------------------------------------------===//
// Caller side interface implementation
//===----------------------------------------------------------------------===//

bool Fortran::lower::CallerInterface::hasAlternateReturns() const {
  return procRef.hasAlternateReturns();
}

/// Return the binding label (from BIND(C...)) or the mangled name of the
/// symbol.
static std::string
getProcMangledName(const Fortran::evaluate::ProcedureDesignator &proc,
                   Fortran::lower::AbstractConverter &converter) {
  if (const Fortran::semantics::Symbol *symbol = proc.GetSymbol())
    return converter.mangleName(symbol->GetUltimate());
  assert(proc.GetSpecificIntrinsic() &&
         "expected intrinsic procedure in designator");
  return proc.GetName();
}

std::string Fortran::lower::CallerInterface::getMangledName() const {
  return getProcMangledName(procRef.proc(), converter);
}

const Fortran::semantics::Symbol *
Fortran::lower::CallerInterface::getProcedureSymbol() const {
  return procRef.proc().GetSymbol();
}

bool Fortran::lower::CallerInterface::isIndirectCall() const {
  if (const Fortran::semantics::Symbol *symbol = procRef.proc().GetSymbol())
    return Fortran::semantics::IsPointer(*symbol) ||
           Fortran::semantics::IsDummy(*symbol);
  return false;
}

bool Fortran::lower::CallerInterface::requireDispatchCall() const {
  // Procedure pointer component reference do not require dispatch, but
  // have PASS/NOPASS argument.
  if (const Fortran::semantics::Symbol *sym = procRef.proc().GetSymbol())
    if (Fortran::semantics::IsPointer(*sym))
      return false;
  // calls with NOPASS attribute still have their component so check if it is
  // polymorphic.
  if (const Fortran::evaluate::Component *component =
          procRef.proc().GetComponent()) {
    if (Fortran::semantics::IsPolymorphic(component->base().GetLastSymbol()))
      return true;
  }
  // calls with PASS attribute have the passed-object already set in its
  // arguments. Just check if their is one.
  std::optional<unsigned> passArg = getPassArgIndex();
  if (passArg)
    return true;
  return false;
}

std::optional<unsigned>
Fortran::lower::CallerInterface::getPassArgIndex() const {
  unsigned passArgIdx = 0;
  std::optional<unsigned> passArg;
  for (const auto &arg : getCallDescription().arguments()) {
    if (arg && arg->isPassedObject()) {
      passArg = passArgIdx;
      break;
    }
    ++passArgIdx;
  }
  if (!passArg)
    return passArg;
  // Take into account result inserted as arguments.
  if (std::optional<Fortran::lower::CallInterface<
          Fortran::lower::CallerInterface>::PassedEntity>
          resultArg = getPassedResult()) {
    if (resultArg->passBy == PassEntityBy::AddressAndLength)
      passArg = *passArg + 2;
    else if (resultArg->passBy == PassEntityBy::BaseAddress)
      passArg = *passArg + 1;
  }
  return passArg;
}

mlir::Value Fortran::lower::CallerInterface::getIfPassedArg() const {
  if (std::optional<unsigned> passArg = getPassArgIndex()) {
    assert(actualInputs.size() > *passArg && actualInputs[*passArg] &&
           "passed arg was not set yet");
    return actualInputs[*passArg];
  }
  return {};
}

const Fortran::evaluate::ProcedureDesignator *
Fortran::lower::CallerInterface::getIfIndirectCall() const {
  if (const Fortran::semantics::Symbol *symbol = procRef.proc().GetSymbol())
    if (Fortran::semantics::IsPointer(*symbol) ||
        Fortran::semantics::IsDummy(*symbol))
      return &procRef.proc();
  return nullptr;
}

static mlir::Location
getProcedureDesignatorLoc(const Fortran::evaluate::ProcedureDesignator &proc,
                          Fortran::lower::AbstractConverter &converter) {
  // Note: If the callee is defined in the same file but after the current
  // unit we cannot get its location here and the funcOp is created at the
  // wrong location (i.e, the caller location).
  // To prevent this, it is up to the bridge to first declare all functions
  // defined in the translation unit before lowering any calls or procedure
  // designator references.
  if (const Fortran::semantics::Symbol *symbol = proc.GetSymbol())
    return converter.genLocation(symbol->name());
  // Use current location for intrinsics.
  return converter.getCurrentLocation();
}

mlir::Location Fortran::lower::CallerInterface::getCalleeLocation() const {
  return getProcedureDesignatorLoc(procRef.proc(), converter);
}

// Get dummy argument characteristic for a procedure with implicit interface
// from the actual argument characteristic. The actual argument may not be a F77
// entity. The attribute must be dropped and the shape, if any, must be made
// explicit.
static Fortran::evaluate::characteristics::DummyDataObject
asImplicitArg(Fortran::evaluate::characteristics::DummyDataObject &&dummy) {
  std::optional<Fortran::evaluate::Shape> shape =
      dummy.type.attrs().none()
          ? dummy.type.shape()
          : std::make_optional<Fortran::evaluate::Shape>(dummy.type.Rank());
  return Fortran::evaluate::characteristics::DummyDataObject(
      Fortran::evaluate::characteristics::TypeAndShape(dummy.type.type(),
                                                       std::move(shape)));
}

static Fortran::evaluate::characteristics::DummyArgument
asImplicitArg(Fortran::evaluate::characteristics::DummyArgument &&dummy) {
  return Fortran::common::visit(
      Fortran::common::visitors{
          [&](Fortran::evaluate::characteristics::DummyDataObject &obj) {
            return Fortran::evaluate::characteristics::DummyArgument(
                std::move(dummy.name), asImplicitArg(std::move(obj)));
          },
          [&](Fortran::evaluate::characteristics::DummyProcedure &proc) {
            return Fortran::evaluate::characteristics::DummyArgument(
                std::move(dummy.name), std::move(proc));
          },
          [](Fortran::evaluate::characteristics::AlternateReturn &x) {
            return Fortran::evaluate::characteristics::DummyArgument(
                std::move(x));
          }},
      dummy.u);
}

static bool isExternalDefinedInSameCompilationUnit(
    const Fortran::evaluate::ProcedureDesignator &proc) {
  if (const auto *symbol{proc.GetSymbol()})
    return symbol->has<Fortran::semantics::SubprogramDetails>() &&
           symbol->owner().IsGlobal();
  return false;
}

Fortran::evaluate::characteristics::Procedure
Fortran::lower::CallerInterface::characterize() const {
  Fortran::evaluate::FoldingContext &foldingContext =
      converter.getFoldingContext();
  std::optional<Fortran::evaluate::characteristics::Procedure> characteristic =
      Fortran::evaluate::characteristics::Procedure::Characterize(
          procRef.proc(), foldingContext, /*emitError=*/false);
  assert(characteristic && "Failed to get characteristic from procRef");
  // The characteristic may not contain the argument characteristic if the
  // ProcedureDesignator has no interface, or may mismatch in case of implicit
  // interface.
  if (!characteristic->HasExplicitInterface() ||
      (converter.getLoweringOptions().getLowerToHighLevelFIR() &&
       isExternalDefinedInSameCompilationUnit(procRef.proc()) &&
       characteristic->CanBeCalledViaImplicitInterface())) {
    // In HLFIR lowering, calls to subprogram with implicit interfaces are
    // always prepared according to the actual arguments. This is to support
    // cases where the implicit interfaces are "abused" in old and not so old
    // Fortran code (e.g, passing REAL(8) to CHARACTER(8), passing object
    // pointers to procedure dummies, passing regular procedure dummies to
    // character procedure dummies, omitted arguments....).
    // In all those case, if the subprogram definition is in the same
    // compilation unit, the "characteristic" from Characterize will be the one
    // from the definition, in case of "abuses" (for which semantics raise a
    // warning), lowering will be placed in a difficult position if it is given
    // the dummy characteristic from the definition and an actual that has
    // seemingly nothing to do with it: it would need to battle to anticipate
    // and handle these mismatches (e.g., be able to prepare a fir.boxchar<>
    // from a fir.real<> and so one). This was the approach of the lowering to
    // FIR, and usually lead to compiler bug every time a new "abuse" was met in
    // the wild.
    // Instead, in HLFIR, the dummy characteristic is always computed from the
    // actual for subprogram with implicit interfaces, and in case of call site
    // vs fun.func MLIR function type signature mismatch, a function cast is
    // done before placing the call. This is a hammer that should cover all
    // cases and behave like existing compiler that "do not see" the definition
    // when placing the call.
    characteristic->dummyArguments.clear();
    for (const std::optional<Fortran::evaluate::ActualArgument> &arg :
         procRef.arguments()) {
      // "arg" may be null if this is a call with missing arguments compared
      // to the subprogram definition. Do not compute any characteristic
      // in this case.
      if (arg.has_value()) {
        if (arg.value().isAlternateReturn()) {
          characteristic->dummyArguments.emplace_back(
              Fortran::evaluate::characteristics::AlternateReturn{});
        } else {
          // Argument cannot be optional with implicit interface
          const Fortran::lower::SomeExpr *expr = arg.value().UnwrapExpr();
          assert(expr && "argument in call with implicit interface cannot be "
                         "assumed type");
          std::optional<Fortran::evaluate::characteristics::DummyArgument>
              argCharacteristic =
                  Fortran::evaluate::characteristics::DummyArgument::FromActual(
                      "actual", *expr, foldingContext,
                      /*forImplicitInterface=*/true);
          assert(argCharacteristic &&
                 "failed to characterize argument in implicit call");
          characteristic->dummyArguments.emplace_back(
              asImplicitArg(std::move(*argCharacteristic)));
        }
      }
    }
  }
  return *characteristic;
}

void Fortran::lower::CallerInterface::placeInput(
    const PassedEntity &passedEntity, mlir::Value arg) {
  assert(static_cast<int>(actualInputs.size()) > passedEntity.firArgument &&
         passedEntity.firArgument >= 0 &&
         passedEntity.passBy != CallInterface::PassEntityBy::AddressAndLength &&
         "bad arg position");
  actualInputs[passedEntity.firArgument] = arg;
}

void Fortran::lower::CallerInterface::placeAddressAndLengthInput(
    const PassedEntity &passedEntity, mlir::Value addr, mlir::Value len) {
  assert(static_cast<int>(actualInputs.size()) > passedEntity.firArgument &&
         static_cast<int>(actualInputs.size()) > passedEntity.firLength &&
         passedEntity.firArgument >= 0 && passedEntity.firLength >= 0 &&
         passedEntity.passBy == CallInterface::PassEntityBy::AddressAndLength &&
         "bad arg position");
  actualInputs[passedEntity.firArgument] = addr;
  actualInputs[passedEntity.firLength] = len;
}

bool Fortran::lower::CallerInterface::verifyActualInputs() const {
  if (getNumFIRArguments() != actualInputs.size())
    return false;
  for (mlir::Value arg : actualInputs) {
    if (!arg)
      return false;
  }
  return true;
}

mlir::Value
Fortran::lower::CallerInterface::getInput(const PassedEntity &passedEntity) {
  return actualInputs[passedEntity.firArgument];
}

static void walkLengths(
    const Fortran::evaluate::characteristics::TypeAndShape &typeAndShape,
    const Fortran::lower::CallerInterface::ExprVisitor &visitor,
    Fortran::lower::AbstractConverter &converter) {
  Fortran::evaluate::DynamicType dynamicType = typeAndShape.type();
  // Visit length specification expressions that are explicit.
  if (dynamicType.category() == Fortran::common::TypeCategory::Character) {
    if (std::optional<Fortran::evaluate::ExtentExpr> length =
            dynamicType.GetCharLength())
      visitor(toEvExpr(*length), /*assumedSize=*/false);
  } else if (dynamicType.category() == Fortran::common::TypeCategory::Derived &&
             !dynamicType.IsUnlimitedPolymorphic()) {
    const Fortran::semantics::DerivedTypeSpec &derivedTypeSpec =
        dynamicType.GetDerivedTypeSpec();
    if (Fortran::semantics::CountLenParameters(derivedTypeSpec) > 0)
      TODO(converter.getCurrentLocation(),
           "function result with derived type length parameters");
  }
}

void Fortran::lower::CallerInterface::walkResultLengths(
    const ExprVisitor &visitor) const {
  assert(characteristic && "characteristic was not computed");
  const Fortran::evaluate::characteristics::FunctionResult &result =
      characteristic->functionResult.value();
  const Fortran::evaluate::characteristics::TypeAndShape *typeAndShape =
      result.GetTypeAndShape();
  assert(typeAndShape && "no result type");
  return walkLengths(*typeAndShape, visitor, converter);
}

void Fortran::lower::CallerInterface::walkDummyArgumentLengths(
    const PassedEntity &passedEntity, const ExprVisitor &visitor) const {
  if (!passedEntity.characteristics)
    return;
  if (const auto *dummy =
          std::get_if<Fortran::evaluate::characteristics::DummyDataObject>(
              &passedEntity.characteristics->u))
    walkLengths(dummy->type, visitor, converter);
}

// Compute extent expr from shapeSpec of an explicit shape.
static Fortran::evaluate::ExtentExpr
getExtentExpr(const Fortran::semantics::ShapeSpec &shapeSpec) {
  if (shapeSpec.ubound().isStar())
    // F'2023 18.5.3 point 5.
    return Fortran::evaluate::ExtentExpr{-1};
  const auto &ubound = shapeSpec.ubound().GetExplicit();
  const auto &lbound = shapeSpec.lbound().GetExplicit();
  assert(lbound && ubound && "shape must be explicit");
  return Fortran::common::Clone(*ubound) - Fortran::common::Clone(*lbound) +
         Fortran::evaluate::ExtentExpr{1};
}

static void
walkExtents(const Fortran::semantics::Symbol &symbol,
            const Fortran::lower::CallerInterface::ExprVisitor &visitor) {
  if (const auto *objectDetails =
          symbol.detailsIf<Fortran::semantics::ObjectEntityDetails>())
    if (objectDetails->shape().IsExplicitShape() ||
        Fortran::semantics::IsAssumedSizeArray(symbol))
      for (const Fortran::semantics::ShapeSpec &shapeSpec :
           objectDetails->shape())
        visitor(Fortran::evaluate::AsGenericExpr(getExtentExpr(shapeSpec)),
                /*assumedSize=*/shapeSpec.ubound().isStar());
}

void Fortran::lower::CallerInterface::walkResultExtents(
    const ExprVisitor &visitor) const {
  // Walk directly the result symbol shape (the characteristic shape may contain
  // descriptor inquiries to it that would fail to lower on the caller side).
  const Fortran::semantics::SubprogramDetails *interfaceDetails =
      getInterfaceDetails();
  if (interfaceDetails) {
    walkExtents(interfaceDetails->result(), visitor);
  } else {
    if (procRef.Rank() != 0)
      fir::emitFatalError(
          converter.getCurrentLocation(),
          "only scalar functions may not have an interface symbol");
  }
}

void Fortran::lower::CallerInterface::walkDummyArgumentExtents(
    const PassedEntity &passedEntity, const ExprVisitor &visitor) const {
  const Fortran::semantics::SubprogramDetails *interfaceDetails =
      getInterfaceDetails();
  if (!interfaceDetails)
    return;
  const Fortran::semantics::Symbol *dummy = getDummySymbol(passedEntity);
  assert(dummy && "dummy symbol was not set");
  walkExtents(*dummy, visitor);
}

bool Fortran::lower::CallerInterface::mustMapInterfaceSymbolsForResult() const {
  assert(characteristic && "characteristic was not computed");
  const std::optional<Fortran::evaluate::characteristics::FunctionResult>
      &result = characteristic->functionResult;
  if (!result || result->CanBeReturnedViaImplicitInterface() ||
      !getInterfaceDetails() || result->IsProcedurePointer())
    return false;
  bool allResultSpecExprConstant = true;
  auto visitor = [&](const Fortran::lower::SomeExpr &e, bool) {
    allResultSpecExprConstant &= Fortran::evaluate::IsConstantExpr(e);
  };
  walkResultLengths(visitor);
  walkResultExtents(visitor);
  return !allResultSpecExprConstant;
}

bool Fortran::lower::CallerInterface::mustMapInterfaceSymbolsForDummyArgument(
    const PassedEntity &arg) const {
  bool allResultSpecExprConstant = true;
  auto visitor = [&](const Fortran::lower::SomeExpr &e, bool) {
    allResultSpecExprConstant &= Fortran::evaluate::IsConstantExpr(e);
  };
  walkDummyArgumentLengths(arg, visitor);
  walkDummyArgumentExtents(arg, visitor);
  return !allResultSpecExprConstant;
}

mlir::Value Fortran::lower::CallerInterface::getArgumentValue(
    const semantics::Symbol &sym) const {
  mlir::Location loc = converter.getCurrentLocation();
  const Fortran::semantics::SubprogramDetails *ifaceDetails =
      getInterfaceDetails();
  if (!ifaceDetails)
    fir::emitFatalError(
        loc, "mapping actual and dummy arguments requires an interface");
  const std::vector<Fortran::semantics::Symbol *> &dummies =
      ifaceDetails->dummyArgs();
  auto it = std::find(dummies.begin(), dummies.end(), &sym);
  if (it == dummies.end())
    fir::emitFatalError(loc, "symbol is not a dummy in this call");
  FirValue mlirArgIndex = passedArguments[it - dummies.begin()].firArgument;
  return actualInputs[mlirArgIndex];
}

const Fortran::semantics::Symbol *
Fortran::lower::CallerInterface::getDummySymbol(
    const PassedEntity &passedEntity) const {
  const Fortran::semantics::SubprogramDetails *ifaceDetails =
      getInterfaceDetails();
  if (!ifaceDetails)
    return nullptr;
  std::size_t argPosition = 0;
  for (const auto &arg : getPassedArguments()) {
    if (&arg == &passedEntity)
      break;
    ++argPosition;
  }
  if (argPosition >= ifaceDetails->dummyArgs().size())
    return nullptr;
  return ifaceDetails->dummyArgs()[argPosition];
}

mlir::Type Fortran::lower::CallerInterface::getResultStorageType() const {
  if (passedResult)
    return fir::dyn_cast_ptrEleTy(inputs[passedResult->firArgument].type);
  assert(saveResult && !outputs.empty());
  return outputs[0].type;
}

mlir::Type Fortran::lower::CallerInterface::getDummyArgumentType(
    const PassedEntity &passedEntity) const {
  return inputs[passedEntity.firArgument].type;
}

const Fortran::semantics::Symbol &
Fortran::lower::CallerInterface::getResultSymbol() const {
  mlir::Location loc = converter.getCurrentLocation();
  const Fortran::semantics::SubprogramDetails *ifaceDetails =
      getInterfaceDetails();
  if (!ifaceDetails)
    fir::emitFatalError(
        loc, "mapping actual and dummy arguments requires an interface");
  return ifaceDetails->result();
}

const Fortran::semantics::SubprogramDetails *
Fortran::lower::CallerInterface::getInterfaceDetails() const {
  if (const Fortran::semantics::Symbol *iface =
          procRef.proc().GetInterfaceSymbol())
    return iface->GetUltimate()
        .detailsIf<Fortran::semantics::SubprogramDetails>();
  return nullptr;
}

//===----------------------------------------------------------------------===//
// Callee side interface implementation
//===----------------------------------------------------------------------===//

bool Fortran::lower::CalleeInterface::hasAlternateReturns() const {
  return !funit.isMainProgram() &&
         Fortran::semantics::HasAlternateReturns(funit.getSubprogramSymbol());
}

std::string Fortran::lower::CalleeInterface::getMangledName() const {
  if (funit.isMainProgram())
    return fir::NameUniquer::doProgramEntry().str();
  return converter.mangleName(funit.getSubprogramSymbol());
}

const Fortran::semantics::Symbol *
Fortran::lower::CalleeInterface::getProcedureSymbol() const {
  if (funit.isMainProgram())
    return funit.getMainProgramSymbol();
  return &funit.getSubprogramSymbol();
}

mlir::Location Fortran::lower::CalleeInterface::getCalleeLocation() const {
  // FIXME: do NOT use unknown for the anonymous PROGRAM case. We probably
  // should just stash the location in the funit regardless.
  return converter.genLocation(funit.getStartingSourceLoc());
}

Fortran::evaluate::characteristics::Procedure
Fortran::lower::CalleeInterface::characterize() const {
  Fortran::evaluate::FoldingContext &foldingContext =
      converter.getFoldingContext();
  std::optional<Fortran::evaluate::characteristics::Procedure> characteristic =
      Fortran::evaluate::characteristics::Procedure::Characterize(
          funit.getSubprogramSymbol(), foldingContext);
  assert(characteristic && "Fail to get characteristic from symbol");
  return *characteristic;
}

bool Fortran::lower::CalleeInterface::isMainProgram() const {
  return funit.isMainProgram();
}

mlir::func::FuncOp
Fortran::lower::CalleeInterface::addEntryBlockAndMapArguments() {
  // Check for bugs in the front end. The front end must not present multiple
  // definitions of the same procedure.
  if (!func.getBlocks().empty())
    fir::emitFatalError(func.getLoc(),
                        "cannot process subprogram that was already processed");

  // On the callee side, directly map the mlir::value argument of the function
  // block to the Fortran symbols.
  func.addEntryBlock();
  mapPassedEntities();
  return func;
}

bool Fortran::lower::CalleeInterface::hasHostAssociated() const {
  return funit.parentHasTupleHostAssoc();
}

mlir::Type Fortran::lower::CalleeInterface::getHostAssociatedTy() const {
  assert(hasHostAssociated());
  return funit.parentHostAssoc().getArgumentType(converter);
}

mlir::Value Fortran::lower::CalleeInterface::getHostAssociatedTuple() const {
  assert(hasHostAssociated() || !funit.getHostAssoc().empty());
  return converter.hostAssocTupleValue();
}

//===----------------------------------------------------------------------===//
// CallInterface implementation: this part is common to both caller and callee.
//===----------------------------------------------------------------------===//

static void addSymbolAttribute(mlir::func::FuncOp func,
                               const Fortran::semantics::Symbol &sym,
                               fir::FortranProcedureFlagsEnumAttr procAttrs,
                               mlir::MLIRContext &mlirContext) {
  const Fortran::semantics::Symbol &ultimate = sym.GetUltimate();
  // The link between an internal procedure and its host procedure is lost
  // in FIR if the host is BIND(C) since the internal mangling will not
  // allow retrieving the host bind(C) name, and therefore func.func symbol.
  // Preserve it as an attribute so that this can be later retrieved.
  if (Fortran::semantics::ClassifyProcedure(ultimate) ==
      Fortran::semantics::ProcedureDefinitionClass::Internal) {
    if (ultimate.owner().kind() ==
        Fortran::semantics::Scope::Kind::Subprogram) {
      if (const Fortran::semantics::Symbol *hostProcedure =
              ultimate.owner().symbol()) {
        std::string hostName = Fortran::lower::mangle::mangleName(
            *hostProcedure, /*keepExternalInScope=*/true);
        func->setAttr(
            fir::getHostSymbolAttrName(),
            mlir::SymbolRefAttr::get(
                &mlirContext, mlir::StringAttr::get(&mlirContext, hostName)));
      }
    } else if (ultimate.owner().kind() ==
               Fortran::semantics::Scope::Kind::MainProgram) {
      func->setAttr(fir::getHostSymbolAttrName(),
                    mlir::SymbolRefAttr::get(
                        &mlirContext,
                        mlir::StringAttr::get(
                            &mlirContext, fir::NameUniquer::doProgramEntry())));
    }
  }

  if (procAttrs)
    func->setAttr(fir::getFortranProcedureFlagsAttrName(), procAttrs);

  // Only add this on bind(C) functions for which the symbol is not reflected in
  // the current context.
  if (!Fortran::semantics::IsBindCProcedure(sym))
    return;
  std::string name =
      Fortran::lower::mangle::mangleName(sym, /*keepExternalInScope=*/true);
  func->setAttr(fir::getSymbolAttrName(),
                mlir::StringAttr::get(&mlirContext, name));
}

static void
setCUDAAttributes(mlir::func::FuncOp func,
                  const Fortran::semantics::Symbol *sym,
                  std::optional<Fortran::evaluate::characteristics::Procedure>
                      characteristic) {
  if (characteristic && characteristic->cudaSubprogramAttrs) {
    func.getOperation()->setAttr(
        cuf::getProcAttrName(),
        cuf::getProcAttribute(func.getContext(),
                              *characteristic->cudaSubprogramAttrs));
  }

  if (sym) {
    if (auto details =
            sym->GetUltimate()
                .detailsIf<Fortran::semantics::SubprogramDetails>()) {
      mlir::Type i64Ty = mlir::IntegerType::get(func.getContext(), 64);
      if (!details->cudaLaunchBounds().empty()) {
        assert(details->cudaLaunchBounds().size() >= 2 &&
               "expect at least 2 values");
        auto maxTPBAttr =
            mlir::IntegerAttr::get(i64Ty, details->cudaLaunchBounds()[0]);
        auto minBPMAttr =
            mlir::IntegerAttr::get(i64Ty, details->cudaLaunchBounds()[1]);
        mlir::IntegerAttr ubAttr;
        if (details->cudaLaunchBounds().size() > 2)
          ubAttr =
              mlir::IntegerAttr::get(i64Ty, details->cudaLaunchBounds()[2]);
        func.getOperation()->setAttr(
            cuf::getLaunchBoundsAttrName(),
            cuf::LaunchBoundsAttr::get(func.getContext(), maxTPBAttr,
                                       minBPMAttr, ubAttr));
      }

      if (!details->cudaClusterDims().empty()) {
        assert(details->cudaClusterDims().size() == 3 && "expect 3 values");
        auto xAttr =
            mlir::IntegerAttr::get(i64Ty, details->cudaClusterDims()[0]);
        auto yAttr =
            mlir::IntegerAttr::get(i64Ty, details->cudaClusterDims()[1]);
        auto zAttr =
            mlir::IntegerAttr::get(i64Ty, details->cudaClusterDims()[2]);
        func.getOperation()->setAttr(
            cuf::getClusterDimsAttrName(),
            cuf::ClusterDimsAttr::get(func.getContext(), xAttr, yAttr, zAttr));
      }
    }
  }
}

/// Declare drives the different actions to be performed while analyzing the
/// signature and building/finding the mlir::func::FuncOp.
template <typename T>
void Fortran::lower::CallInterface<T>::declare() {
  if (!side().isMainProgram()) {
    characteristic.emplace(side().characterize());
    bool isImplicit = characteristic->CanBeCalledViaImplicitInterface();
    determineInterface(isImplicit, *characteristic);
  }
  // No input/output for main program

  // Create / get funcOp for direct calls. For indirect calls (only meaningful
  // on the caller side), no funcOp has to be created here. The mlir::Value
  // holding the indirection is used when creating the fir::CallOp.
  if (!side().isIndirectCall()) {
    std::string name = side().getMangledName();
    mlir::ModuleOp module = converter.getModuleOp();
    mlir::SymbolTable *symbolTable = converter.getMLIRSymbolTable();
    func = fir::FirOpBuilder::getNamedFunction(module, symbolTable, name);
    if (!func) {
      mlir::Location loc = side().getCalleeLocation();
      mlir::MLIRContext &mlirContext = converter.getMLIRContext();
      mlir::FunctionType ty = genFunctionType();
      func =
          fir::FirOpBuilder::createFunction(loc, module, name, ty, symbolTable);
      if (const Fortran::semantics::Symbol *sym = side().getProcedureSymbol()) {
        if (side().isMainProgram()) {
          func->setAttr(fir::getSymbolAttrName(),
                        mlir::StringAttr::get(&converter.getMLIRContext(),
                                              sym->name().ToString()));
        } else {
          addSymbolAttribute(func, *sym, getProcedureAttrs(&mlirContext),
                             mlirContext);
        }
      }
      for (const auto &placeHolder : llvm::enumerate(inputs))
        if (!placeHolder.value().attributes.empty())
          func.setArgAttrs(placeHolder.index(), placeHolder.value().attributes);

      setCUDAAttributes(func, side().getProcedureSymbol(), characteristic);

      if (const Fortran::semantics::Symbol *sym = side().getProcedureSymbol()) {
        if (const auto &info{
                sym->GetUltimate()
                    .detailsIf<Fortran::semantics::SubprogramDetails>()}) {
          if (!info->openACCRoutineInfos().empty()) {
            genOpenACCRoutineConstruct(converter, module, func,
                                       info->openACCRoutineInfos());
          }
        }
      }
    }
  }
}

/// Once the signature has been analyzed and the mlir::func::FuncOp was
/// built/found, map the fir inputs to Fortran entities (the symbols or
/// expressions).
template <typename T>
void Fortran::lower::CallInterface<T>::mapPassedEntities() {
  // map back fir inputs to passed entities
  if constexpr (std::is_same_v<T, Fortran::lower::CalleeInterface>) {
    assert(inputs.size() == func.front().getArguments().size() &&
           "function previously created with different number of arguments");
    for (auto [fst, snd] : llvm::zip(inputs, func.front().getArguments()))
      mapBackInputToPassedEntity(fst, snd);
  } else {
    // On the caller side, map the index of the mlir argument position
    // to Fortran ActualArguments.
    int firPosition = 0;
    for (const FirPlaceHolder &placeHolder : inputs)
      mapBackInputToPassedEntity(placeHolder, firPosition++);
  }
}

template <typename T>
void Fortran::lower::CallInterface<T>::mapBackInputToPassedEntity(
    const FirPlaceHolder &placeHolder, FirValue firValue) {
  PassedEntity &passedEntity =
      placeHolder.passedEntityPosition == FirPlaceHolder::resultEntityPosition
          ? passedResult.value()
          : passedArguments[placeHolder.passedEntityPosition];
  if (placeHolder.property == Property::CharLength)
    passedEntity.firLength = firValue;
  else
    passedEntity.firArgument = firValue;
}

/// Helpers to access ActualArgument/Symbols
static const Fortran::evaluate::ActualArguments &
getEntityContainer(const Fortran::evaluate::ProcedureRef &proc) {
  return proc.arguments();
}

static const std::vector<Fortran::semantics::Symbol *> &
getEntityContainer(Fortran::lower::pft::FunctionLikeUnit &funit) {
  return funit.getSubprogramSymbol()
      .get<Fortran::semantics::SubprogramDetails>()
      .dummyArgs();
}

static const Fortran::evaluate::ActualArgument *getDataObjectEntity(
    const std::optional<Fortran::evaluate::ActualArgument> &arg) {
  if (arg)
    return &*arg;
  return nullptr;
}

static const Fortran::semantics::Symbol &
getDataObjectEntity(const Fortran::semantics::Symbol *arg) {
  assert(arg && "expect symbol for data object entity");
  return *arg;
}

static const Fortran::evaluate::ActualArgument *
getResultEntity(const Fortran::evaluate::ProcedureRef &) {
  return nullptr;
}

static const Fortran::semantics::Symbol &
getResultEntity(Fortran::lower::pft::FunctionLikeUnit &funit) {
  return funit.getSubprogramSymbol()
      .get<Fortran::semantics::SubprogramDetails>()
      .result();
}

/// Bypass helpers to manipulate entities since they are not any symbol/actual
/// argument to associate. See SignatureBuilder below.
using FakeEntity = bool;
using FakeEntities = llvm::SmallVector<FakeEntity>;
static FakeEntities
getEntityContainer(const Fortran::evaluate::characteristics::Procedure &proc) {
  FakeEntities enities(proc.dummyArguments.size());
  return enities;
}
static const FakeEntity &getDataObjectEntity(const FakeEntity &e) { return e; }
static FakeEntity
getResultEntity(const Fortran::evaluate::characteristics::Procedure &proc) {
  return false;
}

/// This is the actual part that defines the FIR interface based on the
/// characteristic. It directly mutates the CallInterface members.
template <typename T>
class Fortran::lower::CallInterfaceImpl {
  using CallInterface = Fortran::lower::CallInterface<T>;
  using PassEntityBy = typename CallInterface::PassEntityBy;
  using PassedEntity = typename CallInterface::PassedEntity;
  using FirValue = typename CallInterface::FirValue;
  using FortranEntity = typename CallInterface::FortranEntity;
  using FirPlaceHolder = typename CallInterface::FirPlaceHolder;
  using Property = typename CallInterface::Property;
  using TypeAndShape = Fortran::evaluate::characteristics::TypeAndShape;
  using DummyCharacteristics =
      Fortran::evaluate::characteristics::DummyArgument;

public:
  CallInterfaceImpl(CallInterface &i)
      : interface(i), mlirContext{i.converter.getMLIRContext()} {}

  void buildImplicitInterface(
      const Fortran::evaluate::characteristics::Procedure &procedure) {
    // Handle result
    if (const std::optional<Fortran::evaluate::characteristics::FunctionResult>
            &result = procedure.functionResult)
      handleImplicitResult(*result, procedure.IsBindC());
    else if (interface.side().hasAlternateReturns())
      addFirResult(mlir::IndexType::get(&mlirContext),
                   FirPlaceHolder::resultEntityPosition, Property::Value);
    // Handle arguments
    const auto &argumentEntities =
        getEntityContainer(interface.side().getCallDescription());
    for (auto pair : llvm::zip(procedure.dummyArguments, argumentEntities)) {
      const Fortran::evaluate::characteristics::DummyArgument
          &argCharacteristics = std::get<0>(pair);
      Fortran::common::visit(
          Fortran::common::visitors{
              [&](const auto &dummy) {
                const auto &entity = getDataObjectEntity(std::get<1>(pair));
                handleImplicitDummy(&argCharacteristics, dummy, entity);
              },
              [&](const Fortran::evaluate::characteristics::AlternateReturn &) {
                // nothing to do
              },
          },
          argCharacteristics.u);
    }
  }

  void buildExplicitInterface(
      const Fortran::evaluate::characteristics::Procedure &procedure) {
    bool isBindC = procedure.IsBindC();
    // Handle result
    if (const std::optional<Fortran::evaluate::characteristics::FunctionResult>
            &result = procedure.functionResult) {
      if (result->CanBeReturnedViaImplicitInterface())
        handleImplicitResult(*result, isBindC);
      else
        handleExplicitResult(*result);
    } else if (interface.side().hasAlternateReturns()) {
      addFirResult(mlir::IndexType::get(&mlirContext),
                   FirPlaceHolder::resultEntityPosition, Property::Value);
    }
    // Handle arguments
    const auto &argumentEntities =
        getEntityContainer(interface.side().getCallDescription());
    for (auto pair : llvm::zip(procedure.dummyArguments, argumentEntities)) {
      const Fortran::evaluate::characteristics::DummyArgument
          &argCharacteristics = std::get<0>(pair);
      Fortran::common::visit(
          Fortran::common::visitors{
              [&](const Fortran::evaluate::characteristics::DummyDataObject
                      &dummy) {
                const auto &entity = getDataObjectEntity(std::get<1>(pair));
                if (!isBindC && dummy.CanBePassedViaImplicitInterface())
                  handleImplicitDummy(&argCharacteristics, dummy, entity);
                else
                  handleExplicitDummy(&argCharacteristics, dummy, entity,
                                      isBindC);
              },
              [&](const Fortran::evaluate::characteristics::DummyProcedure
                      &dummy) {
                const auto &entity = getDataObjectEntity(std::get<1>(pair));
                handleImplicitDummy(&argCharacteristics, dummy, entity);
              },
              [&](const Fortran::evaluate::characteristics::AlternateReturn &) {
                // nothing to do
              },
          },
          argCharacteristics.u);
    }
  }

  void appendHostAssocTupleArg(mlir::Type tupTy) {
    mlir::MLIRContext *ctxt = tupTy.getContext();
    addFirOperand(tupTy, nextPassedArgPosition(), Property::BaseAddress,
                  {mlir::NamedAttribute{
                      mlir::StringAttr::get(ctxt, fir::getHostAssocAttrName()),
                      mlir::UnitAttr::get(ctxt)}});
    interface.passedArguments.emplace_back(
        PassedEntity{PassEntityBy::BaseAddress, std::nullopt,
                     interface.side().getHostAssociatedTuple(), emptyValue()});
  }

  static std::optional<Fortran::evaluate::DynamicType> getResultDynamicType(
      const Fortran::evaluate::characteristics::Procedure &procedure) {
    if (const std::optional<Fortran::evaluate::characteristics::FunctionResult>
            &result = procedure.functionResult)
      if (const auto *resultTypeAndShape = result->GetTypeAndShape())
        return resultTypeAndShape->type();
    return std::nullopt;
  }

  static bool mustPassLengthWithDummyProcedure(
      const Fortran::evaluate::characteristics::Procedure &procedure) {
    // When passing a character function designator `bar` as dummy procedure to
    // `foo` (e.g. `foo(bar)`), pass the result length of `bar` to `foo` so that
    // `bar` can be called inside `foo` even if its length is assumed there.
    // From an ABI perspective, the extra length argument must be handled
    // exactly as if passing a character object. Using an argument of
    // fir.boxchar type gives the expected behavior: after codegen, the
    // fir.boxchar lengths are added after all the arguments as extra value
    // arguments (the extra arguments order is the order of the fir.boxchar).

    // This ABI is compatible with ifort, nag, nvfortran, and xlf, but not
    // gfortran. Gfortran does not pass the length and is therefore unable to
    // handle later call to `bar` in `foo` where the length would be assumed. If
    // the result is an array, nag and ifort and xlf still pass the length, but
    // not nvfortran (and gfortran). It is not clear it is possible to call an
    // array function with assumed length (f18 forbides defining such
    // interfaces). Hence, passing the length is most likely useless, but stick
    // with ifort/nag/xlf interface here.
    if (std::optional<Fortran::evaluate::DynamicType> type =
            getResultDynamicType(procedure))
      return type->category() == Fortran::common::TypeCategory::Character;
    return false;
  }

private:
  void handleImplicitResult(
      const Fortran::evaluate::characteristics::FunctionResult &result,
      bool isBindC) {
    if (auto proc{result.IsProcedurePointer()}) {
      mlir::Type mlirType = fir::BoxProcType::get(
          &mlirContext, getProcedureType(*proc, interface.converter));
      addFirResult(mlirType, FirPlaceHolder::resultEntityPosition,
                   Property::Value);
      return;
    }
    const Fortran::evaluate::characteristics::TypeAndShape *typeAndShape =
        result.GetTypeAndShape();
    assert(typeAndShape && "expect type for non proc pointer result");
    Fortran::evaluate::DynamicType dynamicType = typeAndShape->type();
    // Character result allocated by caller and passed as hidden arguments
    if (dynamicType.category() == Fortran::common::TypeCategory::Character) {
      if (isBindC) {
        mlir::Type mlirType = translateDynamicType(dynamicType);
        addFirResult(mlirType, FirPlaceHolder::resultEntityPosition,
                     Property::Value);
      } else {
        handleImplicitCharacterResult(dynamicType);
      }
    } else if (dynamicType.category() ==
               Fortran::common::TypeCategory::Derived) {
      if (!dynamicType.GetDerivedTypeSpec().IsVectorType()) {
        // Derived result need to be allocated by the caller and the result
        // value must be saved. Derived type in implicit interface cannot have
        // length parameters.
        setSaveResult();
      }
      mlir::Type mlirType = translateDynamicType(dynamicType);
      addFirResult(mlirType, FirPlaceHolder::resultEntityPosition,
                   Property::Value);
    } else {
      // All result other than characters/derived are simply returned by value
      // in implicit interfaces
      mlir::Type mlirType =
          getConverter().genType(dynamicType.category(), dynamicType.kind());
      addFirResult(mlirType, FirPlaceHolder::resultEntityPosition,
                   Property::Value);
    }
  }
  void
  handleImplicitCharacterResult(const Fortran::evaluate::DynamicType &type) {
    int resultPosition = FirPlaceHolder::resultEntityPosition;
    setPassedResult(PassEntityBy::AddressAndLength,
                    getResultEntity(interface.side().getCallDescription()));
    mlir::Type lenTy = mlir::IndexType::get(&mlirContext);
    std::optional<std::int64_t> constantLen = type.knownLength();
    fir::CharacterType::LenType len =
        constantLen ? *constantLen : fir::CharacterType::unknownLen();
    mlir::Type charRefTy = fir::ReferenceType::get(
        fir::CharacterType::get(&mlirContext, type.kind(), len));
    mlir::Type boxCharTy = fir::BoxCharType::get(&mlirContext, type.kind());
    addFirOperand(charRefTy, resultPosition, Property::CharAddress);
    addFirOperand(lenTy, resultPosition, Property::CharLength);
    /// For now, also return it by boxchar
    addFirResult(boxCharTy, resultPosition, Property::BoxChar);
  }

  /// Return a vector with an attribute with the name of the argument if this
  /// is a callee interface and the name is available. Otherwise, just return
  /// an empty vector.
  llvm::SmallVector<mlir::NamedAttribute>
  dummyNameAttr(const FortranEntity &entity) {
    if constexpr (std::is_same_v<FortranEntity,
                                 std::optional<Fortran::common::Reference<
                                     const Fortran::semantics::Symbol>>>) {
      if (entity.has_value()) {
        const Fortran::semantics::Symbol *argument = &*entity.value();
        // "fir.bindc_name" is used for arguments for the sake of consistency
        // with other attributes carrying surface syntax names in FIR.
        return {mlir::NamedAttribute(
            mlir::StringAttr::get(&mlirContext, "fir.bindc_name"),
            mlir::StringAttr::get(&mlirContext,
                                  toStringRef(argument->name())))};
      }
    }
    return {};
  }

  mlir::Type
  getRefType(Fortran::evaluate::DynamicType dynamicType,
             const Fortran::evaluate::characteristics::DummyDataObject &obj) {
    mlir::Type type = translateDynamicType(dynamicType);
    if (std::optional<fir::SequenceType::Shape> bounds = getBounds(obj.type))
      type = fir::SequenceType::get(*bounds, type);
    return fir::ReferenceType::get(type);
  }

  void handleImplicitDummy(
      const DummyCharacteristics *characteristics,
      const Fortran::evaluate::characteristics::DummyDataObject &obj,
      const FortranEntity &entity) {
    Fortran::evaluate::DynamicType dynamicType = obj.type.type();
    if constexpr (std::is_same_v<FortranEntity,
                                 const Fortran::evaluate::ActualArgument *>) {
      if (entity) {
        if (entity->isPercentVal()) {
          mlir::Type type = translateDynamicType(dynamicType);
          addFirOperand(type, nextPassedArgPosition(), Property::Value,
                        dummyNameAttr(entity));
          addPassedArg(PassEntityBy::Value, entity, characteristics);
          return;
        }
        if (entity->isPercentRef()) {
          mlir::Type refType = getRefType(dynamicType, obj);
          addFirOperand(refType, nextPassedArgPosition(), Property::BaseAddress,
                        dummyNameAttr(entity));
          addPassedArg(PassEntityBy::BaseAddress, entity, characteristics);
          return;
        }
      }
    }
    if (dynamicType.category() == Fortran::common::TypeCategory::Character) {
      mlir::Type boxCharTy =
          fir::BoxCharType::get(&mlirContext, dynamicType.kind());
      addFirOperand(boxCharTy, nextPassedArgPosition(), Property::BoxChar,
                    dummyNameAttr(entity));
      addPassedArg(PassEntityBy::BoxChar, entity, characteristics);
    } else {
      // non-PDT derived type allowed in implicit interface.
      mlir::Type refType = getRefType(dynamicType, obj);
      addFirOperand(refType, nextPassedArgPosition(), Property::BaseAddress,
                    dummyNameAttr(entity));
      addPassedArg(PassEntityBy::BaseAddress, entity, characteristics);
    }
  }

  mlir::Type
  translateDynamicType(const Fortran::evaluate::DynamicType &dynamicType) {
    Fortran::common::TypeCategory cat = dynamicType.category();
    // DERIVED
    if (cat == Fortran::common::TypeCategory::Derived) {
      if (dynamicType.IsUnlimitedPolymorphic())
        return mlir::NoneType::get(&mlirContext);
      return getConverter().genType(dynamicType.GetDerivedTypeSpec());
    }
    // CHARACTER with compile time constant length.
    if (cat == Fortran::common::TypeCategory::Character)
      if (std::optional<std::int64_t> constantLen =
              toInt64(dynamicType.GetCharLength()))
        return getConverter().genType(cat, dynamicType.kind(), {*constantLen});
    // INTEGER, REAL, LOGICAL, COMPLEX, and CHARACTER with dynamic length.
    return getConverter().genType(cat, dynamicType.kind());
  }

  void handleExplicitDummy(
      const DummyCharacteristics *characteristics,
      const Fortran::evaluate::characteristics::DummyDataObject &obj,
      const FortranEntity &entity, bool isBindC) {
    using Attrs = Fortran::evaluate::characteristics::DummyDataObject::Attr;

    bool isValueAttr = false;
    [[maybe_unused]] mlir::Location loc =
        interface.converter.getCurrentLocation();
    llvm::SmallVector<mlir::NamedAttribute> attrs = dummyNameAttr(entity);
    auto addMLIRAttr = [&](llvm::StringRef attr) {
      attrs.emplace_back(mlir::StringAttr::get(&mlirContext, attr),
                         mlir::UnitAttr::get(&mlirContext));
    };
    if (obj.attrs.test(Attrs::Optional))
      addMLIRAttr(fir::getOptionalAttrName());
    if (obj.attrs.test(Attrs::Contiguous))
      addMLIRAttr(fir::getContiguousAttrName());
    if (obj.attrs.test(Attrs::Value))
      isValueAttr = true; // TODO: do we want an mlir::Attribute as well?

    // obj.attrs.test(Attrs::Asynchronous) does not impact the way the argument
    // is passed given flang implement asynch IO synchronously. However, it's
    // added to determine whether the argument is captured.
    // TODO: it would be safer to treat them as volatile because since Fortran
    // 2018 asynchronous can also be used for C defined asynchronous user
    // processes (see 18.10.4 Asynchronous communication).
    if (obj.attrs.test(Attrs::Asynchronous))
      addMLIRAttr(fir::getAsynchronousAttrName());
    if (obj.attrs.test(Attrs::Target))
      addMLIRAttr(fir::getTargetAttrName());
    if (obj.cudaDataAttr)
      attrs.emplace_back(
          mlir::StringAttr::get(&mlirContext, cuf::getDataAttrName()),
          cuf::getDataAttribute(&mlirContext, obj.cudaDataAttr));

    // TODO: intents that require special care (e.g finalization)

    if (obj.type.corank() > 0)
      TODO(loc, "coarray: dummy argument coarray in procedure interface");

    // So far assume that if the argument cannot be passed by implicit interface
    // it must be by box. That may no be always true (e.g for simple optionals)

    Fortran::evaluate::DynamicType dynamicType = obj.type.type();
    mlir::Type type = translateDynamicType(dynamicType);
    if (std::optional<fir::SequenceType::Shape> bounds = getBounds(obj.type))
      type = fir::SequenceType::get(*bounds, type);
    if (obj.attrs.test(Attrs::Allocatable))
      type = fir::HeapType::get(type);
    if (obj.attrs.test(Attrs::Pointer))
      type = fir::PointerType::get(type);
    mlir::Type boxType = fir::wrapInClassOrBoxType(
        type, obj.type.type().IsPolymorphic(), obj.type.type().IsAssumedType());

    if (obj.attrs.test(Attrs::Allocatable) || obj.attrs.test(Attrs::Pointer)) {
      // Pass as fir.ref<fir.box> or fir.ref<fir.class>
      const bool isVolatile = obj.attrs.test(Attrs::Volatile);
      mlir::Type boxRefType = fir::ReferenceType::get(boxType, isVolatile);
      addFirOperand(boxRefType, nextPassedArgPosition(), Property::MutableBox,
                    attrs);
      addPassedArg(PassEntityBy::MutableBox, entity, characteristics);
    } else if (obj.IsPassedByDescriptor(isBindC)) {
      // Pass as fir.box or fir.class
      if (isValueAttr &&
          !getConverter().getLoweringOptions().getLowerToHighLevelFIR())
        TODO(loc, "assumed shape dummy argument with VALUE attribute");
      addFirOperand(boxType, nextPassedArgPosition(), Property::Box, attrs);
      addPassedArg(PassEntityBy::Box, entity, characteristics);
    } else if (dynamicType.category() ==
               Fortran::common::TypeCategory::Character) {
      if (isValueAttr && isBindC) {
        // Pass as fir.char<1>
        mlir::Type charTy =
            fir::CharacterType::getSingleton(&mlirContext, dynamicType.kind());
        addFirOperand(charTy, nextPassedArgPosition(), Property::Value, attrs);
        addPassedArg(PassEntityBy::Value, entity, characteristics);
      } else {
        // Pass as fir.box_char
        mlir::Type boxCharTy =
            fir::BoxCharType::get(&mlirContext, dynamicType.kind());
        addFirOperand(boxCharTy, nextPassedArgPosition(), Property::BoxChar,
                      attrs);
        addPassedArg(isValueAttr ? PassEntityBy::CharBoxValueAttribute
                                 : PassEntityBy::BoxChar,
                     entity, characteristics);
      }
    } else {
      // Pass as fir.ref unless it's by VALUE and BIND(C). Also pass-by-value
      // for numerical/logical scalar without OPTIONAL so that the behavior is
      // consistent with gfortran/nvfortran.
      // TODO: pass-by-value for derived type is not supported yet
      mlir::Type passType = fir::ReferenceType::get(type);
      PassEntityBy passBy = PassEntityBy::BaseAddress;
      Property prop = Property::BaseAddress;
      if (isValueAttr) {
        bool isBuiltinCptrType = fir::isa_builtin_cptr_type(type);
        if (isBindC || (!mlir::isa<fir::SequenceType>(type) &&
                        !obj.attrs.test(Attrs::Optional) &&
                        (dynamicType.category() !=
                             Fortran::common::TypeCategory::Derived ||
                         isBuiltinCptrType))) {
          passBy = PassEntityBy::Value;
          prop = Property::Value;
          if (isBuiltinCptrType) {
            auto recTy = mlir::dyn_cast<fir::RecordType>(type);
            mlir::Type fieldTy = recTy.getTypeList()[0].second;
            passType = fir::ReferenceType::get(fieldTy);
          } else {
            passType = type;
          }
        } else {
          passBy = PassEntityBy::BaseAddressValueAttribute;
        }
      }
      addFirOperand(passType, nextPassedArgPosition(), prop, attrs);
      addPassedArg(passBy, entity, characteristics);
    }
  }

  void handleImplicitDummy(
      const DummyCharacteristics *characteristics,
      const Fortran::evaluate::characteristics::DummyProcedure &proc,
      const FortranEntity &entity) {
    if (!interface.converter.getLoweringOptions().getLowerToHighLevelFIR() &&
        proc.attrs.test(
            Fortran::evaluate::characteristics::DummyProcedure::Attr::Pointer))
      TODO(interface.converter.getCurrentLocation(),
           "procedure pointer arguments");
    const Fortran::evaluate::characteristics::Procedure &procedure =
        proc.procedure.value();
    mlir::Type funcType =
        getProcedureDesignatorType(&procedure, interface.converter);
    if (proc.attrs.test(Fortran::evaluate::characteristics::DummyProcedure::
                            Attr::Pointer)) {
      // Prodecure pointer dummy argument.
      funcType = fir::ReferenceType::get(funcType);
      addFirOperand(funcType, nextPassedArgPosition(), Property::BoxProcRef);
      addPassedArg(PassEntityBy::BoxProcRef, entity, characteristics);
      return;
    }
    // Otherwise, it is a dummy procedure.
    std::optional<Fortran::evaluate::DynamicType> resultTy =
        getResultDynamicType(procedure);
    if (resultTy && mustPassLengthWithDummyProcedure(procedure)) {
      // The result length of dummy procedures that are character functions must
      // be passed so that the dummy procedure can be called if it has assumed
      // length on the callee side.
      mlir::Type tupleType =
          fir::factory::getCharacterProcedureTupleType(funcType);
      llvm::StringRef charProcAttr = fir::getCharacterProcedureDummyAttrName();
      addFirOperand(tupleType, nextPassedArgPosition(), Property::CharProcTuple,
                    {mlir::NamedAttribute{
                        mlir::StringAttr::get(&mlirContext, charProcAttr),
                        mlir::UnitAttr::get(&mlirContext)}});
      addPassedArg(PassEntityBy::CharProcTuple, entity, characteristics);
      return;
    }
    addFirOperand(funcType, nextPassedArgPosition(), Property::BaseAddress);
    addPassedArg(PassEntityBy::BaseAddress, entity, characteristics);
  }

  void handleExplicitResult(
      const Fortran::evaluate::characteristics::FunctionResult &result) {
    using Attr = Fortran::evaluate::characteristics::FunctionResult::Attr;
    mlir::Type mlirType;
    if (auto proc{result.IsProcedurePointer()}) {
      mlirType = fir::BoxProcType::get(
          &mlirContext, getProcedureType(*proc, interface.converter));
      addFirResult(mlirType, FirPlaceHolder::resultEntityPosition,
                   Property::Value);
      return;
    }
    const Fortran::evaluate::characteristics::TypeAndShape *typeAndShape =
        result.GetTypeAndShape();
    assert(typeAndShape && "expect type for non proc pointer result");
    mlirType = translateDynamicType(typeAndShape->type());
    const auto *resTypeAndShape{result.GetTypeAndShape()};
    bool resIsPolymorphic =
        resTypeAndShape && resTypeAndShape->type().IsPolymorphic();
    bool resIsAssumedType =
        resTypeAndShape && resTypeAndShape->type().IsAssumedType();
    if (std::optional<fir::SequenceType::Shape> bounds =
            getBounds(*typeAndShape))
      mlirType = fir::SequenceType::get(*bounds, mlirType);
    if (result.attrs.test(Attr::Allocatable))
      mlirType = fir::wrapInClassOrBoxType(fir::HeapType::get(mlirType),
                                           resIsPolymorphic, resIsAssumedType);
    if (result.attrs.test(Attr::Pointer))
      mlirType = fir::wrapInClassOrBoxType(fir::PointerType::get(mlirType),
                                           resIsPolymorphic, resIsAssumedType);

    if (fir::isa_char(mlirType)) {
      // Character scalar results must be passed as arguments in lowering so
      // that an assumed length character function callee can access the
      // result length. A function with a result requiring an explicit
      // interface does not have to be compatible with assumed length
      // function, but most compilers supports it.
      handleImplicitCharacterResult(typeAndShape->type());
      return;
    }

    addFirResult(mlirType, FirPlaceHolder::resultEntityPosition,
                 Property::Value);
    // Explicit results require the caller to allocate the storage and save the
    // function result in the storage with a fir.save_result.
    setSaveResult();
  }

  // Return nullopt for scalars, empty vector for assumed rank, and a vector
  // with the shape (may contain unknown extents) for arrays.
  std::optional<fir::SequenceType::Shape> getBounds(
      const Fortran::evaluate::characteristics::TypeAndShape &typeAndShape) {
    if (typeAndShape.shape() && typeAndShape.shape()->empty())
      return std::nullopt;
    fir::SequenceType::Shape bounds;
    if (typeAndShape.shape())
      for (const std::optional<Fortran::evaluate::ExtentExpr> &extent :
           *typeAndShape.shape()) {
        fir::SequenceType::Extent bound = fir::SequenceType::getUnknownExtent();
        if (std::optional<std::int64_t> i = toInt64(extent))
          bound = *i;
        bounds.emplace_back(bound);
      }
    return bounds;
  }
  std::optional<std::int64_t>
  toInt64(std::optional<
          Fortran::evaluate::Expr<Fortran::evaluate::SubscriptInteger>>
              expr) {
    if (expr)
      return Fortran::evaluate::ToInt64(Fortran::evaluate::Fold(
          getConverter().getFoldingContext(), toEvExpr(*expr)));
    return std::nullopt;
  }
  void addFirOperand(mlir::Type type, int entityPosition, Property p,
                     llvm::ArrayRef<mlir::NamedAttribute> attributes = {}) {
    interface.inputs.emplace_back(
        FirPlaceHolder{type, entityPosition, p, attributes});
  }
  void addFirResult(mlir::Type type, int entityPosition, Property p,
                    llvm::ArrayRef<mlir::NamedAttribute> attributes = {}) {
    interface.outputs.emplace_back(
        FirPlaceHolder{type, entityPosition, p, attributes});
  }
  void addPassedArg(PassEntityBy p, FortranEntity entity,
                    const DummyCharacteristics *characteristics) {
    interface.passedArguments.emplace_back(
        PassedEntity{p, entity, emptyValue(), emptyValue(), characteristics});
  }
  void setPassedResult(PassEntityBy p, FortranEntity entity) {
    interface.passedResult =
        PassedEntity{p, entity, emptyValue(), emptyValue()};
  }
  void setSaveResult() { interface.saveResult = true; }
  int nextPassedArgPosition() { return interface.passedArguments.size(); }

  static FirValue emptyValue() {
    if constexpr (std::is_same_v<Fortran::lower::CalleeInterface, T>) {
      return {};
    } else {
      return -1;
    }
  }

  Fortran::lower::AbstractConverter &getConverter() {
    return interface.converter;
  }
  CallInterface &interface;
  mlir::MLIRContext &mlirContext;
};

template <typename T>
bool Fortran::lower::CallInterface<T>::PassedEntity::isOptional() const {
  if (!characteristics)
    return false;
  return characteristics->IsOptional();
}
template <typename T>
bool Fortran::lower::CallInterface<T>::PassedEntity::mayBeModifiedByCall()
    const {
  if (!characteristics)
    return true;
  if (characteristics->GetIntent() == Fortran::common::Intent::In)
    return false;
  return !hasValueAttribute();
}
template <typename T>
bool Fortran::lower::CallInterface<T>::PassedEntity::mayBeReadByCall() const {
  if (!characteristics)
    return true;
  return characteristics->GetIntent() != Fortran::common::Intent::Out;
}

template <typename T>
bool Fortran::lower::CallInterface<T>::PassedEntity::testTKR(
    Fortran::common::IgnoreTKR flag) const {
  if (!characteristics)
    return false;
  const auto *dummy =
      std::get_if<Fortran::evaluate::characteristics::DummyDataObject>(
          &characteristics->u);
  if (!dummy)
    return false;
  return dummy->ignoreTKR.test(flag);
}

template <typename T>
bool Fortran::lower::CallInterface<T>::PassedEntity::isIntentOut() const {
  if (!characteristics)
    return true;
  return characteristics->GetIntent() == Fortran::common::Intent::Out;
}
template <typename T>
bool Fortran::lower::CallInterface<T>::PassedEntity::mustBeMadeContiguous()
    const {
  if (!characteristics)
    return true;
  const auto *dummy =
      std::get_if<Fortran::evaluate::characteristics::DummyDataObject>(
          &characteristics->u);
  if (!dummy)
    return false;
  const auto &shapeAttrs = dummy->type.attrs();
  using ShapeAttrs = Fortran::evaluate::characteristics::TypeAndShape::Attr;
  if (shapeAttrs.test(ShapeAttrs::AssumedRank) ||
      shapeAttrs.test(ShapeAttrs::AssumedShape))
    return dummy->attrs.test(
        Fortran::evaluate::characteristics::DummyDataObject::Attr::Contiguous);
  if (shapeAttrs.test(ShapeAttrs::DeferredShape))
    return false;
  // Explicit shape arrays are contiguous.
  return dummy->type.Rank() > 0;
}

template <typename T>
bool Fortran::lower::CallInterface<T>::PassedEntity::hasValueAttribute() const {
  if (!characteristics)
    return false;
  const auto *dummy =
      std::get_if<Fortran::evaluate::characteristics::DummyDataObject>(
          &characteristics->u);
  return dummy &&
         dummy->attrs.test(
             Fortran::evaluate::characteristics::DummyDataObject::Attr::Value);
}

template <typename T>
bool Fortran::lower::CallInterface<T>::PassedEntity::hasAllocatableAttribute()
    const {
  if (!characteristics)
    return false;
  const auto *dummy =
      std::get_if<Fortran::evaluate::characteristics::DummyDataObject>(
          &characteristics->u);
  using Attrs = Fortran::evaluate::characteristics::DummyDataObject::Attr;
  return dummy && dummy->attrs.test(Attrs::Allocatable);
}

template <typename T>
bool Fortran::lower::CallInterface<
    T>::PassedEntity::mayRequireIntentoutFinalization() const {
  // Conservatively assume that the finalization is needed.
  if (!characteristics)
    return true;

  // No INTENT(OUT) dummy arguments do not require finalization on entry.
  if (!isIntentOut())
    return false;

  const auto *dummy =
      std::get_if<Fortran::evaluate::characteristics::DummyDataObject>(
          &characteristics->u);
  if (!dummy)
    return true;

  // POINTER/ALLOCATABLE dummy arguments do not require finalization.
  using Attrs = Fortran::evaluate::characteristics::DummyDataObject::Attr;
  if (dummy->attrs.test(Attrs::Allocatable) ||
      dummy->attrs.test(Attrs::Pointer))
    return false;

  // Polymorphic and unlimited polymorphic INTENT(OUT) dummy arguments
  // may need finalization.
  const Fortran::evaluate::DynamicType &type = dummy->type.type();
  if (type.IsPolymorphic() || type.IsUnlimitedPolymorphic())
    return true;

  // INTENT(OUT) dummy arguments of derived types require finalization,
  // if their type has finalization.
  const Fortran::semantics::DerivedTypeSpec *derived =
      Fortran::evaluate::GetDerivedTypeSpec(type);
  if (!derived)
    return false;

  return Fortran::semantics::IsFinalizable(*derived);
}

template <typename T>
bool Fortran::lower::CallInterface<
    T>::PassedEntity::isSequenceAssociatedDescriptor() const {
  if (!characteristics || passBy != PassEntityBy::Box)
    return false;
  const auto *dummy =
      std::get_if<Fortran::evaluate::characteristics::DummyDataObject>(
          &characteristics->u);
  return dummy && dummy->type.CanBeSequenceAssociated();
}

template <typename T>
void Fortran::lower::CallInterface<T>::determineInterface(
    bool isImplicit,
    const Fortran::evaluate::characteristics::Procedure &procedure) {
  CallInterfaceImpl<T> impl(*this);
  if (isImplicit)
    impl.buildImplicitInterface(procedure);
  else
    impl.buildExplicitInterface(procedure);
  // We only expect the extra host asspciations argument from the callee side as
  // the definition of internal procedures will be present, and we'll always
  // have a FuncOp definition in the ModuleOp, when lowering.
  if constexpr (std::is_same_v<T, Fortran::lower::CalleeInterface>) {
    if (side().hasHostAssociated())
      impl.appendHostAssocTupleArg(side().getHostAssociatedTy());
  }
}

template <typename T>
mlir::FunctionType Fortran::lower::CallInterface<T>::genFunctionType() {
  llvm::SmallVector<mlir::Type> returnTys;
  llvm::SmallVector<mlir::Type> inputTys;
  for (const FirPlaceHolder &placeHolder : outputs)
    returnTys.emplace_back(placeHolder.type);
  for (const FirPlaceHolder &placeHolder : inputs)
    inputTys.emplace_back(placeHolder.type);
  return mlir::FunctionType::get(&converter.getMLIRContext(), inputTys,
                                 returnTys);
}

template <typename T>
llvm::SmallVector<mlir::Type>
Fortran::lower::CallInterface<T>::getResultType() const {
  llvm::SmallVector<mlir::Type> types;
  for (const FirPlaceHolder &out : outputs)
    types.emplace_back(out.type);
  return types;
}

template <typename T>
fir::FortranProcedureFlagsEnumAttr
Fortran::lower::CallInterface<T>::getProcedureAttrs(
    mlir::MLIRContext *mlirContext) const {
  fir::FortranProcedureFlagsEnum flags = fir::FortranProcedureFlagsEnum::none;
  if (characteristic) {
    if (characteristic->IsBindC())
      flags = flags | fir::FortranProcedureFlagsEnum::bind_c;
    if (characteristic->IsPure())
      flags = flags | fir::FortranProcedureFlagsEnum::pure;
    if (characteristic->IsElemental())
      flags = flags | fir::FortranProcedureFlagsEnum::elemental;
    // TODO:
    // - SIMPLE: F2023, not yet handled by semantics.
  }

  if constexpr (std::is_same_v<Fortran::lower::CalleeInterface, T>) {
    // Only gather and set NON_RECURSIVE for procedure definition. It is
    // meaningless on calls since this is not part of Fortran characteristics
    // (Fortran 2023 15.3.1) so there is no way to always know if the procedure
    // called is recursive or not.
    if (const Fortran::semantics::Symbol *sym = side().getProcedureSymbol()) {
      // Note: By default procedures are RECURSIVE unless
      // -fno-automatic/-save/-Msave is set. NON_RECURSIVE is is made explicit
      // in that case in FIR.
      if (sym->attrs().test(Fortran::semantics::Attr::NON_RECURSIVE) ||
          (sym->owner().context().languageFeatures().IsEnabled(
               Fortran::common::LanguageFeature::DefaultSave) &&
           !sym->attrs().test(Fortran::semantics::Attr::RECURSIVE))) {
        flags = flags | fir::FortranProcedureFlagsEnum::non_recursive;
      }
    }
  }
  if (flags != fir::FortranProcedureFlagsEnum::none)
    return fir::FortranProcedureFlagsEnumAttr::get(mlirContext, flags);
  return nullptr;
}

template class Fortran::lower::CallInterface<Fortran::lower::CalleeInterface>;
template class Fortran::lower::CallInterface<Fortran::lower::CallerInterface>;

//===----------------------------------------------------------------------===//
// Function Type Translation
//===----------------------------------------------------------------------===//

/// Build signature from characteristics when there is no Fortran entity to
/// associate with the arguments (i.e, this is not a call site or a procedure
/// declaration. This is needed when dealing with function pointers/dummy
/// arguments.

class SignatureBuilder;
template <>
struct Fortran::lower::PassedEntityTypes<SignatureBuilder> {
  using FortranEntity = FakeEntity;
  using FirValue = int;
};

/// SignatureBuilder is a CRTP implementation of CallInterface intended to
/// help translating characteristics::Procedure to mlir::FunctionType using
/// the CallInterface translation.
class SignatureBuilder
    : public Fortran::lower::CallInterface<SignatureBuilder> {
public:
  SignatureBuilder(const Fortran::evaluate::characteristics::Procedure &p,
                   Fortran::lower::AbstractConverter &c, bool forceImplicit)
      : CallInterface{c}, proc{p} {
    bool isImplicit = forceImplicit || proc.CanBeCalledViaImplicitInterface();
    determineInterface(isImplicit, proc);
  }
  SignatureBuilder(const Fortran::evaluate::ProcedureDesignator &procDes,
                   Fortran::lower::AbstractConverter &c)
      : CallInterface{c}, procDesignator{&procDes},
        proc{Fortran::evaluate::characteristics::Procedure::Characterize(
                 procDes, converter.getFoldingContext(), /*emitError=*/false)
                 .value()} {}
  /// Does the procedure characteristics being translated have alternate
  /// returns ?
  bool hasAlternateReturns() const {
    for (const Fortran::evaluate::characteristics::DummyArgument &dummy :
         proc.dummyArguments)
      if (std::holds_alternative<
              Fortran::evaluate::characteristics::AlternateReturn>(dummy.u))
        return true;
    return false;
  };

  /// This is only here to fulfill CRTP dependencies and should not be called.
  std::string getMangledName() const {
    if (procDesignator)
      return getProcMangledName(*procDesignator, converter);
    fir::emitFatalError(
        converter.getCurrentLocation(),
        "should not query name when only building function type");
  }

  /// This is only here to fulfill CRTP dependencies and should not be called.
  mlir::Location getCalleeLocation() const {
    if (procDesignator)
      return getProcedureDesignatorLoc(*procDesignator, converter);
    return converter.getCurrentLocation();
  }

  const Fortran::semantics::Symbol *getProcedureSymbol() const {
    if (procDesignator)
      return procDesignator->GetSymbol();
    return nullptr;
  };

  Fortran::evaluate::characteristics::Procedure characterize() const {
    return proc;
  }
  /// SignatureBuilder cannot be used on main program.
  static constexpr bool isMainProgram() { return false; }

  /// Return the characteristics::Procedure that is being translated to
  /// mlir::FunctionType.
  const Fortran::evaluate::characteristics::Procedure &
  getCallDescription() const {
    return proc;
  }

  /// This is not the description of an indirect call.
  static constexpr bool isIndirectCall() { return false; }

  /// Return the translated signature.
  mlir::FunctionType getFunctionType() {
    if (interfaceDetermined)
      fir::emitFatalError(converter.getCurrentLocation(),
                          "SignatureBuilder should only be used once");
    // Most unrestricted intrinsic characteristics have the Elemental attribute
    // which triggers CanBeCalledViaImplicitInterface to return false. However,
    // using implicit interface rules is just fine here.
    bool forceImplicit =
        procDesignator && procDesignator->GetSpecificIntrinsic();
    bool isImplicit = forceImplicit || proc.CanBeCalledViaImplicitInterface();
    determineInterface(isImplicit, proc);
    interfaceDetermined = true;
    return genFunctionType();
  }

  mlir::func::FuncOp getOrCreateFuncOp() {
    if (interfaceDetermined)
      fir::emitFatalError(converter.getCurrentLocation(),
                          "SignatureBuilder should only be used once");
    declare();
    interfaceDetermined = true;
    return getFuncOp();
  }

  // Copy of base implementation.
  static constexpr bool hasHostAssociated() { return false; }
  mlir::Type getHostAssociatedTy() const {
    llvm_unreachable("getting host associated type in SignatureBuilder");
  }

private:
  const Fortran::evaluate::ProcedureDesignator *procDesignator = nullptr;
  Fortran::evaluate::characteristics::Procedure proc;
  bool interfaceDetermined = false;
};

mlir::FunctionType Fortran::lower::translateSignature(
    const Fortran::evaluate::ProcedureDesignator &proc,
    Fortran::lower::AbstractConverter &converter) {
  return SignatureBuilder{proc, converter}.getFunctionType();
}

mlir::func::FuncOp Fortran::lower::getOrDeclareFunction(
    const Fortran::evaluate::ProcedureDesignator &proc,
    Fortran::lower::AbstractConverter &converter) {
  mlir::ModuleOp module = converter.getModuleOp();
  std::string name = getProcMangledName(proc, converter);
  mlir::func::FuncOp func = fir::FirOpBuilder::getNamedFunction(
      module, converter.getMLIRSymbolTable(), name);
  if (func)
    return func;

  // getOrDeclareFunction is only used for functions not defined in the current
  // program unit, so use the location of the procedure designator symbol, which
  // is the first occurrence of the procedure in the program unit.
  return SignatureBuilder{proc, converter}.getOrCreateFuncOp();
}

// Is it required to pass a dummy procedure with \p characteristics as a tuple
// containing the function address and the result length ?
static bool mustPassLengthWithDummyProcedure(
    const std::optional<Fortran::evaluate::characteristics::Procedure>
        &characteristics) {
  return characteristics &&
         Fortran::lower::CallInterfaceImpl<SignatureBuilder>::
             mustPassLengthWithDummyProcedure(*characteristics);
}

bool Fortran::lower::mustPassLengthWithDummyProcedure(
    const Fortran::evaluate::ProcedureDesignator &procedure,
    Fortran::lower::AbstractConverter &converter) {
  std::optional<Fortran::evaluate::characteristics::Procedure> characteristics =
      Fortran::evaluate::characteristics::Procedure::Characterize(
          procedure, converter.getFoldingContext(), /*emitError=*/false);
  return ::mustPassLengthWithDummyProcedure(characteristics);
}

mlir::Type Fortran::lower::getDummyProcedureType(
    const Fortran::semantics::Symbol &dummyProc,
    Fortran::lower::AbstractConverter &converter) {
  std::optional<Fortran::evaluate::characteristics::Procedure> iface =
      Fortran::evaluate::characteristics::Procedure::Characterize(
          dummyProc, converter.getFoldingContext());
  mlir::Type procType = getProcedureDesignatorType(
      iface.has_value() ? &*iface : nullptr, converter);
  if (::mustPassLengthWithDummyProcedure(iface))
    return fir::factory::getCharacterProcedureTupleType(procType);
  return procType;
}

bool Fortran::lower::isCPtrArgByValueType(mlir::Type ty) {
  return mlir::isa<fir::ReferenceType>(ty) &&
         fir::isa_integer(fir::unwrapRefType(ty));
}

// Return the mlir::FunctionType of a procedure
static mlir::FunctionType
getProcedureType(const Fortran::evaluate::characteristics::Procedure &proc,
                 Fortran::lower::AbstractConverter &converter) {
  return SignatureBuilder{proc, converter, false}.genFunctionType();
}

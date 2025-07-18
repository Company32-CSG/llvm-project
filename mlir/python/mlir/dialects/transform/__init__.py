#  Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
#  See https://llvm.org/LICENSE.txt for license information.
#  SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

from .._transform_enum_gen import *
from .._transform_ops_gen import *
from .._transform_ops_gen import _Dialect
from ..._mlir_libs._mlirDialectsTransform import *
from ..._mlir_libs._mlirDialectsTransform import AnyOpType, OperationType

try:
    from ...ir import *
    from .._ods_common import (
        get_op_result_or_value as _get_op_result_or_value,
        get_op_results_or_values as _get_op_results_or_values,
        _cext as _ods_cext,
    )
except ImportError as e:
    raise RuntimeError("Error loading imports from extension module") from e

from typing import Dict, Optional, Sequence, Union, NewType


@register_attribute_builder("ParamOperandAttr")
def _paramOperandAttr(x: int, context) -> Attribute:
    return Attribute.parse(f"#transform.param_operand<index={x}>", context=context)


@_ods_cext.register_operation(_Dialect, replace=True)
class CastOp(CastOp):
    def __init__(
        self,
        result_type: Type,
        target: Union[Operation, Value],
        *,
        loc=None,
        ip=None,
    ):
        super().__init__(result_type, _get_op_result_or_value(target), loc=loc, ip=ip)


@_ods_cext.register_operation(_Dialect, replace=True)
class ApplyPatternsOp(ApplyPatternsOp):
    def __init__(
        self,
        target: Union[Operation, Value, OpView],
        *,
        loc=None,
        ip=None,
    ):
        super().__init__(target, loc=loc, ip=ip)
        self.regions[0].blocks.append()

    @property
    def patterns(self) -> Block:
        return self.regions[0].blocks[0]


@_ods_cext.register_operation(_Dialect, replace=True)
class GetParentOp(GetParentOp):
    def __init__(
        self,
        result_type: Type,
        target: Union[Operation, Value],
        *,
        isolated_from_above: bool = False,
        op_name: Optional[str] = None,
        deduplicate: bool = False,
        nth_parent: int = 1,
        loc=None,
        ip=None,
    ):
        super().__init__(
            result_type,
            _get_op_result_or_value(target),
            isolated_from_above=isolated_from_above,
            op_name=op_name,
            deduplicate=deduplicate,
            nth_parent=nth_parent,
            loc=loc,
            ip=ip,
        )


@_ods_cext.register_operation(_Dialect, replace=True)
class MergeHandlesOp(MergeHandlesOp):
    def __init__(
        self,
        handles: Sequence[Union[Operation, Value]],
        *,
        deduplicate: bool = False,
        loc=None,
        ip=None,
    ):
        super().__init__(
            [_get_op_result_or_value(h) for h in handles],
            deduplicate=deduplicate,
            loc=loc,
            ip=ip,
        )


@_ods_cext.register_operation(_Dialect, replace=True)
class ReplicateOp(ReplicateOp):
    def __init__(
        self,
        pattern: Union[Operation, Value],
        handles: Sequence[Union[Operation, Value]],
        *,
        loc=None,
        ip=None,
    ):
        super().__init__(
            [_get_op_result_or_value(h).type for h in handles],
            _get_op_result_or_value(pattern),
            [_get_op_result_or_value(h) for h in handles],
            loc=loc,
            ip=ip,
        )


@_ods_cext.register_operation(_Dialect, replace=True)
class SequenceOp(SequenceOp):
    def __init__(
        self,
        failure_propagation_mode,
        results: Sequence[Type],
        target: Union[Operation, Value, Type],
        extra_bindings: Optional[
            Union[Sequence[Value], Sequence[Type], Operation, OpView]
        ] = None,
    ):
        root = (
            _get_op_result_or_value(target)
            if isinstance(target, (Operation, Value))
            else None
        )
        root_type = root.type if not isinstance(target, Type) else target

        if extra_bindings is None:
            extra_bindings = []
        if isinstance(extra_bindings, (Operation, OpView)):
            extra_bindings = _get_op_results_or_values(extra_bindings)

        extra_binding_types = []
        if len(extra_bindings) != 0:
            if isinstance(extra_bindings[0], Type):
                extra_binding_types = extra_bindings
                extra_bindings = []
            else:
                extra_binding_types = [v.type for v in extra_bindings]

        super().__init__(
            results_=results,
            failure_propagation_mode=failure_propagation_mode,
            root=root,
            extra_bindings=extra_bindings,
        )
        self.regions[0].blocks.append(*tuple([root_type] + extra_binding_types))

    @property
    def body(self) -> Block:
        return self.regions[0].blocks[0]

    @property
    def bodyTarget(self) -> Value:
        return self.body.arguments[0]

    @property
    def bodyExtraArgs(self) -> BlockArgumentList:
        return self.body.arguments[1:]


@_ods_cext.register_operation(_Dialect, replace=True)
class NamedSequenceOp(NamedSequenceOp):
    def __init__(
        self,
        sym_name,
        input_types: Sequence[Type],
        result_types: Sequence[Type],
        sym_visibility=None,
        arg_attrs=None,
        res_attrs=None,
    ):
        function_type = FunctionType.get(input_types, result_types)
        super().__init__(
            sym_name=sym_name,
            function_type=TypeAttr.get(function_type),
            sym_visibility=sym_visibility,
            arg_attrs=arg_attrs,
            res_attrs=res_attrs,
        )
        self.regions[0].blocks.append(*input_types)

    @property
    def body(self) -> Block:
        return self.regions[0].blocks[0]

    @property
    def bodyTarget(self) -> Value:
        return self.body.arguments[0]

    @property
    def bodyExtraArgs(self) -> BlockArgumentList:
        return self.body.arguments[1:]


@_ods_cext.register_operation(_Dialect, replace=True)
class YieldOp(YieldOp):
    def __init__(
        self,
        operands: Optional[Union[Operation, Sequence[Value]]] = None,
        *,
        loc=None,
        ip=None,
    ):
        if operands is None:
            operands = []
        super().__init__(_get_op_results_or_values(operands), loc=loc, ip=ip)


OptionValueTypes = Union[
    Sequence["OptionValueTypes"], Attribute, Value, Operation, OpView, str, int, bool
]


@_ods_cext.register_operation(_Dialect, replace=True)
class ApplyRegisteredPassOp(ApplyRegisteredPassOp):
    def __init__(
        self,
        result: Type,
        target: Union[Operation, Value, OpView],
        pass_name: Union[str, StringAttr],
        *,
        options: Optional[Dict[Union[str, StringAttr], OptionValueTypes]] = None,
        loc=None,
        ip=None,
    ):
        options_dict = {}
        dynamic_options = []

        ParamOperandAttr = AttrBuilder.get("ParamOperandAttr")
        context = (loc and loc.context) or Context.current

        cur_param_operand_idx = 0

        def option_value_to_attr(value):
            nonlocal cur_param_operand_idx
            if isinstance(value, (Value, Operation, OpView)):
                dynamic_options.append(_get_op_result_or_value(value))
                cur_param_operand_idx += 1
                return ParamOperandAttr(cur_param_operand_idx - 1, context)
            elif isinstance(value, Attribute):
                return value
            # The following cases auto-convert Python values to attributes.
            elif isinstance(value, bool):
                return BoolAttr.get(value)
            elif isinstance(value, int):
                default_int_type = IntegerType.get_signless(64, context)
                return IntegerAttr.get(default_int_type, value)
            elif isinstance(value, str):
                return StringAttr.get(value)
            elif isinstance(value, Sequence):
                return ArrayAttr.get([option_value_to_attr(elt) for elt in value])
            else:
                raise TypeError(f"Unsupported option type: {type(value)}")

        for key, value in options.items() if options is not None else {}:
            if isinstance(key, StringAttr):
                key = key.value
            options_dict[key] = option_value_to_attr(value)
        super().__init__(
            result,
            _get_op_result_or_value(target),
            pass_name,
            dynamic_options,
            options=DictAttr.get(options_dict),
            loc=loc,
            ip=ip,
        )


def apply_registered_pass(
    result: Type,
    target: Union[Operation, Value, OpView],
    pass_name: Union[str, StringAttr],
    *,
    options: Optional[Dict[Union[str, StringAttr], OptionValueTypes]] = None,
    loc=None,
    ip=None,
) -> Value:
    return ApplyRegisteredPassOp(
        result=result,
        pass_name=pass_name,
        target=target,
        options=options,
        loc=loc,
        ip=ip,
    ).result


AnyOpTypeT = NewType("AnyOpType", AnyOpType)


def any_op_t() -> AnyOpTypeT:
    return AnyOpTypeT(AnyOpType.get())

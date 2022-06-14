#include <script/Builtins.hpp>
#include <script/vm/InstructionHandler.hpp>
#include <script/vm/MemoryBuffer.hpp>

#include <math/math_util.h>

namespace hyperion {

using namespace vm;
using namespace compiler;

HYP_SCRIPT_FUNCTION(ScriptFunctions::ArraySize)
{
    HYP_SCRIPT_CHECK_ARGS(==, 1);

    aint64 len = 0;

    Value *target_ptr = params.args[0];
    AssertThrow(target_ptr != nullptr);

    const int buffer_size = 256;
    char buffer[buffer_size];
    std::snprintf(
        buffer,
        buffer_size,
        "ArraySize() is undefined for type '%s'",
        target_ptr->GetTypeString()
    );
    vm::Exception e(buffer);

    if (target_ptr->GetType() == Value::ValueType::HEAP_POINTER) {
        union {
            ImmutableString *str_ptr;
            Array *array_ptr;
            MemoryBuffer *memory_buffer_ptr;
            Object *obj_ptr;
        } data;

        if (target_ptr->GetValue().ptr == nullptr) {
            params.handler->state->ThrowException(
                params.handler->thread,
                vm::Exception::NullReferenceException()
            );
        } else if ((data.str_ptr = target_ptr->GetValue().ptr->GetPointer<ImmutableString>()) != nullptr) {
            // get length of string
            len = data.str_ptr->GetLength();
        } else if ((data.array_ptr = target_ptr->GetValue().ptr->GetPointer<Array>()) != nullptr) {
            // get length of array
            len = data.array_ptr->GetSize();
        } else if ((data.memory_buffer_ptr = target_ptr->GetValue().ptr->GetPointer<MemoryBuffer>()) != nullptr) {
            // get length of memory buffer
            len = data.memory_buffer_ptr->GetSize();
        } else if ((data.obj_ptr = target_ptr->GetValue().ptr->GetPointer<Object>()) != nullptr) {
            // get number of members in object
            len = data.obj_ptr->GetSize();
        } else {
            params.handler->state->ThrowException(params.handler->thread, e);
        }
    } else {
        params.handler->state->ThrowException(params.handler->thread, e);
    }

    HYP_SCRIPT_RETURN_INT64(len);
}

HYP_SCRIPT_FUNCTION(ScriptFunctions::ArrayPush)
{
    HYP_SCRIPT_CHECK_ARGS(>=, 2);

    vm::Value *target_ptr = params.args[0];
    AssertThrow(target_ptr != nullptr);

    vm::Exception e("ArrayPush() requires an array argument");

    if (target_ptr->GetType() == vm::Value::ValueType::HEAP_POINTER) {
        vm::Array *array_ptr = nullptr;

        if (target_ptr->GetValue().ptr == nullptr) {
            params.handler->state->ThrowException(
                params.handler->thread,
                vm::Exception::NullReferenceException()
            );
        } else if ((array_ptr = target_ptr->GetValue().ptr->GetPointer<vm::Array>()) != nullptr) {
            array_ptr->PushMany(params.nargs - 1, &params.args[1]);
        } else {
            params.handler->state->ThrowException(params.handler->thread, e);
        }
    } else {
        params.handler->state->ThrowException(params.handler->thread, e);
    }

    // return same value
    HYP_SCRIPT_RETURN(*target_ptr);
}

HYP_SCRIPT_FUNCTION(ScriptFunctions::ArrayPop)
{
    HYP_SCRIPT_CHECK_ARGS(==, 1);

    vm::Value *target_ptr = params.args[0];
    AssertThrow(target_ptr != nullptr);

    vm::Exception e("ArrayPop() requires an array argument");

    vm::Value value; // value that was popped from array

    if (target_ptr->GetType() == vm::Value::ValueType::HEAP_POINTER) {
        vm::Array *array_ptr = nullptr;

        if (target_ptr->GetValue().ptr == nullptr) {
            params.handler->state->ThrowException(
                params.handler->thread,
                vm::Exception::NullReferenceException()
            );
        } else if ((array_ptr = target_ptr->GetValue().ptr->GetPointer<vm::Array>()) != nullptr) {
            if (array_ptr->GetSize() == 0) {
                params.handler->state->ThrowException(
                    params.handler->thread,
                    vm::Exception::OutOfBoundsException()
                );

                return;
            }
            
            value = Value(array_ptr->AtIndex(array_ptr->GetSize() - 1));

            array_ptr->Pop();
        } else {
            params.handler->state->ThrowException(params.handler->thread, e);
        }
    } else {
        params.handler->state->ThrowException(params.handler->thread, e);
    }

    // return popped value
    HYP_SCRIPT_RETURN(value);
}

HYP_SCRIPT_FUNCTION(ScriptFunctions::Puts)
{
    HYP_SCRIPT_CHECK_ARGS(>=, 1);

    vm::ExecutionThread *thread = params.handler->thread;
    vm::VMState *state = params.handler->state;

    const auto *string_arg = params.args[0]->GetValue().ptr->GetPointer<ImmutableString>();

    if (!string_arg) {
        state->ThrowException(
            thread,
            Exception::InvalidArgsException("string")
        );

        return;
    }

    int puts_result = std::puts(string_arg->GetData());

    HYP_SCRIPT_RETURN_INT32(puts_result);
}

HYP_SCRIPT_FUNCTION(ScriptFunctions::ToString)
{
    HYP_SCRIPT_CHECK_ARGS(==, 1);

    // create heap value for string
    vm::HeapValue *ptr = params.handler->state->HeapAlloc(params.handler->thread);

    AssertThrow(ptr != nullptr);
    ptr->Assign(params.args[0]->ToString());

    vm::Value res;
    // assign register value to the allocated object
    res.m_type = vm::Value::HEAP_POINTER;
    res.m_value.ptr = ptr;

    ptr->Mark();

    HYP_SCRIPT_RETURN(res);
}

HYP_SCRIPT_FUNCTION(ScriptFunctions::Format)
{
    HYP_SCRIPT_CHECK_ARGS(>=, 1);

    vm::Value *target_ptr = params.args[0];
    AssertThrow(target_ptr != nullptr);

    vm::Exception e("Format() expects a string as the first argument");

    if (target_ptr->GetType() == vm::Value::ValueType::HEAP_POINTER) {
        if (target_ptr->GetValue().ptr == nullptr) {
            params.handler->state->ThrowException(params.handler->thread, vm::Exception::NullReferenceException());
        } else if (vm::ImmutableString *str_ptr = target_ptr->GetValue().ptr->GetPointer<vm::ImmutableString>()) {
            // scan through string and merge each argument where there is a '%'
            const size_t original_length = str_ptr->GetLength();

            std::string result_string;
            result_string.reserve(original_length);

            const char *original_data = str_ptr->GetData();
            AssertThrow(original_data != nullptr);

            const int buffer_size = 256;
            char buffer[buffer_size + 1] = {0};

            // number of '%' characters handled
            int num_fmts = 1;
            int buffer_idx = 0;

            for (size_t i = 0; i < original_length; i++) {
                if (original_data[i] == '%' && num_fmts < params.nargs) {
                    // set end of buffer to be NUL
                    buffer[buffer_idx + 1] = '\0';
                    // now upload to result string
                    result_string += buffer;
                    // clear buffer
                    buffer_idx = 0;
                    buffer[0] = '\0';

                    vm::ImmutableString str = params.args[num_fmts++]->ToString();

                    result_string.append(str.GetData());
                } else {
                    buffer[buffer_idx] = original_data[i];

                    if (buffer_idx == buffer_size - 1 || i == original_length - 1) {
                        // set end of buffer to be NUL
                        buffer[buffer_idx + 1] = '\0';
                        // now upload to result string
                        result_string += buffer;
                        //clear buffer
                        buffer_idx = 0;
                        buffer[0] = '\0';
                    } else {
                        buffer_idx++;
                    }
                }
            }

            while (num_fmts < params.nargs) {
                vm::ImmutableString str = params.args[num_fmts++]->ToString();

                result_string.append(str.GetData());
            }

            // store the result in a variable
            vm::HeapValue *ptr = params.handler->state->HeapAlloc(params.handler->thread);
            AssertThrow(ptr != nullptr);
            // assign it to the formatted string
            ptr->Assign(vm::ImmutableString(result_string.data()));

            vm::Value res;
            // assign register value to the allocated object
            res.m_type = vm::Value::HEAP_POINTER;
            res.m_value.ptr = ptr;

            ptr->Mark();

            HYP_SCRIPT_RETURN(res);
        } else {
            params.handler->state->ThrowException(params.handler->thread, e);
        }
    } else {
        params.handler->state->ThrowException(params.handler->thread, e);
    }
}

HYP_SCRIPT_FUNCTION(ScriptFunctions::Print)
{
    HYP_SCRIPT_CHECK_ARGS(>=, 1);

    vm::Value *target_ptr = params.args[0];
    AssertThrow(target_ptr != nullptr);

    vm::Exception e("Print() expects a string as the first argument");

    if (target_ptr->GetType() == vm::Value::ValueType::HEAP_POINTER) {
        if (target_ptr->GetValue().ptr == nullptr) {
            params.handler->state->ThrowException(params.handler->thread, vm::Exception::NullReferenceException());
        } else if (vm::ImmutableString *str_ptr = target_ptr->GetValue().ptr->GetPointer<vm::ImmutableString>()) {
            // scan through string and merge each argument where there is a '%'
            const size_t original_length = str_ptr->GetLength();

            std::string result_string;
            result_string.reserve(original_length);

            const char *original_data = str_ptr->GetData();
            AssertThrow(original_data != nullptr);

            const int buffer_size = 256;
            char buffer[buffer_size + 1] = {0};

            // number of '%' characters handled
            int num_fmts = 1;
            int buffer_idx = 0;

            for (size_t i = 0; i < original_length; i++) {
                if (original_data[i] == '%' && num_fmts < params.nargs) {
                    // set end of buffer to be NUL
                    buffer[buffer_idx + 1] = '\0';
                    // now upload to result string
                    result_string += buffer;
                    // clear buffer
                    buffer_idx = 0;
                    buffer[0] = '\0';

                    vm::ImmutableString str = params.args[num_fmts++]->ToString();

                    result_string.append(str.GetData());
                } else {
                    buffer[buffer_idx] = original_data[i];

                    if (buffer_idx == buffer_size - 1 || i == original_length - 1) {
                        // set end of buffer to be NUL
                        buffer[buffer_idx + 1] = '\0';
                        // now upload to result string
                        result_string += buffer;
                        //clear buffer
                        buffer_idx = 0;
                        buffer[0] = '\0';
                    } else {
                        buffer_idx++;
                    }
                }
            }

            while (num_fmts < params.nargs) {
                vm::ImmutableString str = params.args[num_fmts++]->ToString();

                result_string.append(str.GetData());
            }

            utf::cout << result_string;
            
            HYP_SCRIPT_RETURN_INT32(result_string.size());
        } else {
            params.handler->state->ThrowException(params.handler->thread, e);
        }
    } else {
        params.handler->state->ThrowException(params.handler->thread, e);
    }
}

HYP_SCRIPT_FUNCTION(ScriptFunctions::Malloc)
{
    HYP_SCRIPT_CHECK_ARGS(==, 1);

    vm::Value *target_ptr = params.args[0];
    AssertThrow(target_ptr != nullptr);

    vm::Exception e("Malloc() expects an integer as the first argument");

    Number num;

    if (target_ptr->GetSignedOrUnsigned(&num)) {
        // create heap value for string
        vm::HeapValue *ptr = params.handler->state->HeapAlloc(params.handler->thread);

        auint64 malloc_size = num.flags & Number::FLAG_SIGNED
            ? static_cast<auint64>(MathUtil::Max(0, num.i))
            : num.u;

        AssertThrow(ptr != nullptr);
        ptr->Assign(vm::MemoryBuffer(malloc_size));

        vm::Value res;
        // assign register value to the allocated object
        res.m_type = vm::Value::HEAP_POINTER;
        res.m_value.ptr = ptr;

        ptr->Mark();

        HYP_SCRIPT_RETURN(res);
    } else {
        params.handler->state->ThrowException(params.handler->thread, e);
    }
}

HYP_SCRIPT_FUNCTION(ScriptFunctions::Free)
{
    HYP_SCRIPT_CHECK_ARGS(==, 1);

    vm::Value *target_ptr = params.args[0];
    AssertThrow(target_ptr != nullptr);

    vm::Exception e("Free() expects a pointer type");

    if (target_ptr->GetType() == Value::HEAP_POINTER) {
        // just mark nullptr, gc will collect it.
        target_ptr->GetValue().ptr = nullptr;
    } else {
        params.handler->state->ThrowException(params.handler->thread, e);
    }
}

void ScriptFunctions::Build(APIInstance &api_instance)
{
    api_instance.Module(hyperion::compiler::Config::global_module_name)
        .Function(
            "ArraySize",
            BuiltinTypes::INT,
            {
                { "self", BuiltinTypes::ANY } // one of: Array, String, Object
            },
            ArraySize
        )
        .Function(
            "ArrayPush",
            BuiltinTypes::ARRAY,
            {
                { "self", BuiltinTypes::ARRAY },
                {
                    "args",
                    SymbolType::GenericInstance(
                        BuiltinTypes::VAR_ARGS,
                        GenericInstanceTypeInfo {
                            {
                                { "arg", BuiltinTypes::ANY }
                            }
                        }
                    )
                }
            },
            ArrayPush
        )
        .Function(
            "ArrayPop",
            BuiltinTypes::ANY, // returns object that was popped
            {
                { "self", BuiltinTypes::ARRAY }
            },
            ArrayPop
        )
        .Function(
            "Puts",
            BuiltinTypes::INT,
            {
                { "str", BuiltinTypes::STRING }
            },
            Puts
        )
        .Function(
            "ToString",
            BuiltinTypes::STRING,
            {
                { "obj", BuiltinTypes::ANY }
            },
            ToString
        )
        .Function(
            "Format",
            BuiltinTypes::STRING,
            {
                { "format", BuiltinTypes::STRING },
                {
                    "args",
                    SymbolType::GenericInstance(
                        BuiltinTypes::VAR_ARGS,
                        GenericInstanceTypeInfo {
                            {
                                { "arg", BuiltinTypes::ANY }
                            }
                        }
                    )
                }
            },
            Format
        )
        .Function(
            "Print",
            BuiltinTypes::INT,
            {
                { "format", BuiltinTypes::STRING },
                {
                    "args",
                    SymbolType::GenericInstance(
                        BuiltinTypes::VAR_ARGS,
                        GenericInstanceTypeInfo {
                            {
                                { "arg", BuiltinTypes::ANY }
                            }
                        }
                    )
                }
            },
            Print
        )
        .Function(
            "Malloc",
            BuiltinTypes::ANY,
            {
                { "size", BuiltinTypes::INT } // TODO: should be unsigned, but having conversion issues
            },
            Malloc
        )
        .Function(
            "Free",
            BuiltinTypes::VOID,
            {
                { "ptr", BuiltinTypes::ANY } // TODO: should be unsigned, but having conversion issues
            },
            Free
        );
}

} // namespace hyperion

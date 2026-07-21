# NiHao Programming Language Reference Manual

## 1. Overview

NiHao is a new statically compiled language designed for system-level programming and high-performance applications, combining modern language features with low-level control capabilities.

## 2. Basic Syntax

### 2.1 Comments

```
// Single-line comment
/* Multi-line comment */
```

### 2.2 Statement Separation

- Statements are separated by newlines

- Multiple statements on the same line use semicolons: `stmt1; stmt2`

### 2.3 Built-in Functions

- `typeof(type)`Type checking, returns type

- `sizeof(type)`Size checking, returns size

- `holdof(type,member,ptr)`Ownership checking, returns member owner

- `visof(var)`Visibility checking, returns visibility attribute

### 2.4 Keyword Description

- `alias`Type alias

- `const`Modifier for immutability

- `flow`Modifier for dynamic visibility

- `static`Modifier for static visibility

- `_flow`Dynamic visibility attribute enum value

- `_static`Static visibility attribute enum value

- `_undef`Undefined invisible attribute enum value

- `cooking {...}`Compile-time execution code block keyword

- `align n {...}`Byte alignment code block

- `use` include the module extern

- `module` define the current module name

- `link ...`Auxiliary compilation, Export extern from linked file

## 3. Type System

### 3.1 Basic Types

| Type     | Description                           | Size         |
| -------- | ------------------------------------- | ------------ |
| `char`   | Character type                        | 1 byte       |
| `string` | String type                           | Dynamic      |
| `u8`     | Unsigned 8-bit integer                | 1 byte       |
| `u16`    | Unsigned 16-bit integer               | 2 bytes      |
| `u32`    | Unsigned 32-bit integer               | 4 bytes      |
| `u64`    | Unsigned 64-bit integer               | 8 bytes      |
| `i8`     | Signed 8-bit integer                  | 1 byte       |
| `i16`    | Signed 16-bit integer                 | 2 bytes      |
| `i32`    | Signed 32-bit integer                 | 4 bytes      |
| `i64`    | Signed 64-bit integer                 | 8 bytes      |
| `f32`    | Single-precision floating point       | 4 bytes      |
| `f64`    | Double-precision floating point       | 8 bytes      |
| `fx32`   | Single-precision fixed-point (Q16.16) | 4 bytes      |
| `fx64`   | Double-precision fixed-point (Q32.32) | 8 bytes      |
| `void`   | Generic pointer type                  | Pointer size |

### 3.2 Composite Types

**Array Declaration:**

```
fixedArray char[3]       // Fixed-size array
dynamicArray i8[...]     // Dynamic array without initial value
initArray u16[6...]      // Dynamic array with initial size
// Array access
```

**Type Aliases:**

```
alias Byte = u8
alias StringPtr = void[]
```

**Type Definitions:**

```
Person struct{
    name string
    age u8
    flag u8:1 // Supports bitfield syntax
}
```

**Unions:**

```
Data union{
    asInt i32
    asFloat f32
}
```

**Enums:**

```
Color enum{ RED, GREEN, BLUE }
```

### 3.3 Type Operations

###### 3.3.1 Type align

```
// Type checking
if typeof(value) == i32 { ... }

// Type size
size u8 = sizeof(Person)

// Type alignment
align 4 {
  Protocol struct {
    data u8
    len  u32
    flag u32:1 
    tag  u32:2
  }
}  // 4-byte alignment
```

###### 3.3.1 Type nesting

```nihao
// Normal nesting
aunion union{
    value u16
    reg struct{
        r0 u8
        r1 u8
    }
}
aunion.reg.r1 = 1

// Anonymous nesting
xunion union{
    value u8
    struct{
        r0 u8:2
        r1 u8:2
        r2 u8:2
        r3 u8:2
    }
}
xunion.r1 = 1
```

## 4. Variable Declaration and Visibility

### 4.1 Declaration Modifiers

| Modifier    | Description                         |
| ----------- | ----------------------------------- |
| `const`     | Defines fixed visibility variable   |
| `flow`      | Defines dynamic visibility variable |
| `static`    | Defines static visibility variable  |
| No modifier | Defines local visibility variable   |

### 4.2 Examples

```
const MAX_SIZE i32 = 1024
flow counter i8 = 0
static globalVar f32 = 3.14
{inferred string = "Hello"}
```

## 5. Pointers and Memory Management

### 5.1 Pointer Operations

###### 5.1.1 Pointer Definition

```nihao
// Declaring a null pointer is not allowed; it must be assigned at declaration.
variable i8 = 0
varptr void = &var

// Single-level pointer
ptr void = malloc(i32)   // Allocate memory
ptr.(i32) = 42           // Dereference and assign

// Multi-level pointers
ptr2 void[] = &ptr       // Define a second-level pointer
ptr = ptr2.()            // One-level dereference
variable = ptr2[].(i32)  // Two-level dereference

ptr3 void[][] ?= &ptr2    // Define a third-level pointer
ptr2 = ptr3.()            // One-level dereference
ptr  = ptr3[].()          // Two-level dereference
variable = ptr3[][].(i32) // Three-level dereference
```

###### 5.1.2 Array Pointer

```nihao
arry char[9] = {1,2,3,4,5,6,7,8,9}
arryptr void = &arry           // Obtain the array pointer
arryptr[0] = 0
arryptr[9] = 9                 // Undefined behavior
arryptr.(char[9])[9] = 9       // Compilation error: Out-of-bounds
arryptr.(char[9])[0] = 0       // Dereference member [0]


arrybuffer char[8] = arryptr.(char[9])[0..7]
// arrybuffer == {0,1,2,4,5,6,7,8}

// Array of array pointers
arryptr2 void[2] = {&arry,&arrybuffer}
arryptr2[0].(char[9])[8] = arry[8]
arryptr2[1].(char[8])[7] = arrybuffer[7]
```

###### 5.1.3 Pointer Array

```nihao
dptrarry1 void[3] = malloc(void[3]) // Dynamically allocate a 1D pointer array
dptrarry1[2] = ptr
dptrarry1[2].(i32) += 1

dptr3 void[4][5] = malloc(void[4][5]) // Dynamically allocate a 2D pointer array
dptr3[3][4] = ptr       // Safe pointer assignment
// Error: dptr3[0][0].(int64) error: int64 type size > i32 type size!
dptr3[3][4].(i32) += 1  // Multi-level pointer dereference

// Pointer to pointer array
ptrarry void = &arryptr2
ptrarry.(void[2])[0].(char[9])[8] = 8
ptrarry.(void[2])[1].(char[8])[7] = 7

// arry == {0,1,2,4,5,6,7,8,8}
// arrybuffer == {0,1,2,4,5,6,7,7}
```

###### 5.1.4 Composite Type Pointer

```nihao
Say struct{
    name char[9]
    say string
}
xiaoming Say
stptr void = &xiaoming
stptr.(char[9])[0..8] = "xiaoming"  // Pointer slice assignment
stptr.(Say).say = "NiHao I am xiaoming!" // Pointer type dereference
talk = xiaoming.say
puts(talk)
// puts(talk) out--> "NiHao I am xiaoming!"
```

###### 5.1.5 Function Pointer

```nihao
const callback_handle(argc u16) {
    puts("callback call!")
}

const callback void(u16) = callback_handle

const callback_register(const cb void(u16), event u32) u32 {
    if event == 1 {
        callback = cb
    }
}

flow callback_handle_with_return(argc u16) i32 {
    puts("callback call!")
    return 42
}

flow callback2 void(u16)i32 = callback_handle_with_return

const callback_register_with_return(flow cb void(u16)i32 , event u32) u32 {
    if event == 1 {
        callback2 = cb
    }
}
```

### 5.2 Memory Checking

```
// Visibility checking
if visof(ptr) == _static { 
    // ...
}

// Ownership checking
boy Person = {"xiaoming", 13}
ptr void = &boy.name
if holdof(ptr) == boy { 
    // ...
}
```

## 6. Control Structures

### 6.1 Conditional Statements

```
if condition {
    // ...
} else if anotherCondition {
    // ...
} else {
    // ...
}
```

### 6.2 Loop Structures

**do Loop:**

```
do value > 0 {
    value++
    if value == 100 {
        break
    }
    else if value == 50{
      continue
    }
}
```

**while Loop (with pattern matching):**

```
var1 u8
while var1 += 1 {
    is -1 {
        break
    }
    is 0..50 {
        continue
    }
    break
}
```

**for Loop:**

```
for i = 0; i < 10; i++ {
    // ...
}
```

## 7. Function Definitions

### 7.1 Function Declaration

```
// Function with no return and no parameters
const greet() {
    print("Hello")
}

// Function with return and parameters
const add(a i8, b i8) i8 {
    return a + b
}

// Function with multiple returns
const swap(a i8, b i8) (i8, i8) {
    return b, a
}
```

## 8. Module System

### 8.1 Module Definition

```
module mathUtils

const add(a i32, b i32) i32 {
    return a + b
}
```

### 8.2 Module Usage

```
use mathUtils
```

### 8.3 Library Linking

```
link "libc.so" libc
```

## 9. Compilation Directives

### 9.1 Common Commands

```
nihao init     # Initialize project
nihao build    # Build project
nihao run      # Build and run
nihao debug    # Build in debug mode
```

### 9.2 Compile-time Execution

```
cooking {
    // Code executed at compile time
    const BUILD_TIME = time.now()
}
```

## 10. Example Program

```
module main
use stdio
use stdlib
link "libhttp.so" http

alias http_client = http.http_client
alias time = stdlib.time

const ConstValue i8 = 100

multireturn struct{
    value1 u8
    value u8
}

const main() 
{
    puts("Program starting\n")

    // Dynamic memory allocation
    flow dynptr void = malloc(i32)

    // Pointer operations
    dynptr.(i32) = ConstValue

    // Array operations
    arry f32[3] = {1.1, 1.2, 1.3}
    ptrarry void[3] = &arry
    ptrarry2 void[3] = {&arry[2], &arry[1], &arry[0]}

    // Visibility checking
    if visof(staticptr) == _static {
        flow temp void = malloc(float32)
    }

    if visof(dynptr) == _flow {
        puts("the ptr is _flow attribute \n");
    }

    // Multiple return value handling
    returnValue multireturn = calculate()

    return
    /* If the flow variable: dynptr, temp, is not returned,
     * they will be automatically free.
    */
}

const calculate() multireturn  
{
    if visof(value) != _undef {
      return {0,0}
    }
    else if visof(ConstValue) == _static {
      return {ConstValue, (ConstValue*2)}
    }
}
```

## 11. Advanced Features

### 11.1 Type Safety

```
// Safe pointer transfer
flow safePtr void ?= ptr

// Equivalent to
if visof(ptr) == _flow {
    safePtr = ptr
}
```

## 12. NiHao Language Visibility System and Lifetime Management

Variable attributes determine visibility attributes
![VisibilityDefinition](Visibility%20definition%20en.png)

### 12.1 Storage Duration and Scope Combination Rules

```
// Storage duration and scope rules
const MAX_SIZE i32 = 1024     // Static storage duration, global scope
static counter i32 = 0        // Static storage duration, module scope  
flow dynamic_var i32 = 42     // Dynamic storage duration, dynamic scope

// Local code block
{
    local_var i32 = 100       // Automatic storage duration, local scope
}

// Pointer safety transfer rules
const safe_pointer_operations() {
    // Safe transfer: same scope or longer-lived scope
    flow ptr1 void = &dynamic_var     // Safe: flow -> flow

    // Unsafe transfer: short-lived scope to long-lived scope
    // static ptr3 void = &local_var   // Error: local variable cannot be passed to static pointer
    // const ptr4 void = &dynamic_var  // Error: flow cannot be passed to const
}
```

### 12.2 Visibility Check Safety Transfer

```
// Visibility check rules for safe pointer assignment operator ?=
const visibility_checks() {
    source_ptr void = &some_variable
    target_ptr void

    // Safe assignment: check visibility compatibility
    target_ptr ?= source_ptr  // Equivalent to the following check:

    // Compile-time generated check logic
    if visof(source_ptr) == _flow && visof(target_ptr) == _flow {
        target_ptr = source_ptr  // flow -> flow safe
    }
    else if visof(source_ptr) == _static && visof(target_ptr) == _static {
        target_ptr = source_ptr  // static -> static safe
    }
    else if visof(source_ptr) == _const && visof(target_ptr) == _const {
        target_ptr = source_ptr  // const -> const safe
    }
    else {
        panic("Incompatible visibility pointer assignment")
    }
}
```

## 13. Visibility-Based Pointer Safety System

### 13.1 Pointer Transfer Visibility Rules Table

```
// Pointer assignment visibility compatibility matrix in the intersection of scopes,
// Source -> Target    const    static    flow    local
// const               Safe     Error     Error   Error
// static              Safe     Safe      Error   Error  
// flow                Safe     Error     Safe    Error
// local               Safe     Error     Error   Safe

const demonstrate_rules() {
    const GLOBAL i32 = 100
    static MODULE_VAR i32 = 200
    flow DYNAMIC_VAR i32 = 300
    LOCAL_VAR i32 = 400

    // Safe examples
    flow ptr1 void ?= &DYNAMIC_VAR     // flow -> flow: safe
    const ptr6 void ?= &MODULE_VAR     // static -> const: safe
   
    // Error examples (compile-time check)
    // static ptr5 void ?= &DYNAMIC_VAR  // flow -> static: error
    // static ptr7 void ?= &LOCAL_VAR    // local -> static: error
}
```

### 13.2 Function Parameter Visibility Constraints

```
// Function parameter visibility annotations
const process_static_data(static ptr void) i32 {
    // Can only accept static or const pointers
    return ptr.(i32)
}

const process_dynamic_data(flow ptr void) i32 {
    // Can accept flow, static, const pointers
    return ptr.(i32)
}

// Return value visibility constraints
const get_static_pointer() static void {
    static data i32 = 42
    return &data  // Return static pointer
}

const get_dynamic_pointer() flow void {
    flow data i32 = 42
    return &data  // Return flow pointer
}

const example_usage() {
    static static_ptr void = get_static_pointer()
    flow dynamic_ptr void = get_dynamic_pointer()

    // Safe calls
    process_static_data(static_ptr)     // static -> static: safe
    process_dynamic_data(dynamic_ptr)  // flow -> flow: safe
}
```

## 14. Dynamic Scope Lifetime Management

### 14.1 Flow Variable Scope Inference

```
// Compiler automatically infers flow variable scope
const scope_demonstration() {
    flow var1 i32 = 10

    if condition {
        flow var2 i32 = 20
        flow ptr1 void ?= &var1     // Safe: var1 scope contains var2
        flow ptr2 void ?= &var2     // Safe: same scope

        // var2 scope ends here
    }

    // ptr2 cannot be used here since var2 has left scope
    flow ptr3 void ?= &var1         // Safe: var1 still in scope
}

// Nested scope lifetime checking
const nested_scopes() {
    flow outer_var i32 = 100

    {
        flow inner_var i32 = 200
        flow inner_ptr void ?= &outer_var  // Safe: outer scope contains inner
        flow outer_ptr void ?= &inner_var  // Safe: same function scope
    }

    // After leaving inner scope, inner_var becomes invalid
    // But outer_var remains valid
}
```

### 14.2 Cross-Function Call Scope Management

```
// Scope transfer during function calls
const caller_function() {
    flow local_dynamic i32 = 42
    flow result i32 = process_with_callback(local_dynamic, &callback_function)
}

const process_with_callback(flow data i32, 
                           flow callback void(i32)i32
                           )flow i32 {
    // Both data and callback are flow, ensuring lifetime compatibility
    return callback(data)
}

const callback_function(value i32) i32 {
    return value * 2
}
```

## 15. Safe Pointer Operation Patterns

### 15.1 Visibility-Based Safe Dereferencing

```
// Visibility check for safe dereference operator !
const safe_dereference_examples() {
    flow dynamic_ptr void = &some_flow_variable
    static static_ptr void = &some_static_variable

    // Safe dereferencing
    value1 = dynamic_ptr?.(i32)     // Safe dereference of flow pointer
    value2 = static_ptr?.(i32)     // Safe dereference of static pointer

    // Safe dereference equivalent to
    if visof(dynamic_ptr) != _undef && dynamic_ptr != null {
        value1 = dynamic_ptr.(i32)
    } else {
        panic("Unsafe dereference")
    }
}
```

### 15.2 Safe Access to Array and Structure Pointers

```
// Structure definition
Person struct {
    name string   // Dynamic string
    age i32   // Static age
}

flow some_person Person
dynamic_array u8[10...]

const safe_structure_access() {
    flow person_ptr void = &some_person

    // Safe access to structure fields
    flow   name_ptr void ?= person_ptr.(Person).name  // Safe flow field transfer
    static age_ptr  void ?= person_ptr.(Person).age   // Safe static field transfer

    // Array bounds checking combined with visibility
    flow array_ptr void = &dynamic_array

    //
    element = array_ptr!.(i32[10])
    /* Equivalent to:
        if visof(array_ptr) == _flow && sizeof(dynamic_array) >= 10 
        {
            element = array_ptr.(i32[10])[5]
        }
    */
}
```

## 16. Error Handling and Debugging Support

### 16.1 Visibility Error Diagnostics

```
// Detailed visibility error information
const demonstrate_visibility_errors() {
    flow dynamic_var i32 = 42
    static static_var i32 = 100

    // Diagnostic information when visibility error occurs
    static error_ptr void ?= &dynamic_var  
    // Compilation error: Cannot assign flow visibility (_flow) to static visibility (_static)
    // Reason: Static pointer may outlive flow variable, causing dangling pointer
}

// Runtime visibility checking
const runtime_visibility_check(ptr void) {

    while visof(ptr) {
        is _const => puts("Constant pointer, global lifetime")
        is _static => puts("Static pointer, module lifetime") 
        is _flow => puts("Dynamic pointer, requires scope analysis")
        is _undef => puts("Undefined or invalid pointer")
        break
    }
}
```

### 16.2 Scope Debugging Tools

```
// Compile-time scope analysis report
const analyzed_function() {
    flow var1 i32 = 10        // [Scope: Function level]
    {
        flow var2 i32 = 20    // [Scope: Undetermined]
        var3 i32 = 30    // [Scope: Block level]
    }
    // [Warning: var2, var3 leaving scope]

    /* [Warning: var2 scope change]
        function {
            {---------------scope start

            }---------------scope end old

        }-------------------scope end new

    */
    var4 i32 = var2; 
    // [Compilation error: var3 left scope --> undefined]
    var4 = var3;

    // [Warning: tracked_var scope change]
    flow ptrvar1 void = scope_tracing_example()

    // [Compilation error: tracked_var can only be passed to flow variables]
    ptrvar2 void = scope_tracing_example()
}

const scope_tracing_example() flow i32{
    flow tracked_var i32 = 42

    return &tracked_var  // Enable scope tracing
    // Record scope entry/exit in debug mode
}
```

## 17. Practical Application Examples

### 17.1 Complete Module Example

```
module security_module

// Module-level static data
static module_counter i32 = 0
const MAX_CONNECTIONS i32 = 1000

// Secure data processor
SecurityProcessor struct {
    config static ConfigData
    state flow ProcessorState
}


const process_request(flow self SecurityProcessor, flow request Request) flow Response {
    // Secure state access
    self.state.current_request ?= request

    // Static configuration access
    if self.state.connection_count < self.config.(ConfigData).max_connections {
        self.state.connection_count++
        return create_response(200, "OK")
    }

    return create_response(429, "Too Many Requests")
}

const main() {
    flow processor SecurityProcessor = create_processor()
    flow request Request = receive_request()

    // Secure method call
    flow response Response = processor.process_request(request)
    send_response(response)
}
```

### 17.2 Memory Safety Patterns

```
// Visibility-based memory safety patterns
const memory_safe_patterns() {
    // Pattern 1: Dynamic data processed within enclosed scope
    {
        flow temporary_data Data = load_temporary_data()
        process_data(temporary_data)  // Data automatically cleaned up after processing
    }

    // Pattern 2: Static data held long-term
    static persistent_cache Cache = initialize_cache()
    use_cache(persistent_cache)

    // Pattern 3: Safe data transfer chain
    flow source_data Data = acquire_data()
    flow processed_data Data = transform_data(source_data)
    flow result Result = analyze_data(processed_data)
    // All flow data automatically cleaned up when function ends
}
```

## Summary

This design is based on NiHao language's existing visibility system, implementing memory safety through:

1. **Clear Storage Duration and Scope**: Clear lifetime rules for const/static/flow/local variables

2. **Visibility-Checked Pointer Safety**: Visibility compatibility checked during assignment using `?=`operator

3. **Dynamic Scope Management**: Flow variable scope automatically inferred by compiler

4. **Progressive Safety**: From local safety to module safety, finally to global safety

Key Advantages:

- No complex ownership system introduced

- Utilizes existing visibility modifiers

- Combined compile-time and runtime safety checks

- Highly consistent with NiHao language design philosophy

This maintains language simplicity while providing powerful memory safety guarantees.

---

*NiHao v1.0 Language Specification - © 2025 NiHao Development Team*

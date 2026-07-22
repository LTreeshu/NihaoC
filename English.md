# NiHao Programming Language Reference Manual

## 1. Overview

NiHao is a new statically compiled language designed for system-level programming and high-performance applications, combining modern language features with low-level control capabilities.

## 2. Basic Syntax

### 2.1 Comments

```nihao
// Single-line comment
/* Multi-line comment */
```

### 2.2 Statement Termination

- Statements are terminated by newline or optional `;`
- Multiple statements on the same line are separated by semicolons: `stmt1; stmt2`

### 2.3 Built-in Functions

- `typeof(type)` – type introspection, returns type
- `sizeof(type)` – size inquiry, returns size in bytes
- `alignof(type)` – alignment inquiry, returns alignment requirement
- `structof(member)` – owner inquiry, returns the owner struct of a member
- `unionof(member)` – owner inquiry, returns the owner union of a member
- `offsetof(type, member)` – returns byte offset of a member
- `bitoffsetof(type, bitmember)` – returns bit offset of a bit-field member
- `visof(var)` – visibility inquiry, returns visibility attribute

### 2.4 Keyword Reference

- `alias` – type alias
- `const` – denotes fixed, immutable visibility
- `flow` – denotes dynamic visibility
- `static` – denotes static visibility
- `var` – denotes local visibility, can be type‑inferred
- `_undef` – undefined visibility enumeration value
- `_const` – fixed/immutable visibility enumeration value
- `_flow` – dynamic visibility enumeration value
- `_static` – static visibility enumeration value
- `_var` – local visibility enumeration value
- `cooking { ... }` – compile‑time execution block
- `align n { ... }` – byte‑alignment block
- `use ...` – module import
- `module ...` – module definition
- `linkas "..."` – static library export naming
- `link "..." ...` – static library import usage
- `func` – denotes a function with no return attribute and no return value (but can have a return type when used with a return type)
- `[[inline]]` – function attribute: inline hint
- `[[weak]]` – function attribute: weak definition
- `[[static]]` – function attribute: internal linkage
- `[[used]]` – function attribute: force retain symbol
- `[[unused]]` – function attribute: force discard (deprecate)
- `[[export]".my_section"]` – function attribute: export to a specific section

## 3. Type System

### 3.1 Primitive Types

| Type     | Description                     | Size                 |
| -------- | ------------------------------- | -------------------- |
| `void`   | Generic pointer type            | machine pointer size |
| `char[]` | String type                     | dynamic              |
| `char`   | Character type                  | 1 byte               |
| `u8`     | Unsigned 8‑bit integer          | 1 byte               |
| `u16`    | Unsigned 16‑bit integer         | 2 bytes              |
| `u32`    | Unsigned 32‑bit integer         | 4 bytes              |
| `u64`    | Unsigned 64‑bit integer         | 8 bytes              |
| `i8`     | Signed 8‑bit integer            | 1 byte               |
| `i16`    | Signed 16‑bit integer           | 2 bytes              |
| `i32`    | Signed 32‑bit integer           | 4 bytes              |
| `i64`    | Signed 64‑bit integer           | 8 bytes              |
| `f32`    | Single‑precision floating point | 4 bytes              |
| `f64`    | Double‑precision floating point | 8 bytes              |
| `fx32`   | Fixed‑point Q16.16              | 4 bytes              |
| `fx64`   | Fixed‑point Q32.32              | 8 bytes              |

### 3.2 Composite Types

The basic declaration structure is `[attribute] [name] [type]`.

**Array declarations:**

```nihao
static fixedArray char[3]       // fixed‑size array, type is char[3]
flow dynamicArray i8[...]       // dynamic array without initial size
var initArray u16[6...]         // dynamic array with initial size

// Array indexing assignment
fixedArry[0] = 0

// Array slice assignment
dynamicArray[2..3] = {3,4}
```

**Type aliases:**

```nihao
alias Byte = u8
alias StringPtr = void[]
```

**Type definitions (struct):**

```nihao
Person struct{
    name char[]
    age u8
    flag u8:1 // bit‑field syntax supported
}
```

**Unions:**

```nihao
Data union{
    asInt i32
    asFloat f32
}
```

**Enumerations:**

```nihao
Color enum{ RED, GREEN, BLUE }
```

### 3.3 Type Operations

#### 3.3.1 Type Alignment

```nihao
// Type introspection
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
}  // 4‑byte aligned
```

#### 3.3.2 Type Nesting

```nihao
// Regular nesting
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

## 4. Variable Declarations and Visibility

### 4.1 Declaration Modifiers

| Modifier | Description                 |
| -------- | --------------------------- |
| `const`  | Fixed, immutable visibility |
| `flow`   | Dynamic visibility          |
| `static` | Static visibility           |
| `var`    | Local visibility            |

### 4.2 Examples

```nihao
const MAX_SIZE i32 = 1024
flow counter i8 = 0 
static globalVar f32 = 3.14
{var inferred char[] = "Hello"}
// Local variables do not require the var prefix; it can be omitted.
{localstr char[] = "Hello"}

// Multiple variable declarations
var {a = 0, b = 1, c = 0} i8
var {aa = "aa", bb = "bb", cc = "cc"} char[2]
var {aaa = "aaa", bbb = "bbb", ccc = "ccc"} char[]
```

## 5. Pointers and Memory Management

### 5.1 Pointer Operations

#### 5.1.1 Pointer Definition

```nihao
// Null pointers are not allowed; must be assigned upon declaration
variable i8 = 0
varptr void = &var

// Single‑level pointer
ptr void = malloc(i32)   // allocate memory
ptr.(i32) = 42           // dereference and assign
ptr?.(i64)               // safe dereference; compiler error because i64 > i32, out‑of‑bounds

// Multi‑level pointers
ptr2 void[] = &ptr       // second‑level pointer definition
ptr = ptr2.()            // one‑level dereference; when dereferencing void type, () may be omitted
variable = ptr2[].(i32)  // two‑level dereference

ptr3 void[][] ?= &ptr2   // third‑level pointer definition
ptr2 = ptr3.()           // one‑level dereference
ptr  = ptr3[].()         // two‑level dereference
variable = ptr3[][].(i32) // three‑level dereference
```

#### 5.1.2 Array Pointers

```nihao
arry char[9] = {1,2,3,4,5,6,7,8,9}
arryptr void = &arry           // get array pointer
arryptr[0] = 0
arryptr[9] = 9                 // undefined behavior
arryptr.(char[9])[0] = 0       // dereference element [0]
// arryptr.(char[9])[9] = 9    // compile error: out‑of‑bounds

arrybuffer char[8] = arryptr.(char[9])[0..7]
// arrybuffer == {0,1,2,4,5,6,7,8}

// Array of array pointers
arryptr2 void[2] = {&arry, &arrybuffer}
arryptr2[0].(char[9])[8] = arry[8]
arryptr2[1].(char[8])[7] = arrybuffer[7]
```

#### 5.1.3 Pointer Arrays

```nihao
dptrarry1 void[3] = malloc(void[3]) // dynamically allocate 1‑D pointer array
dptrarry1[2] = ptr
dptrarry1[2].(i32) += 1

dptr3 void[4][5] = malloc(void[4][5]) // dynamically allocate 2‑D pointer array
dptr3[3][4] = ptr       // safe pointer assignment
// error: dptr3[0][0].(int64) error: int64 type size > i32 type size!
dptr3[3][4].(i32) += 1  // multi‑level dereference

// Pointer to array of pointers
ptrarry void = &arryptr2
ptrarry.(void[2])[0].(char[9])[8] = 8
ptrarry.(void[2])[1].(char[8])[7] = 7

// arry == {0,1,2,4,5,6,7,8,8}
// arrybuffer == {0,1,2,4,5,6,7,7}
```

#### 5.1.4 Composite Type Pointers

```nihao
Say struct{
    name char[9]
    say char[]
}
xiaoming Say 
stptr void = &xiaoming
stptr.(char[9])[0..8] = "xiaoming"  // pointer slice assignment
stptr.(Say).say = "NiHao I am xiaoming!" // pointer type reference
talk = xiaoming.say
puts(talk)
// puts(talk) out--> "NiHao I am xiaoming!"
```

#### 5.1.5 Function Pointers

` void() ` represents a function pointer type; inside `()` you can declare the number and types of parameters. Examples:

- ` func ptr void(u8,char[]) ` – no return value
- ` func ptr void(u8,char[]) u32` – returns u32
- ` const ptr void(u8,char[]) u32` – returns u32 with const attribute
- ` flow ptr void(u8,char[]) void` – returns void with flow attribute
- ` static ptr void(u8,char[]) char[]` – returns char[] with static attribute

```nihao
func callback_handle(argc u16) {
    puts("callback call!")
}

func callback void(u16) = callback_handle

func callback_register(func cb void(u16), event u32) u32 {
    if event == 1 {
        callback = cb
    }
    return event
}

flow callback_handle_with_return(argc u16) i32 {
    puts("callback call!")
    return 42
}

flow callback2 void(u16)i32 = callback_handle_with_return

func callback_register_with_return(flow cb void(u16)i32 , event u32) u32 {
    if event == 1 {
        callback2 = cb
    }
    return event
}
```

### 5.2 Memory Introspection

```nihao
// Visibility check
if visof(ptr) == _static { 
    // ...
}

// Owner check
var boy Person = {"xiaoming", 13}
var ptr void = &boy.name
if structof(Person, ptr) == boy { 
    // ...
}
```

## 6. Control Structures

### 6.1 Conditional Statements

```nihao
if condition {
    // ...
} else if anotherCondition {
    // ...
} else {
    // ...
}
```

### 6.2 Loops

**do loop:**

```nihao
do value > 0 {
    value++
    if value == 100 {
        break
    }
    else if value == 50 {
      continue
    }
}
```

**while loop (with pattern matching):**

```nihao
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

**for loop:**

```nihao
for i = 0; i < 10; i++ {
    // ...
}
```

## 7. Function Definitions

### 7.1 Function Declarations

```nihao
// No parameters, no return value
func greet() {
    print("Hello")
}

// With parameters and return value
func add(a i8, b i8) i8 {
    return a + b
}

Person struct {
    id u32
    name char[]
}

// Function returning a void pointer with flow attribute
flow create(id u32) void {
    flow person void = malloc(sizeof(Person))
    person.(Person).id = id
    return person
}

// Function returning a void pointer with const attribute
const create(id u32) void {
    static xxx Person = {32, "xxx"}
    return &person
}

// Function returning a void pointer with static attribute
static get_arry(size u32) void {
    static arry char[u32]
    return &arry
}

// Function with function attributes
[[static]]
func getCounter() u32 {
    static counter u32
    return (counter + 1)
}

[[inline]]
const sub(a u8, b u8) {
    return (a+b)
}

[[weak]]
flow newCard(id u32) void {
    flow card struct {
        id u32
        name char[]
    } = {id, "obj"}
    return &card
}
```

## 8. Module System

### 8.1 Module Definition

```nihao
module mathUtils

func add(a i32, b i32) i32 {
    return a + b
}
```

### 8.2 Module Usage

```nihao
use mathUtils
```

### 8.3 Library Linking

```nihao
link "libc.so" libc
```

## 9. Compilation Directives

### 9.1 Common Commands

```bash
nihao init     # Initialize project
nihao build    # Build project
nihao run      # Build and run
nihao debug    # Build in debug mode
```

### 9.2 Compile‑time Execution

```nihao
cooking {
    // Code executed at compile time
    const BUILD_TIME = time.now()
}
```

## 10. Example Program

```nihao
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

func main() 
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
     * they will be automatically freed.
    */
}

func calculate() multireturn  
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

```nihao
// Safe pointer assignment
flow safePtr void ?= ptr

// Equivalent to:
if visof(ptr) == _flow {
    safePtr = ptr
}
```

## 12. NiHao Visibility System and Lifetime Management

Visibility is categorized by variable attributes.

![Visibility definition.png](Visibility%20definition.png)

Pointer assignment visibility compatibility matrix (within intersecting scopes):

| Source \ Target | const | static | flow  | local |
| --------------- | ----- | ------ | ----- | ----- |
| const           | safe  | error  | error | error |
| static          | safe  | safe   | error | error |
| flow            | safe  | error  | safe  | error |
| local           | safe  | error  | error | safe  |

### 12.1 Storage Duration and Scope Combination Rules

```nihao
// Storage duration and scope rules
const MAX_SIZE i32 = 1024     // static storage duration, global scope
static counter i32 = 0        // static storage duration, module scope  
flow dynamic_var i32 = 42     // dynamic storage duration, dynamic scope

// Local block
{
    local_var i32 = 100       // automatic storage duration, local scope
}

// Pointer safety transfer rules
func safe_pointer_operations() {
    // Safe transfer: same scope or longer‑lived scope
    flow ptr1 void = &dynamic_var     // safe: flow -> flow
    static ptr2 void = &counter       // safe: static -> static

    // Unsafe transfer: shorter‑lived to longer‑lived
    // static ptr3 void = &local_var   // error: local variable cannot be assigned to static pointer
    // const ptr4 void = &dynamic_var  // error: flow cannot be assigned to const
}
```

### 12.2 Visibility‑checked Safe Transfer

```nihao
// Safe pointer assignment operator ?= visibility check rules
func visibility_checks() {
    var varconst u32
    var source_ptr = &varconst

    const target_ptr void

    {
        var undefptr void
        source_ptr = undefptr
    }

    // Safe assignment: check visibility compatibility
    target_ptr ?= source_ptr  // equivalent to the following check:

    // Compile‑time generated check logic
    if visof(source_ptr) == _flow && visof(target_ptr) == _flow {
        target_ptr = source_ptr  // flow -> flow safe
    }
    else if visof(source_ptr) == _static && visof(target_ptr) == _static {
        target_ptr = source_ptr  // static -> static safe
    }
    else if visof(source_ptr) == _const && visof(target_ptr) == _const {
        target_ptr = source_ptr  // const -> const safe
    }
    else if visof(source_ptr) == _var && visof(target_ptr) == _var {
        target_ptr = source_ptr  // var -> var safe
    }
    else if visof(source_ptr) == _flow && visof(target_ptr) == _const {
        target_ptr = source_ptr  // flow -> const safe
    }
    else if visof(source_ptr) == _static && visof(target_ptr) == _const {
        target_ptr = source_ptr  // static -> const safe
    }
    else if visof(source_ptr) == _const && visof(target_ptr) == _const {
        target_ptr = source_ptr  // const -> const safe
    }
    else if visof(source_ptr) == _var && visof(target_ptr) == _const {
        target_ptr = source_ptr  // var -> const safe
    }
    else if visof(source_ptr) == _undef {
        panic("undefined pointer cannot be assigned")
    }
    else {
        panic("incompatible visibility pointer assignment")
    }
}
```

## 13. Visibility‑based Pointer Safety System

### 13.1 Visibility Matrix for Pointer Transfers

```nihao
// Pointer assignment visibility compatibility matrix (within intersecting scopes)
// Source -> Target    const    static    flow    local
// const                safe     error     error   error
// static               safe     safe      error   error  
// flow                 safe     error     safe    error
// local                safe     error     error   safe

func demonstrate_rules() {
    const GLOBAL i32 = 100
    static MODULE_VAR i32 = 200
    flow DYNAMIC_VAR i32 = 300
    LOCAL_VAR i32 = 400

    // Safe examples
    flow ptr1 void ?= &DYNAMIC_VAR     // flow -> flow: safe
    const ptr6 void ?= &MODULE_VAR    // static -> const: safe

    // Error examples (compile‑time)
    // static ptr5 void ?= &DYNAMIC_VAR  // flow -> static: error
    // static ptr7 void ?= &LOCAL_VAR    // local -> static: error
}
```

### 13.2 Visibility Constraints on Function Parameters

```nihao
// Function parameter visibility annotations
func process_static_data(static ptr void) i32 {
    // Only accepts static pointers
    return ptr.(i32)
}

func process_dynamic_data(flow ptr void) i32 {
    // Only accepts flow pointers
    return ptr.(i32)
}

// Return value visibility constraints
static get_static_pointer() void {
    static data i32 = 42
    return &data  // returns static pointer
}

flow get_dynamic_pointer() void {
    flow data i32 = 42
    return &data  // returns flow pointer
}

func example_usage() {
    static static_ptr void = get_static_pointer()
    flow dynamic_ptr void = get_dynamic_pointer()

    // Safe calls
    process_static_data(static_ptr)     // static -> static: safe
    process_dynamic_data(dynamic_ptr)  // flow -> flow: safe
}
```

## 14. Dynamic Scope Lifetime Management

### 14.1 Flow Variable Scope Inference

```nihao
// Compiler automatically infers the scope of flow variables
func scope_demonstration() {
    flow var1 i32 = 10

    if condition {
        flow var2 i32 = 20
        flow ptr1 void ?= &var1     // safe: var1 scope contains var2
        flow ptr2 void ?= &var2     // safe: same scope

        // var2's scope ends here
    }

    // ptr2 cannot be used here because var2 has left scope
    flow ptr3 void ?= &var1         // safe: var1 still in scope
}

// Nested scope lifetime checking
func nested_scopes() {
    flow outer_var i32 = 100

    {
        flow inner_var i32 = 200
        flow inner_ptr void ?= &outer_var  // safe: outer scope contains inner
        flow outer_ptr void ?= &inner_var  // safe: same function scope
    }

    // After leaving inner scope, inner_var is invalid
    // outer_var remains valid
}
```

### 14.2 Cross‑Function Call Scope Management

```nihao
// Scope transfer across function calls
func caller_function() {
    flow local_dynamic i32 = 42
    flow result i32 = process_with_callback(local_dynamic, &callback_function)
}

flow process_with_callback(flow data i32, 
                           flow callback void(i32)i32
                           ) i32 {
    // data and callback are both flow, ensuring lifetime compatibility
    return callback(data)
}

func callback_function(value i32) i32 {
    return value * 2
}
```

## 15. Safe Pointer Operation Patterns

### 15.1 Visibility‑based Safe Dereference

```nihao
// Safe dereference operator ! visibility check
func safe_dereference_examples() {
    flow dynamic_ptr void = &some_flow_variable
    static static_ptr void = &some_static_variable

    // Safe dereference
    value1 = dynamic_ptr?.(i32)     // safe dereference of flow pointer
    value2 = static_ptr?.(i32)     // safe dereference of static pointer

    // Safe dereference equivalent to:
    if visof(dynamic_ptr) != _undef && dynamic_ptr != null {
        value1 = dynamic_ptr.(i32)
    } else {
        panic("unsafe dereference")
    }
}
```

### 15.2 Safe Access to Arrays and Structs

```nihao
// Struct definition
Person struct {
    name char[]   // dynamic string
    age i32       // static age
}

flow some_person Person
dynamic_array u8[10...]

func safe_structure_access() {
    flow person_ptr void = &some_person

    // Safe access to struct fields
    flow   name_ptr void ?= person_ptr.(Person).name  // flow field safe transfer
    static age_ptr  void ?= person_ptr.(Person).age   // static field safe transfer

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

```nihao
// Detailed visibility error messages
func demonstrate_visibility_errors() {
    flow dynamic_var i32 = 42
    static static_var i32 = 100

    // Trigger visibility error with diagnostic info
    static error_ptr void ?= &dynamic_var  
    // Compile error: cannot assign flow visibility (_flow) to static visibility (_static)
    // Reason: static pointer may outlive the flow variable, causing dangling pointer
}

// Runtime visibility check
func runtime_visibility_check(ptr void) {

    while visof(ptr) {
        is _const => puts("constant pointer, global lifetime")
        is _static => puts("static pointer, module lifetime") 
        is _flow => puts("dynamic pointer, needs scope analysis")
        is _var => puts("local pointer, needs scope analysis")
        is _undef => puts("undefined or invalid pointer")
        break
    }
}
```

### 16.2 Scope Debugging Tools

```nihao
// Compile‑time scope analysis report
func analyzed_function() {
    flow var1 i32 = 10        // [scope: function‑level]
    {
        flow var2 i32 = 20    // [scope: undetermined]
        var3 i32 = 30    // [scope: block‑level]
    }
    // [warning: var2, var3 leaving scope]

    /* [warning: var2 scope change]
        function {
            {---------------scope start

            }---------------scope end old

        }-------------------scope end new
    */
    var4 i32 = var2; 
    // [compile error: var3 left scope --> undefined]
    var4 = var3;

    // [warning: tracked_var scope change]
    flow ptrvar1 void = scope_tracing_example()

    // [compile error: tracked_var can only be assigned to flow variable]
    ptrvar2 void = scope_tracing_example()
}

flow scope_tracing_example() i32{
    flow tracked_var i32 = 42

    return &tracked_var  // enable scope tracing
    // In debug mode, record scope entry/exit
}
```

## 17. Practical Application Examples

### 17.1 Complete Module Example

```nihao
module security_module

// Module‑level static data
static module_counter i32 = 0
const MAX_CONNECTIONS i32 = 1000

// Secure data processor
SecurityProcessor struct {
    config static ConfigData
    state flow ProcessorState
}

flow process_request(flow self SecurityProcessor, flow request Request) Response {
    // Safe state access
    self.state.current_request ?= request

    // Static configuration access
    if self.state.connection_count < self.config.(ConfigData).max_connections {
        self.state.connection_count++
        return create_response(200, "OK")
    }

    return create_response(429, "Too Many Requests")
}

func main() {
    flow processor SecurityProcessor = create_processor()
    flow request Request = receive_request()

    // Safe method call
    flow response Response = processor.process_request(request)
    send_response(response)
}
```

### 17.2 Memory Safety Patterns

```nihao
// Visibility‑based memory safety patterns
func memory_safe_patterns() {
    // Pattern 1: dynamic data processed within a closed scope
    {
        flow temporary_data Data = load_temporary_data()
        process_data(temporary_data)  // data automatically cleaned after processing
    }

    // Pattern 2: static data held long‑term
    static persistent_cache Cache = initialize_cache()
    use_cache(persistent_cache)

    // Pattern 3: safe data transfer chain
    flow source_data Data = acquire_data()
    flow processed_data Data = transform_data(source_data)
    flow result Result = analyze_data(processed_data)
    // All flow data automatically cleaned at function exit
}
```

## Summary

This design leverages NiHao's existing visibility system to achieve memory safety through:

1. **Clear storage duration and scope**: const/static/flow/local variables have well‑defined lifetime rules.
2. **Visibility‑checked pointer safety**: the `?=` operator verifies visibility compatibility at assignment.
3. **Dynamic scope management**: flow variable scopes are automatically inferred by the compiler.
4. **Gradual safety**: from local safety to module safety and finally to global safety.

Key advantages:

- No complex ownership system introduced.
- Utilizes existing visibility modifiers.
- Combines compile‑time and run‑time safety checks.
- Highly consistent with NiHao's design philosophy.

This maintains language simplicity while providing powerful memory safety guarantees.

---

*NiHao v1.0 Language Specification - © 2025 NiHao Development Team*

```

```

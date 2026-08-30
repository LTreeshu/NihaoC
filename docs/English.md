# NiHao Programming Language Reference Manual

> **English version.** This document is a full translation of the Chinese specification [`Chinese.md`](./Chinese.md), which is the authoritative reference. If any discrepancy arises, the Chinese version prevails.
>
> Version: v2.0 (2026-08-05)

## 1. Overview

NiHao is a new statically compiled language designed for system-level programming and high-performance applications, combining modern language features with low-level control.

## 2. Basic Syntax

### 2.1 Comments

```nihao
// single-line comment
/* multi-line comment */
```

### 2.2 Statement Termination

- Statements are separated by a newline or `;` — both are optional delimiters.
- Multiple statements on the same line are separated by `;`: `stmt1; stmt2`
- `#` is also a valid statement terminator.

### 2.3 Built-in Functions

- `typeof(type)` — type inspection, returns the type
- `sizeof(type)` — size inspection, returns the size
- `alignof(type)` — alignment inspection, returns the alignment size
- `structof(member)` — ownership inspection, returns the owning struct
- `unionof(member)` — ownership inspection, returns the owning union
- `offsetof(type,member)` — returns the byte offset
- `bitoffsetof(type,bitmember)` — returns the bit offset
- `holdof(type, member)` — returns the base address of the enclosing aggregate
- `visof(var)` — visibility inspection, returns the visibility attribute
- `len(x)` — logical length (2026-08-19): array = capacity; dynamic `char[]` = literal length;
  slice variable `s = arr[lo..hi]` = boundary difference `hi-lo` (bounds must be compile-time
  constants; returns a compile-time value)

### 2.4 Keyword Reference

- `alias` — type alias
- `const` — fixed-visibility immutable modifier
- `flow` — dynamic-visibility modifier
- `static` — static-visibility modifier
- `var` — local-visibility modifier, type may be inferred
- `_undef` — visibility enum: undefined / invisible
- `_const` — visibility enum: const (fixed, immutable)
- `_flow` — visibility enum: flow (dynamic)
- `_static` — visibility enum: static
- `_var` — visibility enum: var (local)
- `cooking {...}` — compile-time execution block
- `align n {...}` — byte-alignment block
- `use ...` — module import
- `module ...` — module definition
- `linkas "..."` — static-library export naming
- `link "..." ...` — static-library import
- `func` — no-return-value attribute and no-return function definition
- `[[inline]]` — inline function attribute
- `[[weak]]` — weak-definition function attribute
- `[[local]]` — internal-linkage function attribute
- `[[used]]` — force-keep function attribute
- `[[unused]]` — deprecation function attribute
- `[[export] ".my_section"]` — export to a specific section

## 3. Type System

### 3.1 Primitive Types

| Type    | Description                 | Size    |
| ------- | --------------------------- | ------- |
| `void`  | Generic pointer type        | machine pointer size |
| `char[]`| String type                 | dynamic |
| `char`  | Character type              | 1 byte  |
| `u8`    | Unsigned 8-bit integer      | 1 byte  |
| `u16`   | Unsigned 16-bit integer     | 2 bytes |
| `u32`   | Unsigned 32-bit integer     | 4 bytes |
| `u64`   | Unsigned 64-bit integer     | 8 bytes |
| `i8`    | Signed 8-bit integer        | 1 byte  |
| `i16`   | Signed 16-bit integer       | 2 bytes |
| `i32`   | Signed 32-bit integer       | 4 bytes |
| `i64`   | Signed 64-bit integer       | 8 bytes |
| `f32`   | Single-precision float      | 4 bytes |

> f32 strict width (implemented 2026-08-19): values are rounded to single precision on
> assignment/initialization (storage-truncation semantics); arithmetic still promotes to
> double. `f32 x = 0.1` stores back so that `x != 0.1` (f64 literal).
| `f64`   | Double-precision float      | 8 bytes |
| `fx32`  | Fixed-point (Q16.16)        | 4 bytes |
| `fx64`  | Fixed-point (Q32.32)        | 8 bytes |

C-compatible types also exist: `string` (alias of `char[]`), `short`, `int`, `long`, `float`, `double`, `bool`.

### 3.2 Composite Types

General declaration form: `[attribute] [name] [type]`

**Array declarations:**

```nihao
static fixedArray char[3]       // fixed-size array; char[3] is the type
flow dynamicArray i8[...]   // dynamic array (1.0 semantics: declare/index only, no auto-grow; growth deferred to 2.0)
var initArray u16[6...]   // array with initial capacity (1.0 semantics: fixed capacity 6, no auto-grow)

// indexed access & assignment
fixedArry[0] = 0

// slice access & assignment
dynamicArray[2..3] = {3,4}
```

**Type aliases:**

```nihao
alias Byte = u8
alias StringPtr = char[]
```

**Struct definitions:**

```nihao
Person struct{
    name char[]
    age u8
    flag u8:1 // bit-field syntax supported
}
```

**Union definitions:**

```nihao
Data union{
    asInt i32
    asFloat f32
}
```

**Enum definitions:**

```nihao
Color enum{ RED, GREEN, BLUE }
```

### 3.3 Type Operations

#### 3.3.1 Type Alignment

```nihao
// type inspection
if typeof(value) == i32 { ... }

// type size
size u8 = sizeof(Person)

// type alignment
align 4 {
  Protocol struct {
    data u8
    len  u32
    flag u32:1 
    tag  u32:2
  }
}  // 4-byte alignment
```

#### 3.3.2 Type Nesting

```nihao
// ordinary nesting
aunion union{
    value u16
    reg struct{
        r0 u8
        r1 u8
    }
}
aunion.reg.r1 = 1

// anonymous nesting
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

**Named-type nesting (implemented 2026-08-19)** — chained member access + whole-struct copy:

```nihao
Point struct { x i32 y i32 }
Line struct { a Point b Point }

l Line
l.a.x = 1
l.b.y = 2            // chained access (recursively expanded offsets)

m Line
m = l                // whole-struct copy (per-member, nested recursion; copies are independent)

n Line = {{1, 2}, {3, 4}}   // nested initializer list (recursive fill, 2026-08-26)

// union nesting: aggregate members share slots (total slots = largest member, 2026-08-27)
U union { a Point b Point }
un U
un.a.x = 1
un.b.y = 5          // writing b.y overwrites the shared slot → un.a.y == 5
```

## 4. Variable Declarations and Visibility

### 4.1 Declaration Modifiers

| Modifier  | Description                   |
| --------  | ----------------------------- |
| `const`   | Fixed-visibility immutable variable |
| `flow`    | Dynamic-visibility variable   |
| `static`  | Static-visibility variable    |
| `var`     | Local-visibility variable     |

### 4.2 Examples

```nihao
const MAX_SIZE i32 = 1024
flow counter i8 = 0 
static globalVar f32 = 3.14
{var inferred char[] = "Hello"}
// the `var` prefix is not mandatory for local variables
{localstr char[] = "Hello"}

// multi-variable declaration
var {a = 0,b = 1,c = 0} i8
var {aa = "aa",bb = "bb",cc = "cc"} char[2]
var {aaa = "aaa",bbb = "bbb",ccc = "ccc"} char[]
```

## 5. Pointers and Memory Management

### 5.1 Pointer Operations

#### 5.1.1 Pointer Definition

```nihao
// empty pointers are not allowed; a pointer must be initialized at declaration
variable i8 = 0
varptr void = &var

// single-level pointer
ptr void = malloc(i32)   // allocate memory
ptr.(i32) = 42           // dereference & assign
ptr?.(i64)               // safe dereference — compile error: i64 > i32, out of bounds

// multi-level pointer
ptr2 void[] = &ptr       // pointer-to-pointer
ptr = ptr2.()            // one-level dereference; type may be omitted for void
variable = ptr2[].(i32)  // two-level dereference

ptr3 void[][] ?= &ptr2    // pointer-to-pointer-to-pointer
ptr2 = ptr3.()            // one-level dereference
ptr  = ptr3[].()          // two-level dereference
variable = ptr3[][].(i32) // three-level dereference
```

> **Pointer declaration syntax (decided 2026-08-19: implicit inference + explicit declaration both supported)**
>
> - **Implicit inference**: `p = &x` auto-infers `p` as a pointer to `x`'s type (no type name needed).
> - **Explicit declaration**: `p T* = &x` is also legal (`T*` named pointer type; `parse_type` natively supports `*`, cgen emits `T*`).
> - **`->` pointer member access**: `p->field` is equivalent to `(*p).field`; chained `p->a->b` and compound assignment `p->n += 1` are supported (aligned in both A-plan and IR layers since 2026-08-19).

#### 5.1.2 Array Pointers

```nihao
arry char[9] = {1,2,3,4,5,6,7,8,9}
arryptr void = &arry           // pointer to an array
arryptr[0] = 0
arryptr[9] = 9                 // undefined behavior
arryptr.(char[9])[0] = 0       // dereference member [0]
// arryptr.(char[9])[9] = 9       // compile error: out of bounds

arrybuffer char[8] = arryptr.(char[9])[0..7]
// arrybuffer == {0,1,2,4,5,6,7,8}

// array of array pointers
arryptr2 void[2] = {&arry,&arrybuffer}
arryptr2[0].(char[9])[8] = arry[8]
arryptr2[1].(char[8])[7] = arrybuffer[7]
```

#### 5.1.3 Pointer Arrays

```nihao
dptrarry1 void[3] = malloc(void[3]) // dynamically allocate a 1-D pointer array
dptrarry1[2] = ptr
dptrarry1[2].(i32) += 1

dptr3 void[4][5] = malloc(void[4][5]) // dynamically allocate a 2-D pointer array
dptr3[3][4] = ptr       // safe pointer transfer
// error: dptr3[0][0].(int64) error: int64 type size > i32 type size!
dptr3[3][4].(i32) += 1  // multi-level dereference

// pointer to pointer array
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
stptr.(Say).say = "NiHao I am xiaoming!" // typed pointer reference
talk = xiaoming.say
puts(talk)
// puts(talk) out--> "NiHao I am xiaoming!"
```

#### 5.1.5 Function Pointers

**Type syntax:**
A function pointer type is written as `void(parameter-type-list)`. If the function has a return value, append the return type at the end. Examples:

- `void(u8, char[])` — a function pointer taking `u8` and `char[]`, returning nothing;
- `void(u8, char[]) i32` — a function pointer returning `i32`;
- If the return value is a generic pointer (`void` pointer), write `void` in the return position, and the declaring variable **must** carry the matching attribute prefix (`flow`/`static`/`const`) to declare the memory category, e.g. `flow cb void(u8) void` is a function pointer returning dynamic memory. The attribute of a function pointer variable must match the attribute of its return value.

**Variable declaration:**
It follows the general form `[attribute] [name] [type]`:

```nihao
var call void(i32, i32)        // no-return function pointer
var add_cb void(i32, i32) i32  // function pointer returning i32
flow factory void(char[]) void  // function pointer returning dynamic memory
static loader void() void       // function pointer returning static memory
const getter void() void        // function pointer returning a read-only pointer
```

**Assignment and invocation:**

```nihao
// target function (no return value)
func callback_handle(argc u16) {
    puts("callback call!")
}
// assign to a function pointer variable (type must match)
var cb void(u16) = callback_handle
// invoke
cb(100)

// function pointer with a return value
func add(a i32, b i32) i32 { return a + b }
var calc void(i32, i32) i32 = add
result = calc(10, 20)  // result == 30

// function pointer returning dynamic memory (must be received with flow)
flow create_user(name char[]) void {
    flow user void = malloc(sizeof(User))
    user.(User).name = name
    return user
}
flow factory void(char[]) void = create_user
flow user = factory("Alice")  // lifetime managed automatically

// function pointer returning static memory (receive with static or const)
static get_config() void {
    static config Config = { .port = 8080 }
    return &config
}
static loader void() void = get_config   // static → static ✅ recommended
const config_reader void() void = get_config  // static → const ✅ read-only borrow
// var bad_loader void() void = get_config  // static → var ❌ compile error

// function pointer returning a read-only pointer (must be received with const)
const get_version() void {
    static ver char[] = "v2026"
    return &ver
}
const version_getter void() void = get_version
```

**Function pointers as parameters:**

```nihao
// register a callback (parameter type written directly as void(u32))
func register_callback(cb void(u32)) {
    global_cb = cb
}

// callback parameter returning dynamic memory (must be received with flow)
func process_async(flow handler void(i32) void) {
    flow data = handler(100)
    // ...
}

// invocation
func my_handler(x i32) void { return malloc(16); }
process_async(my_handler)   // flow attribute matched automatically
```

**Function pointer arrays (`void(...)` followed directly by `[n]`):**

```nihao
// no-return function pointer array
func handle_a(u8) { puts("A"); }
func handle_b(u8) { puts("B"); }
var table void(u8)[2] = { handle_a, handle_b }
table[0](1)  // calls handle_a

// array of function pointers returning dynamic pointers (every element is a flow callback)
flow create_packet(u8) void { return malloc(64); }
flow create_frame(u8) void { return malloc(128); }
flow dispatcher void(u8)void[2] = { create_packet, create_frame }
flow packet = dispatcher[0](0x01)
```

**Aliases simplify complex types:**

```nihao
alias Callback = void(u8, i32) i32
alias AsyncFactory = void(char[]) void
alias ConfigLoader = void() void

var handler Callback = some_function
flow factory AsyncFactory = create_async
static loader ConfigLoader = get_config
```

**Receiving rules for function pointers (mandatory, per §7.3):**

- If the function pointer type is `void(...) void` (returning a `void` pointer), the variable prefix must match the return attribute: `flow` for dynamic pointers, `static` for static pointers, `const` for read-only pointers.
- If the return type is non-pointer (e.g. `i32`, `char[]`) or there is no return value, the prefix may be `func` or `var` (or omitted).
- Error example (compile error): `flow bad void() void = get_static_ptr` (a static return assigned to `flow` is rejected).

### 5.2 Memory Introspection

```nihao
// visibility check
if visof(ptr) == _static { 
    // ...
}

// ownership check
var boy Person = {"xiaoming", 13}
var ptr void = &boy.name
if structof(Person,ptr) == boy { 
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
    else if value == 50{
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

**goto and labels (2026-08-19):**

```nihao
i i32 = 0
loop:
i = i + 1
if i < 3 {
    goto loop        // jump to label (defined before or after, unique per function)
}
```

## 7. Function Definitions

### 7.1 Function Declaration Syntax

#### 7.1.1 Core Rules

> **All function declarations follow a uniform layout:**
>
> ```
> [bracket-attributes] [return-attribute] name(parameter-list) [return-type] { body }
> ```
>
> - **Bracket attributes** (optional): modify linkage or compiler behavior, e.g. `[[local]]`, `[[inline]]`
> - **Return attribute** (required): determines the visibility/lifetime of the return value (or the function itself)
> - **Return type**: write `void` when returning a `void` pointer; write the concrete type for non-pointer returns; may be omitted when there is no return value

#### 7.1.2 Return-Attribute Selection Rules

| Return case                                     | Required attribute | Description                        |
| ----------------------------------------------- | ------------------ | ---------------------------------- |
| No return value                                 | `func`             | ordinary function                  |
| Returns non-pointer (e.g. `i32`, `f64`, `Person`) | `func`             | ordinary return value              |
| Returns `void` pointer (dynamic memory)          | `flow`             | caller owns it; auto-released      |
| Returns `void` pointer (static storage)          | `static`           | lifetime is the whole program      |
| Returns `void` pointer (read-only)               | `const`            | pointed-to data is immutable       |

#### 7.1.3 Bracket Attributes (Linkage & Compiler Behavior)

| Attribute                | Effect                        | Use case                       |
| -----------------------  | ----------------------------- | ------------------------------ |
| `[[local]]`              | internal linkage (file-scope) | private helper functions       |
| `[[inline]]`             | suggest inlining              | short, hot functions           |
| `[[weak]]`               | weak symbol, overridable      | library default implementations|
| `[[used]]`               | keep symbol even if unused    | functions called from asm/debuggers |
| `[[unused]]`             | deprecated, emit a warning    | legacy transitional APIs       |
| `[[export] ".section"]`  | export to a specific section  | special sections for linker scripts |

**Combination rule:** bracket attributes and the return attribute may coexist; bracket attributes come first.

---

### 7.2 Complete Function Modifier Table

| Scenario                 | Example                                              | Description                       |
| ------------------------ | --------------------------------------------------- | --------------------------------- |
| external linkage, no return | `func greet() { ... }`                             | most common no-return function (default external linkage) |
| external linkage, non-pointer return | `func add(a i8, b i8) i8 { ... }`            | returns a plain type |
| external linkage, dynamic pointer | `flow create_buffer(size u32) void { ... }`    | returns dynamic memory |
| external linkage, static pointer | `static get_counter() void { ... }`            | returns address of a static variable |
| external linkage, read-only pointer | `const get_version() void { ... }`          | returns read-only data |
| internal linkage, dynamic pointer | `[[local]] flow create() void { ... }`        | file-scope, returns dynamic memory |
| internal linkage, static pointer | `[[local]] static get() void { ... }`          | file-scope, returns static memory |
| internal linkage, non-pointer | `[[local]] func helper(x i32) i32 { ... }`       | ordinary file-scope function |
| inline, dynamic pointer   | `[[inline]] flow create_small() void { ... }`      | suggest inline |
| weak, static pointer      | `[[weak]] static get_default() void { ... }`       | overridable default |
| deprecated, dynamic pointer | `[[unused]] flow old_api() void { ... }`         | deprecation warning |
| export-to-section, static pointer | `[[export]".init"] static init_data() void { ... }` | place into a specific section |

---

### 7.3 Caller Receiving Rules

The return attribute of a function determines which attribute the **caller must use** to receive the return value, guaranteeing memory safety.

| Function return attr | Allowed receiving attrs | Forbidden attrs             | Reason                                  |
| -------------------- | ----------------------- | --------------------------- | --------------------------------------- |
| `flow`               | `flow`                  | `func`, `static`, `const`   | ownership must transfer; only `flow` manages memory |
| `static`             | `static`, `const`       | `flow`, `var`               | static memory has the longest lifetime; may borrow safely but never free |
| `const`              | `const`                 | `func`, `static`, `flow`    | the read-only constraint must be preserved |

```nihao
// receiving examples
flow buf void = create_buffer(1024)      // flow → flow ✅
static p void = get_counter()            // static → static ✅
const ver void = get_version()           // const → const ✅
// flow bad = get_version()              // const → flow ❌ compile error
```

---

### 7.4 Complete Example Set

#### 7.4.1 `func` — no return or non-pointer return

```nihao
// no return value
func greet() {
    print("Hello, Nihao C!")
}

// no return value, with parameters
func log(msg char[]) {
    print("[LOG] ", msg)
}

// returns a non-pointer type
func add(a i8, b i8) i8 {
    return a + b
}

// returns a struct (non-pointer)
Person struct { name char[], age i32 }
func make_person(name char[], age i32) Person {
    return Person{name, age}
}

// returns an array
func make_array() i32[3] {
    return {1, 2, 3}
}
```

#### 7.4.2 `flow` — returns a dynamic memory pointer

```nihao
// returns a dynamic memory pointer
flow create_buffer(size u32) void {
    return malloc(size)
}

// returns a dynamic struct pointer
flow create_person(name char[], age i32) void {
    flow p void = malloc(sizeof(Person))
    p.(Person).name = name
    p.(Person).age = age
    return p
}

// internal linkage, file-scope only
[[local]] 
flow create_internal_buffer(size u32) void {
    return malloc(size)
}
```

#### 7.4.3 `static` — returns a static memory pointer

```nihao
// returns a static counter pointer
static get_counter() void {
    static count i32 = 0
    return &count
}

// returns a static config struct pointer
static get_default_config() void {
    static config Config = {.timeout = 30, .retries = 3}
    return &config
}

// internal linkage + static pointer return
[[local]] 
static get_internal_config() void {
    static config Config = {.timeout = 10}
    return &config
}
```

#### 7.4.4 `const` — returns a read-only pointer

```nihao
// returns a read-only version string
const get_version() void {
    static version char[] = "v1.0.0"
    return &version
}

// returns read-only build info
const get_build_info() void {
    static info BuildInfo = {.date = "2025-01-01", .commit = "abc123"}
    return &info
}

// internal linkage, file-scope only
[[local]] 
const get_internal_version() void {
    static version char[] = "v1.0.0-internal"
    return &version
}
```

#### 7.4.5 Bracket Attribute Combinations

```nihao
// inline + dynamic pointer
[[inline]] 
flow create_small_buffer() void {
    return malloc(64)
}

// weak + static pointer (overridable)
[[weak]] 
static get_default_handler() void {
    static handler Handler = {.id = 0}
    return &handler
}

// used + no return (symbol kept even if uncalled)
[[used]] 
func debug_dump() {
    print("Debug dump called")
}

// unused + dynamic pointer (warning)
[[unused]] 
flow old_create() void {
    return malloc(1024)
}

// export to .init section + static pointer
[[export] ".init"] 
static get_init_data() void {
    static data InitData = {.magic = 0xDEADBEEF}
    return &data
}

// multi-combination: internal + weak + dynamic pointer
[[local, weak]] 
flow create_fast() void {
    return malloc(32)
}
```

---

### 7.5 Function Prototypes (no body)

A prototype declares the existence of a function without providing an implementation, typically used in header-like files or forward references.

```nihao
// ordinary declarations
func add(a i8, b i8) i8;
flow create_buffer(size u32) void;
static get_counter() void;
const get_version() void;

// declarations with bracket attributes
[[local]] flow create_internal() void;
[[inline]] func square(x i32) i32;
[[weak]] static get_default() void;
```

---

### 7.6 Function Pointer Types

#### 7.6.1 Syntax

Function pointer types use the `void()` form with a parameter type list and an optional return type:

```
void( parameter-type-list ) [return-type]
```

If the return type is `void` (generic pointer) with an attribute (`flow`/`static`/`const`), place the attribute before the return type.

#### 7.6.2 Examples

```nihao
// no-return function pointer: void(u16)
func callback_handle(argc u16) {
    puts("callback called")
}
var cb void(u16) = callback_handle   // cb is a function pointer variable

// function pointer with a return value: void(u16) i32
flow callback_with_return(argc u16) i32 {
    return 42
}
flow cb2 void(u16)i32 = callback_with_return

// passed as a parameter
func register_callback(flow cb void(u16)i32, event u32) u32 {
    if event == 1 {
        return cb(event)
    }
    return 0
}

// using a flow function pointer
flow async_cb void(u8)void = async_handler
```

---

### 7.7 Quick Reference Table

| Need                        | Notation                                   | Return attr | Bracket attr (optional) |
| --------------------------- | ------------------------------------------ | ----------- | ----------------------- |
| ordinary, no return         | `func greet() { ... }`                     | `func`      | —                       |
| non-pointer return          | `func add(a i8, b i8) i8 { ... }`          | `func`      | —                       |
| dynamic pointer return      | `flow create() void { ... }`               | `flow`      | —                       |
| static pointer return       | `static get() void { ... }`                | `static`    | —                       |
| read-only pointer return    | `const get() void { ... }`                 | `const`     | —                       |
| internal linkage, dynamic   | `[[local]] flow create() void { ... }`     | `flow`      | `[[local]]`             |
| internal linkage, non-pointer | `[[local]] func helper() i32 { ... }`    | `func`      | `[[local]]`             |
| inline, dynamic             | `[[inline]] flow create() void { ... }`    | `flow`      | `[[inline]]`            |
| weak, static                | `[[weak]] static get() void { ... }`       | `static`    | `[[weak]]`              |
| deprecated, dynamic         | `[[unused]] flow old() void { ... }`       | `flow`      | `[[unused]]`            |

---

> **📖 Full BNF grammar**: see [`BNF.md`](./BNF.md) for the complete Backus-Naur Form covering function declarations, the type system, statements, expressions, the module system, etc.

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
nihao init     # initialize a project
nihao build    # build the project
nihao run      # build and run
nihao debug    # debug-mode build
```

### 9.2 Compile-time Execution

```nihao
cooking {
    // code executed at compile time
    const BUILD_TIME = time.now()
    // compile-time variables: const NAME [TYPE] = expr (shared across blocks, folded at runtime)
    const BASE i32 = 10
    // compile-time functions (macro expansion, 2026-08-19): const NAME(p1, p2) = expr
    const sq(x) = x * x
    static_assert(sq(5) == 25, "sq(5) != 25")   // nested sq(sq(2)) / compose sq(cube(2)) supported
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

/* Multi return: named struct (same mechanism as C) */
MultiResult struct {
    value1 u8
    value2 u8
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
    returnValue MultiResult = calculate()

    return
    /* If the flow variable: dynptr, temp, is not returned,
     * they will be automatically free.
    */
}

func calculate() MultiResult  
{
    if visof(value) != _undef {
      return {0,0}
    }
    else if visof(ConstValue) == _static {
      return {ConstValue, (ConstValue*2)}
    }
}
```

## 11. Visibility System and Pointer Lifetime Management

NiHao manages variable storage duration, ownership, and borrowing through a unified attribute system to guarantee memory safety. This chapter integrates the visibility (storage-duration) system with pointer ownership/borrowing rules into a complete static-analysis framework.

---

### 11.1 Variable Storage-Duration Attributes (Visibility)

Every variable declaration must specify exactly one of the four storage-duration attributes below, controlling its lifetime and scope:

| Attribute | Storage | Mutability | Scope         | Typical use               |
| --------- | ------- | ---------- | ------------- | ------------------------- |
| `const`   | static  | immutable  | global/module | constants, read-only config |
| `static`  | static  | mutable    | module-level  | module-shared state       |
| `flow`    | dynamic | mutable    | function/block | dynamic allocation (owns memory) |
| `var`     | automatic | mutable  | block-level   | local variables (automatic storage) |

```nihao
const MAX_SIZE i32 = 1024     // static storage, globally read-only
static counter i32 = 0        // static storage, module-mutable
flow dynamic_data i32 = 42    // dynamic storage, function-mutable
var local_temp i32 = 100      // automatic storage, block-mutable
```

---

### 11.2 Storage-Duration Compatibility Matrix

On assignment, **the target's storage duration must be no shorter than the source's** (target lifetime ≥ source lifetime); otherwise a dangling pointer may result. The compiler performs static checks according to the following matrix:

| Source \ Target | `const` | `static` | `flow` | `var` |
| --------------- | ------- | -------- | ------ | ----- |
| `const`         | safe    | error    | error  | error |
| `static`        | safe    | safe     | error  | error |
| `flow`          | safe    | error    | safe   | error |
| `var`           | safe    | error    | error  | safe  |

- **safe**: assignment allowed; the compiler's lifetime check passes.
- **error**: the target lifetime may be shorter than the source; assignment is forbidden (compile error).

For example, a `flow` pointer cannot be assigned to a `static` pointer, because `flow` may be destroyed at function exit while `static` needs to hold it long-term. `static` may be assigned to `const` (read-only borrow) but not to `var` or `flow` (avoiding mutable borrows or transfers that mismatch lifetimes).

---

## 12. Pointer Ownership and Borrowing Rules

Pointer transfer is governed not only by storage duration but also by **ownership** and **borrowing** semantics. NiHao classifies pointers into two kinds:

- **Owning pointers**: `flow` (dynamic) and `static` (static) — they manage the pointed-to data (freeing or persisting it).
- **Borrowing pointers**: `var` (mutable borrow) and `const` (read-only borrow) — they do not own the data; their lifetime is bounded by the source pointer.

### 12.1 Core Transfer Rules (for `void` pointers)

For an owning `flow` pointer, transfers to other attributes follow these rules (source-state changes):

| Source → Target (both `void`) | Semantics    | Source state |
| ----------------------------- | ------------ | ------------ |
| `flow` → `var`                | mutable borrow | frozen      |
| `flow` → `const`              | read-only borrow | frozen    |
| `flow` → `flow`               | ownership transfer | invalidated |
| `flow` → `static`             | **forbidden** | —            |
| `var` → `var`                 | mutable borrow | frozen      |
| `var` → `const`               | read-only borrow | frozen    |
| `var` → `flow`                | **forbidden** | —            |
| `const` → `const`             | read-only borrow | stays valid |
| `const` → `var`               | **forbidden** | —            |
| `const` → `flow`              | **forbidden** | —            |
| `static` → `const`            | read-only borrow | stays valid |
| `static` → `static`           | shared reference | stays valid |
| `static` → `var`              | **forbidden** | —            |
| `static` → `flow`             | **forbidden** | —            |

> **frozen**: the source pointer may not be read or written while borrowed (like Rust's immutable borrow).
> **invalidated**: the source pointer may no longer be used (ownership has transferred).
> **stays valid**: the source remains usable, unchanged.

### 12.2 Combined Transfer Matrix (storage duration + ownership/borrowing)

Merging the storage-duration compatibility matrix with the rules above yields the complete pointer-transfer table. Each cell reads: `allowed/forbidden (source-state change)`.

| Source \ Target | `const`             | `static`           | `flow`              | `var`            |
| --------------- | ------------------- | ------------------ | ------------------- | ---------------- |
| `const`         | allowed (stays valid)| forbidden          | forbidden           | forbidden        |
| `static`        | allowed (stays valid, read-only borrow) | allowed (stays valid, shared) | forbidden | forbidden |
| `flow`          | allowed (frozen, read-only borrow) | forbidden | allowed (invalidated, ownership transfer) | allowed (frozen, mutable borrow) |
| `var`           | allowed (frozen, read-only borrow) | forbidden | forbidden           | allowed (frozen, mutable borrow) |

This table is the basis for the compiler's static analysis, ensuring every pointer operation satisfies both lifetime requirements and ownership/borrowing semantics.

---

## 12.3 Transfer Rules for Function Parameters and Return Values

### Parameters

The attribute of a function parameter determines how arguments are passed:

```nihao
// parameter is flow: takes ownership; freed when the function returns
func consume(flow val void) { ... }

// parameter is var: mutable borrow; the argument is frozen
func modify(var val void) { ... }

// parameter is const: read-only borrow; the argument is frozen
func inspect(const val void) { ... }
```

At a call site, the compiler applies the corresponding state change per the parameter attribute:

```nihao
flow p void = malloc(i32)
consume(p)   // p invalidated (ownership transfer)

flow q void = malloc(i32)
modify(q)    // q frozen (mutable borrow)
inspect(q)   // q frozen (read-only borrow)
```

### Return values

- Returning a `flow` pointer: ownership transfers to the caller (the caller is responsible for freeing).
- Returning a `static` pointer: returns a static address; the caller obtains a shared reference.
- Returning a `var` or `const` borrowed pointer: the borrow must remain valid (lifetime no shorter than the caller's scope).

```nihao
flow create_ptr() void {
    flow ptr void = malloc(i32)
    ptr.(i32) = 100
    return ptr   // ownership transfers to the caller
}

static get_static_ptr() void {
    static data i32 = 42
    return &data  // returns a static pointer
}
```

---

## 13. Lifetime Management (Scope Inference)

### 13.1 Scopes and Borrow Validity

The lifetime of a borrowed pointer (`var`/`const`) must be contained within the validity of its source pointer. The compiler infers this automatically through nested-scope analysis:

```nihao
func demo() {
    flow outer void = malloc(i32)   // outer's scope is the whole function
    {
        var inner void = outer      // mutable borrow; outer frozen
        // inner is valid only inside this block
    }                          // inner destroyed; outer unfrozen
    // outer can be used again here
}
```

A borrowed pointer may not outlive the scope of its source:

```nihao
flow source = malloc(i32)
{
    var borrow void = source
    // error: cannot return borrow to an outer scope
    // flow leaked void = borrow   // compile error: borrow cannot transfer ownership
}
// borrow destroyed; source may be used again
```

### 13.2 Cross-Function-Call Lifetimes

When a function receives a borrowed parameter, the compiler ensures the argument stays valid for the duration of the call:

```nihao
func use_borrow(var ptr void) {
    // ptr is valid inside the function
}

func caller() {
    flow data void = malloc(i32)
    use_borrow(data)   // data frozen; valid during the call
    // data unfrozen after the call returns
}
```

If a function returns a borrowed pointer, it must return a pointer whose lifetime is no shorter than the caller's scope (usually static or an externally passed borrow):

```nihao
const get_const_ref() void {
    static data i32 = 100
    return &data   // safe: static lifetime
}
```

### 13.3 Lifetime Error Diagnostics

The compiler provides detailed diagnostics to help locate lifetime mismatches:

```nihao
func invalid() {
    flow local u32 = 42
    static bad_ptr = &local   // compile error: flow cannot be assigned to static
    // error message: target static outlives source flow; possible dangling pointer
}
```

Runtime visibility checks (in debug mode) can be queried via `visof`:

```nihao
func debug_vis(ptr void) {
    while visof(ptr) {
        is _flow => puts("dynamic pointer")
        is _static => puts("static pointer")
        is _var => puts("mutable borrow")
        is _const => puts("read-only borrow")
        break
    }
}
```

---

## 14. Safe Operation Patterns and Complete Examples

### 14.1 Safe Dereference and Access

Use the `?.` operator for safe dereference; the compiler combines visibility checks with bounds checking:

```nihao
func safe_access(flow ptr void) {
    value = ptr?.(i32)   // ensures ptr is non-null and visibility is correct
}

// equivalent to
if visof(ptr) == _flow {
    safePtr = ptr
}else{
    // compile-time error
}
```

Struct and array access follow the same rules:

```nihao
Person struct { name char[], age i32 }
flow person_ptr void = &some_person
flow name_ptr void = person_ptr.(Person).name   // field transfer must satisfy visibility
```

### 14.2 Complete Example (Ownership, Borrowing, and Storage Duration)

The example below demonstrates all the rules together:

```nihao
module main
use stdio
use stdlib

// ---------- helper functions ----------
func consume(flow val void) {
    puts("Consuming pointer")
    // val is auto-freed
}

func modify(var val void) {
    val.(i32) = 200
    puts("Modified to 200")
}

func inspect(const val void) {
    puts("Inspecting value: ")
    puts(val.(i32))
}

flow create_ptr() void {
    flow ptr void = malloc(i32)
    ptr.(i32) = 100
    return ptr          // ownership transfer
}

// ---------- main ----------
func main() {
    // value types (non-pointer) are copied; no ownership concept
    var a i32 = 10
    var b i32 = a
    b = 20                // a is still 10

    // pointer types
    flow ptr void = malloc(i32)
    ptr.(i32) = 50

    // flow -> var (mutable borrow)
    var mut_ref void = ptr     // ptr frozen
    mut_ref.(i32) = 100
    // ptr.(i32) = 200     // error: ptr frozen

    // flow -> const (read-only borrow)
    const read_ref void = ptr  // ptr frozen (shared)
    // read_ref.(i32) = 300 // error: read-only
    // ptr.(i32) = 400     // error: ptr still frozen

    // flow -> flow (ownership transfer)
    flow new_owner void = ptr  // ptr,read_ref,mut_ref invalidated
    // ptr.(i32) = 500     // error: ptr invalidated

    // function-call transfer
    flow p void = malloc(i32)
    p.(i32) = 1000
    consume(p)            // p invalidated

    flow q void = malloc(i32)
    q.(i32) = 1100
    modify(q)             // q frozen; mutated inside
    puts(q.(i32))         // prints 200
    inspect(q)            // q frozen (read-only)

    // chained calls (nested borrows)
    flow r void = malloc(i32)
    r.(i32) = 50
    inspect(r)            // r frozen
    // modify(r)           // error: r still borrowed
    modify(r)             // mutable first
    inspect(r)            // read-only after (allowed)

    // return values
    flow p2 void = create_ptr()  // receives ownership
    puts(p2.(i32))          // 100
    // p2 auto-freed

    // nested scopes
    flow m void = malloc(i32)
    m.(i32) = 500
    {
        var n void = m                // m frozen
        n.(i32) = 600
        {
            const o void = n          // n frozen
            // o.(i32) = 700     // error
            // n.(i32) = 800     // error
        }                        // o ends; n unfrozen
        n.(i32) = 900
    }                            // n ends; m unfrozen
    puts(m.(i32))                // 900
}
```

#### Compile-time Static Analysis Process

The compiler performs multi-phase static checks on the example to ensure every pointer transfer satisfies the storage-duration compatibility matrix and the ownership/borrowing rules. The analysis flow:

1. **Attribute inference**: annotate every variable and parameter with a storage-duration attribute (`const`/`static`/`flow`/`var`) and an ownership state (owning / borrowing).
2. **Visibility compatibility check**: per the §11.2 matrix, verify the target's lifetime is not shorter than the source's.
3. **Ownership/borrowing rule check**: per the §12.1 transfer rules, verify the source's state change (frozen/invalidated/stays-valid) is legal.
4. **Scope lifetime inference**: compute each borrowed pointer's active interval, ensuring it does not outlive the source's scope.

The following table excerpts the key pointer operations and shows the compiler's analysis:

| Statement (or comment)                                   | Operation type              | Rule applied                                                    | Analysis result & state change                                                                          |
|:-------------------------------------------------------- |:--------------------------- |:--------------------------------------------------------------- |:------------------------------------------------------------------------------------------------------- |
| `flow ptr void = malloc(i32)`                            | dynamic allocation          | —                                                               | `ptr` is `_flow`, **owns memory**, valid.                                                               |
| `var mut_ref void = ptr`                                 | assignment (`_flow` → `_var`)| compatible; `_var` shorter lifetime (allowed); `flow→var` = mutable borrow | **`ptr` frozen** (no read/write); `mut_ref` is a mutable borrow (`_var`), valid in scope.                |
| `const read_ref void = ptr`                              | assignment (`_flow` → `_const`)| compatible; transfer `flow→const` = read-only borrow            | **`ptr` stays frozen** (multiple borrows coexist); `read_ref` read-only borrow.                         |
| `flow new_owner void = ptr`                              | assignment (`_flow` → `_flow`)| compatible; `flow→flow` = ownership transfer                    | **check fails!** `ptr` is frozen by `mut_ref` and `read_ref`; ownership may not be transferred. Compile error. |
| `consume(p)`                                             | call (arg `p` → param `flow`)| param `_flow` receives ownership; `flow→flow` = transfer        | **`p` invalidated**; ownership moves to the parameter, freed when the function returns.                  |
| `modify(q)`                                              | call (arg `q` → param `var`) | param `_var` = mutable borrow; `flow→var` = mutable borrow      | **`q` frozen**; data may be mutated inside, but not transferred or freed. `q` unfrozen after the call.   |
| `inspect(q)`                                             | call (arg `q` → param `const`)| param `_const` = read-only borrow; `flow→const` = read-only     | **`q` frozen** (during that statement only); read-only inside; unfrozen after.                           |
| `inspect(r)` <br>`// modify(r)` (comment)<br>`modify(r)` | consecutive calls           | first `inspect` creates a statement-level read-only borrow; ends when the statement ends | the second `modify` occurs after the borrow ended; `r` can be mutably borrowed again — analysis passes. Calling `modify` within the borrow (e.g. same expression) would be a compile error. |
| `flow p2 void = create_ptr()`                            | function return (`_flow`)   | return type `_flow`; ownership transfers to `p2`                | `p2` owns memory, valid. The local pointer inside `create_ptr` is invalidated (ownership moved out).     |
| `var n = m`                                              | assignment (`_flow` → `_var`)| same as `ptr → mut_ref`: mutable borrow                          | **`m` frozen** (until `n`'s scope ends); `n` is a mutable borrow.                                        |
| `const o = n`                                            | assignment (`_var` → `_const`)| source `n` is `_var` (mutable borrow); `var→const` = read-only borrow | **`n` frozen** (until `o`'s scope ends); `o` is a read-only borrow.                                     |
| `// o.(i32) = 700` (comment)<br>`// n.(i32) = 800` (comment)| attempted writes           | `o` read-only; `n` frozen because of `o`; neither mutable       | compiler errors at the commented positions if uncommented.                                                |
| `n.(i32) = 900`                                          | write                       | `o`'s scope ended; `n` unfrozen; `n` is a mutable borrow and may write | analysis passes; `n` may modify the pointed-to data (owned by `m`).                                     |
| `puts(m.(i32))`                                          | read                        | `n`'s scope ended; `m` unfrozen; `m` owns memory, readable      | analysis passes; reads value 900.                                                                        |

---

#### Final Lifetime and State Results

After compile-time static analysis, each pointer variable's final lifetime attribution and state transitions are shown below. "Destroyed/freed at" is determined by the storage-duration attribute and any ownership transfers.

| Variable     | Storage attr | Initial state        | State transition sequence                                                                                          | Final destroy/free point                          |
|:------------ |:------------ |:-------------------- |:------------------------------------------------------------------------------------------------------------------ |:------------------------------------------------- |
| `ptr`        | `_flow`      | owns, valid          | valid → frozen (borrowed by `mut_ref` and `read_ref`) → stays frozen to scope end                                  | freed before `main` ends (if not transferred)     |
| `mut_ref`    | `_var` (borrow)| —                  | created as mutable borrow → destroyed at scope end                                                                 | destroyed before `main` ends (borrow only; no free) |
| `read_ref`   | `_const` (borrow)| —                | created as read-only borrow → destroyed at scope end                                                               | destroyed before `main` ends (borrow only)        |
| `new_owner`  | `_flow`      | —                    | rejected at compile time because `ptr` is frozen; variable is not generated                                        | not generated                                     |
| `p`          | `_flow`      | owns, valid          | valid → **invalidated** after `consume(p)` (ownership transfer)                                                    | freed by the parameter when `consume` returns     |
| `q`          | `_flow`      | owns, valid          | valid → frozen during `modify(q)` → unfrozen → frozen during `inspect(q)` → unfrozen → valid to function end       | freed before `main` ends                          |
| `r`          | `_flow`      | owns, valid          | valid → statement-frozen during `inspect(r)` → unfrozen → frozen during `modify(r)` → unfrozen → frozen during `inspect(r)` → unfrozen → valid to end | freed before `main` ends |
| `p2`         | `_flow`      | owns, valid (inherited from `create_ptr`) | stays valid to function end                                                              | freed before `main` ends                          |
| `m`          | `_flow`      | owns, valid          | valid → frozen (borrowed by `n` in nested block) → unfrozen on leaving `n`'s block → valid to function end          | freed before `main` ends                          |
| `n`          | `_var` (borrow)| —                  | created as mutable borrow of `m` → frozen (borrowed by `o` in inner block) → unfrozen on leaving `o`'s block → destroyed on leaving its own block | destroyed when its inner block ends; no memory freed |
| `o`          | `_const` (borrow)| —                | created as read-only borrow of `n` → destroyed on leaving its block                                                | destroyed at the innermost block's end; no memory freed |

#### Nested-Scope Borrow-Chain Analysis (for `m`, `n`, `o`)

| Scope level                  | Active variables           | Borrow relation       | Owning source | State description                                      |
|:---------------------------- |:-------------------------- |:--------------------- |:------------- |:------------------------------------------------------ |
| **outer `main` function scope** | `m` (`_flow`)            | none                  | `m` itself    | `m` valid, owns the memory block.                      |
| **middle block `{ ... }`**   | `m` (`_flow`), `n` (`_var`)| `n` borrows `m`       | `m`           | `m` frozen; `n` may mutate the data.                   |
| **innermost block `{ ... }`**| `m`, `n`, `o` (`_const`)  | `o` borrows `n`; `n` borrows `m` | `m`   | `m` frozen, `n` frozen, `o` read-only. All writes forbidden. |
| **leaving innermost block**  | `m`, `n`                  | `o` destroyed; `n` unfrozen | `m`        | `o`'s borrow ends; `n` regains mutable-borrow ability. |
| **leaving middle block**     | `m`                       | `n` destroyed; `m` unfrozen | `m`        | all borrows end; `m` regains full ownership; readable and writable. |

This nested analysis guarantees that at every level, a borrow never outlives the source pointer's scope, fully satisfying all static constraints of chapters 11 and 12. Through such analysis the compiler eliminates dangling pointers, double frees, and data races at compile time.

### 14.3 Common Safe Patterns

- **Encapsulated dynamic data**: create `flow` data inside a block, process it, and let it auto-release.
- **Shared static cache**: hold data long-term with `static` pointers; expose read-only access through `const` borrows.
- **Ownership transfer chains**: move ownership between functions via `flow` returns, avoiding copies.

```nihao
func safe_patterns() {
    // pattern 1: temporary dynamic data
    {
        flow temp void = load_data()
        process(temp)
    }  // temp auto-released

    // pattern 2: static cache
    static cache void = initialize_cache()
    const cache_ref void = &cache   // read-only borrow
    use_cache(cache_ref)

    // pattern 3: ownership transfer
    flow data void = acquire()
    flow processed = transform(data)  // data invalidated
    analyze(processed)                // processed continues to transfer
}
```

---

## 15. Summary

NiHao manages three things through the unified attribute system (`const`, `static`, `flow`, `var`):

- **Storage duration** (lifetime): the compatibility matrix prevents dangling pointers.
- **Ownership and borrowing**: transfer rules control pointer state changes (frozen, invalidated, stays-valid).
- **Scope inference**: the compiler automatically analyzes borrow validity to ensure safety.

This design builds on NiHao's existing visibility system to achieve memory safety:

- Dangling-pointer prevention at compile time,
- Double-free prevention through ownership transfer semantics,
- Data-race prevention through borrow discipline (freeze/invalidate).

All of it is enforced statically — no runtime garbage collector, no runtime cost.

---

## Appendix: Keyword and Operator Reference

| Category      | Tokens |
| ------------- | ------ |
| Types         | `void` `char` `string` `bool` `i8` `i16` `i32` `i64` `u8` `u16` `u32` `u64` `f32` `f64` `fx32` `fx64` `short` `int` `long` `float` `double` |
| Storage/visibility | `const` `flow` `static` `var` `_undef` `_const` `_flow` `_static` `_var` |
| Functions     | `func` `is` `return` `break` `continue` `goto` |
| Aggregates    | `struct` `union` `enum` `alias` |
| Modules       | `module` `use` `link` `linkas` `as` |
| Control       | `if` `else` `switch` `case` `default` `for` `do` `while` |
| Compile-time  | `cooking` `align` `static_assert` |
| Introspection | `sizeof` `typeof` `alignof` `offsetof` `bitoffsetof` `holdof` `structof` `unionof` `visof` `malloc` |
| Literals      | `true` `false` |
| Operators     | `+ - * / % ++ -- == != < > <= >= && \|\| ! & \| ^ ~ << >> = += -= *= /= %= &= \|= ^= <<= >>= -> . .( ?. ?( ?= ? : :: , .. # ; ( ) [ ] { }` |

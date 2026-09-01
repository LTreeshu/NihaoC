# NihaoC Language Keywords and Syntax Elements

###### Reserved Keywords

- `const` – constant/function definition keyword
- `flow` – dynamically allocated variable declaration keyword
- `static` – static variable declaration keyword
- `var` – local variable declaration keyword
- `_const` – constant visibility attribute enumeration value
- `_flow` – dynamic visibility attribute enumeration value
- `_static` – static visibility attribute enumeration value
- `_undef` – undefined invisible attribute enumeration value
- `_var` – local visibility attribute enumeration value
- `cooking { ... }` – compile‑time execution block keyword
- `align n { ... }` – byte‑alignment block
- `module` – module definition
- `use` – module import
- `func` no return value function return value no attribute function definition
- `alias` – type alias
- `link ...` – static library export usage
- `linkas "..."` – static library export naming
- `as` – alias binding keyword
- `cooking` – compile-time execution (`cooking { ... }` block)
- `align` – byte alignment (`align n { ... }` block)
- `continue` – continue next loop iteration
- `default` – default branch option
- `goto` – jump keyword
- `true` / `false` – boolean literals
- `bitoffsetof` – bit-field member offset
- `holdof` – variable owner query
- `string` – string type alias (same as `char[]`)
- `register` / `restrict` / `volatile` – C-style qualifiers
- `struct` – structure definition
- `union` – union definition
- `enum` – enumeration definition
- `typeof` – type introspection
- `sizeof` – get type size
- `alignof` – get type alignment
- `offsetof` – get struct member offset
- `structof` – get base address of the struct containing a member
- `unionof` – get base address of the union containing a member
- `if` – conditional branch
- `else` – else branch
- `for` – loop control
- `while` – loop control
- `is` – pattern matching control
- `do` – loop control
- `switch` – multi‑way branch
- `case` – branch option
- `break` – exit branch
- `return` – function return
- `visof` – visibility check
- `malloc` – dynamic memory allocation (builtin function, not a keyword)

###### Type Keywords

- `void` – pointer type
- `bool` – boolean type
- `i8` – 8‑bit signed integer type
- `i16` – 16‑bit signed integer type
- `i32` – 32‑bit signed integer type
- `u8` – 8‑bit unsigned integer type
- `u16` – 16‑bit unsigned integer type
- `u32` – 32‑bit unsigned integer type
- `u64` – 64‑bit unsigned integer type
- `i64` – 64‑bit signed integer type
- `f32` – 32‑bit floating‑point type
- `f64` – 64‑bit floating‑point type
- `fx32` – 32‑bit fixed‑point type
- `fx64` – 64‑bit fixed‑point type
- `char` – character type
- `char[]` – string type
- `short` – short integer (C-compat)
- `int` – integer (C-compat)
- - `long` – long integer (C-compat)
- - `float` – single-precision float (C-compat)
- - `double` – double-precision float (C-compat)

###### Reserved Operators

- `&` – address‑of operator
- `=` – assignment operator
- `?=` – safe assignment operator (with pointer checking)
- `.` – struct/union member access
- `.()` – void pointer dereference
- `.(type)` – typed dereference (with built‑in out‑of‑bounds check)
- `->` – struct/union pointer member access
- `{}` – block / initializer list / multiple return values
- `()` – function call, type cast
- `[]` – array subscript, multi‑level pointer
- `[start..end]` – array/slice range operator
- `?:` – conditional (ternary) operator
- `,` – comma operator

###### Arithmetic Operators

- `+` – addition
- `-` – subtraction, unary negation
- `*` – multiplication
- `/` – division
- `%` – modulo (remainder)
- `++` – increment (prefix or postfix)
- `--` – decrement (prefix or postfix)

###### Relational (Comparison) Operators

- `==` – equal to
- `!=` – not equal to
- `<` – less than
- `>` – greater than
- `<=` – less than or equal to
- `>=` – greater than or equal to

###### Logical Operators

- `&&` – logical AND
- `||` – logical OR
- `!` – logical NOT (unary)

###### Bitwise Operators

- `&` – bitwise AND
- `|` – bitwise OR
- `^` – bitwise XOR
- `~` – bitwise NOT (unary)
- `<<` – left shift
- `>>` – right shift

###### Assignment Operators

- `=` – simple assignment
- `+=` – add and assign
- `-=` – subtract and assign
- `*=` – multiply and assign
- `/=` – divide and assign
- `%=` – modulo and assign
- `&=` – bitwise AND and assign
- `|=` – bitwise OR and assign
- `^=` – bitwise XOR and assign
- `<<=` – left shift and assign
- `>>=` – right shift and assign

###### Other Operators

-
// NihaoC 语言语法测试文件 - comprehensive_test.nh
// 此文件包含NihaoC语言的所有主要语法用例

// ==================== 1. 模块声明和导入 ====================
module main
use stdio
use stdlib
use math

// 外部库链接
link "libhttp.so" http


// ==================== 2. 类型别名 ====================
alias http_client = http.http_client
alias time = stdlib.time
alias string = char*
alias byte = u8

// ==================== 3. 常量定义 ====================
const MAX_BUFFER_SIZE i32 = 4096
const PI f64 = 3.141592653589793
const APP_NAME string = "NihaoC Test App"
const ENABLE_DEBUG bool = true

// ==================== 4. 结构体定义 ====================
// 基本结构体
Person struct {
    name char[32]
    age u8
    height f32
    is_student bool
    halfword1 u16:8
    halfword2 u16:8
}

// 嵌套结构体
Address struct {
    street char[64]
    city char[32]
    zip_code u32
}

Employee struct {
    person Person
    address Address
    salary f64
    department char[32]
}

// 多返回值结构体
Multireturn struct {
    success bool
    value i32
    message char[128]
}

// 对齐结构体
align 8 {
    PackedData struct {
        id u64
        timestamp u64
        data f64[4]
    }
}

// ==================== 5. 变量声明 ====================
// 静态变量
static global_counter u32 = 0

// 动态变量
flow dynamic_buffer void = malloc(u8, MAX_BUFFER_SIZE)
flow temp_data void = malloc(f32, 100)

// 局部变量
{
    local local_var i32 = 42
    local local_str string = "局部字符串"
}

// ==================== 6. 函数定义 ====================
// 基本函数
const add_numbers(a i32, b i32) i32 {
    return a + b
}

// 多返回值函数
const process_data(input i32) multireturn {
    if input < 0 {
        return {false, 0, "输入值不能为负数"}
    }
    
    flow result i32 = input * 2
    return {true, result, "处理成功"}
}

// 递归函数
const factorial(n u32) u64 {
    if n <= 1 {
        return 1
    }
    return n * factorial(n - 1)
}

// 内联函数
inline square(x f32) f32 {
    return x * x
}

// 可见性检查函数
const safe_pointer_operation(ptr void) bool {
    if visof(ptr) == _flow {
        puts("指针是动态分配的")
        return true
    }
    else if visof(ptr) == _static {
        puts("指针是静态的")
        return true
    }
    else if visof(ptr) == _undef {
        puts("指针未定义")
        return false
    }
    return false
}

// ==================== 7. 指针操作 ====================
const pointer_demo() {
    // 基本指针
    flow ptr_int void = malloc(i32)
    ptr_int.(i32) = 100
    
    // 数组指针
    arry f32[5] = {1.1, 2.2, 3.3, 4.4, 5.5}
    ptr_arry void[5] = &arry
    
    // 多级指针
    flow ptr_ptr void[] = malloc(void)
    ptr_ptr.() = &ptr_int
    flow ptr_ptr_save void[] = ptr_ptr.?() 
    
    // 结构体指针
    xiaoming Person
    stptr void = &xiaoming
    stptr.(char[32])[0..31] = "小明"
    stptr->age = 20
    stptr->height = 175.5
    stptr->is_student = true
    
    // 指针切片
    flow slice_ptr void = &arry[1]
    slice_ptr.(f32[3]) = {2.2, 3.3, 4.4}
}

// ==================== 8. 数组操作 ====================
const array_demo() {
    // 一维数组
    numbers i32[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9}
    
    // 二维数组
    matrix f32[3][3] = {
        {1.0, 0.0, 0.0},
        {0.0, 1.0, 0.0},
        {0.0, 0.0, 1.0}
    }
    
    // 动态数组
    flow dyn_array void = malloc(f64, 20)
    
    // 数组遍历
    for i u32 = 0; i < 10; i++ {
        numbers[i] = i * i
    }
    
    // 数组作为参数
    sum = calculate_sum(numbers, 10)
}

const calculate_sum(arr i32[], size u32) i32 {
    total i32 = 0
    for i u32 = 0; i < size; i++ {
        total += arr[i]
    }
    return total
}

// ==================== 9. 控制流语句 ====================
const control_flow_demo() {
    // if-else if-else
    score f32 = 85.5
    
    if score >= 90.0 {
        puts("优秀")
    }
    else if score >= 80.0 {
        puts("良好")
    }
    else if score >= 60.0 {
        puts("及格")
    }
    else {
        puts("不及格")
    }
    
    // switch-case
    option u8 = 2
    switch option {
        case 1:
            puts("选项1")
            break
        case 2:
            puts("选项2")
            break
        case 3:
            puts("选项3")
            break
        default:
            puts("未知选项")
    }
    
    // for循环
    for i u32 = 0; i < 5; i++ {
        puts("循环次数: ")
        print(i)
    }
    
    // while循环
    counter u32 = 0
    while counter < 3 {
        puts("while循环")
        counter++
    }
    
    // do-while循环
    do {
        puts("do-while循环")
        counter--
    } while (counter > 0)
}

// ==================== 10. 编译期执行 ====================
cooking {
    // 编译期计算
    const COMPILE_TIME_VALUE i32 = 10 * 20 + 5
    
    // 编译期断言
    static_assert(sizeof(i32) == 4, "i32必须是4字节")
    static_assert(COMPILE_TIME_VALUE == 205, "编译期计算错误")
}

// ==================== 11. 错误处理 ====================
const error_handling_demo() {
    // 空指针检查
    flow ptr void = malloc(i32, 1)
    if ptr == null {
        puts("内存分配失败")
        return
    }
    
    // 数组边界检查
    arr i32[5] = {1, 2, 3, 4, 5}
    index i32 = 10
    
    if index >= 0 && index < 5 {
        value = arr[index]
    } else {
        puts("数组索引越界")
    }
    
    // 除零检查
    divisor f32 = 0.0
    if divisor != 0.0 {
        result = 100.0 / divisor
    } else {
        puts("除数不能为零")
    }
}

// ==================== 12. 类型操作 ====================
const type_operations() {
    value i32 = 42
    
    // 类型检查
    if typeof(value) == i32 {
        puts("value是i32类型")
    }
    
    // 类型转换
    float_value f32 = (&value).(f32)
    
    // 大小和对齐
    size u8 = sizeof(Person)
    align u8 = alignof(Person)
    
    // 偏移量计算
    offset u8 = offsetof(Person, age)
}

// ==================== 13. 内存管理 ====================
const memory_management() {
    // 分配内存
    flow mem_block void = malloc(u8, 1024)
    
    // 重新分配
    mem_block = realloc(mem_block, 2048)
    
    // 复制内存
    source i32[5] = {1, 2, 3, 4, 5}
    flow dest void = malloc(i32, 5)
    memcpy(dest, &source, sizeof(i32) * 5)
    
    // 设置内存
    flow zero_mem void = malloc(u8, 100)
    memset(zero_mem, 0, 100)
    
    // 内存比较
    if memcmp(dest, &source, sizeof(i32) * 5) == 0 {
        puts("内存内容相同")
    }
    
    // 自动释放（flow变量在函数结束时自动释放）
}

// ==================== 14. 字符串操作 ====================
const string_operations() {
    // 字符串字面量
    greeting string = "你好，NihaoC！"
    
    // 字符串长度
    length u32 = strlen(greeting)
    
    // 字符串复制
    flow copy_str void = malloc(char, length + 1)
    strcpy(copy_str, greeting)
    
    // 字符串连接
    part1 string = "Hello, "
    part2 string = "World!"
    flow combined void = malloc(char, strlen(part1) + strlen(part2) + 1)
    strcpy(combined, part1)
    strcat(combined, part2)
    
    // 字符串比较
    if strcmp("apple", "apple") == 0 {
        puts("字符串相等")
    }
    
    // 字符串查找
    position = strstr(greeting, "NihaoC")
    if position != null {
        puts("找到子字符串")
    }
}

// ==================== 15. 数学运算 ====================
const math_operations() {
    // 基本运算
    a f64 = 10.5
    b f64 = 3.2
    c f32 = 0x45
    
    sum = a + b
    difference = a - b
    product = a * b
    quotient = a / b
    remainder = a % b

    c = c >> 2
    c = c << 2
    
    // 数学函数
    sine = sin(PI / 2)
    cosine = cos(0)
    tangent = tan(PI / 4)
    
    square_root = sqrt(16.0)
    power = pow(2.0, 3.0)
    logarithm = log(10.0)
    
    absolute = abs(-5.3)
    floor_val = floor(3.7)
    ceil_val = ceil(3.2)
    round_val = round(3.5)
}

// ==================== 16. 输入输出 ====================
const io_operations() {
    // 输出
    puts("这是一条消息")
    print("数字: %d, 浮点数: %f, 字符串: %s", 42, 3.14, "测试")
    
    // 格式化输出
    flow buffer void = malloc(char, 100)
    sprintf(buffer, "值: %d, 时间: %u", 100, time())
    puts(buffer)
    
    // 文件操作（示例）
    flow file void = fopen("test.txt", "w")
    if file != null {
        fputs("文件内容", file)
        fclose(file)
    }
}

// ==================== 17. 主函数 ====================
const main() {
    puts("NihaoC 语法测试程序启动")
    puts("==========================")
    
    // 执行各个测试函数
    pointer_demo()
    array_demo()
    control_flow_demo()
    error_handling_demo()
    type_operations()
    memory_management()
    string_operations()
    math_operations()
    io_operations()
    
    // 多返回值测试
    result multireturn = process_data(50)
    if result.success {
        puts("处理成功，结果值: ")
        print(result.value)
        puts("消息: ")
        puts(result.message)
    }
    
    // 递归测试
    fact = factorial(5)
    puts("5的阶乘是: ")
    print(fact)
    
    // 可见性检查
    flow test_ptr void = malloc(i32)
    safe_pointer_operation(test_ptr)
    
    puts("==========================")
    puts("NihaoC 语法测试程序结束")
    
    return
    // flow变量会自动释放
}
# mio 编程语言
mio 是一种编译型编程语言，采用自举方式开发，首个编译版本使用 C 语言实现。编译器将 `.mio` 源文件翻译为 C 代码，再由 C 编译器生成可执行文件。
## 获取
### 下载预编译二进制文件
访问 [Releases](https://github.com/mioLanguage/mio/releases) 页面下载对应平台的二进制文件。
>[!NOTE]
>如果遇到依赖库缺失问题，建议从源代码编译。
### 从源代码编译
**环境要求：** C11 兼容编译器（GCC / Clang / MSVC）
```bash
gcc -std=c11 -o mioc src/main.c
```
## 使用方法
```bash
# 查看版本与许可证
mioc -v
mioc --version
# 编译 mio 源文件（自动生成同名的 .c 文件）
mioc hello.mio
# 指定输出的 C 文件
mioc hello.mio -o output.c
# 编译生成的 C 代码
gcc output.c -o hello
./hello
```
# 鸣谢
[HZY](https://hzy.us.kg)、blvmwd、lindan5563、dFszheng、CHE_CHI
# Wiki
## 注释
`#` 后面的内容为注释，直到行尾：

```mio
# 这是一行注释
var x = 10;  # 行末注释
```
## 导入

**关键字：** `import`

**使用方法：** 导入 C 头文件或其他 mio 文件，必须写在文件顶层。
```mio
import stdio.h;              # 导入 C 标准库
import "mylib.mio";          # 导入 mio 文件（用引号包裹）
import stdio.h, "lib.mio";   # 同时导入多个
```
## 变量

**关键字：** `var`（可变）、`const`（常量）

**使用方法：**
```mio
var 变量名: 类型 = 初始值;      # 变量
const 常量名: 类型 = 初始值;    # 常量（不可修改）
```
### 基本类型
| 类型 | 说明 |
|------|------|
| `i32` / `i64` | 有符号整数 |
| `u32` / `u64` | 无符号整数 |
| `f32` / `f64` | 浮点数 |
| `bool` | 布尔值（`true` / `false`）|
| `char` | 单个字符 |
| `T[N]` | 长度为 N 的数组 |
### 示例
```mio
var x: i32 = 42;
const PI: f64 = 3.14;

# 同时声明多个
var a: i64 = 1, b: i32 = 2;

# 数组
var arr: i32[3] = {1, 2, 3};
```
### 自动补全类型
有初始值时可以省略类型，编译器自动推导：
```mio
var x = 42;          # i32
var pi = 3.14;       # f64
var flag = true;     # bool
var nums = {1, 2, 3};  # i32[3]
```
## 控制流（if）
**关键字：** `if`、`elif`、`else`
### 基本用法
条件后面必须跟冒号 `:`
```mio
if: 条件 {
    代码块
} elif: 条件 {
    代码块
} else {
    代码块
}
```
### 示例
```mio
if: score >= 90 {
    printf("A\n");
} elif: score >= 80 {
    printf("B\n");
} else {
    printf("C\n");
}
```
### 单语句简写
单语句可以省略大括号：
```mio
if: x > 0 printf("正数\n");
```
## 循环
### while 循环

**关键字：** `while`

条件后面必须跟冒号 `:`
```mio
var i: i32 = 0;
while: i < 5 {
    printf("%d\n", i);
    i = i + 1;
}
```
### for 循环
**关键字：** `for`

**格式：** `for: 初始化; 条件; 更新 { 代码块 }`

冒号 `:` 必须跟在 `for` 后面
```mio
var sum: i32 = 0;
for: i = 0; i < 10; i = i + 1 {
    sum = sum + i;
}
```
### 省略部分
```mio
var i: i32 = 0;
for: ; i < 5; i = i + 1 {   # 省略初始化
    printf("%d\n", i);
}
```
### break 与 continue
```mio
while: true {
    if: x > 100 {
        break;       # 跳出循环
    }
    if: x % 2 == 0 {
        x = x + 1;
        continue;    # 跳过本次迭代剩余部分
    }
    x = x + 1;
}
```
## 跳转（goto）
**关键字：** `goto`

**定义标签：** `:标签名`
```mio
var i: i32 = 0;
:loop
    printf("%d\n", i);
    i = i + 1;
    if: i < 5 {
        goto loop;
    }
```
## 函数
**关键字：** `def`、`static`
### 定义格式
```mio
def 返回类型 函数名(参数名: 参数类型, ...) {
    函数体
}
```
### 示例

```mio
# 显式返回
def i32 add(a: i32, b: i32) {
    return a + b;
}

# 隐式返回（最后一行不加分号）
def i32 add(a: i32, b: i32) {
    a + b
}

# 无返回值
def void say_hello() {
    printf("hello\n");
}

# 静态函数（仅当前文件可见）
static def void helper() {
    printf("helper\n");
}
```
### 函数调用
```mio
var result = add(10, 20);
say_hello();
```
## 结构体
**关键字：** `struct`
### 定义
```mio
struct 结构体名 {
    字段名: 类型;
    字段名: 类型;

    # 构造函数（成员初始化列表）
    def 结构体名(参数列表): 字段名(参数), ... {}

    # 方法（this 为指针）
    def 返回类型 方法名(this: 结构体名, 其他参数) {
        函数体
    }

    # 静态方法
    static def 返回类型 方法名(参数) {
        函数体
    }

    # 运算符重载
    def 结构体名 operator+(this: 结构体名, other: 结构体名) {
        函数体
    }
}
```
### 完整示例
```mio
struct Point {
    x: f64;
    y: f64;

    # 构造函数
    def Point(xx: f64, yy: f64): x(xx), y(yy) {}

    # 方法
    def f64 distance(this: Point) {
        return this.x * this.x + this.y * this.y;
    }

    # 运算符重载
    def Point operator+(this: Point, other: Point) {
        return Point(this.x + other.x, this.y + other.y);
    }

    # 静态方法
    static def void info() {
        printf("Point struct\n");
    }
}
```
### 使用
```mio
var p = Point(3.0, 4.0);    # 构造
var d = p.distance();        # 方法调用
var q = p + p;               # 运算符重载
Point.info();                # 静态方法
```
> [!NOTE]
> 方法中的 `this` 是指针，成员访问会自动使用 `->`，无需手动区分。
## 枚举
**关键字：** `enum`
### 定义
```mio
enum 枚举名 {
    变体名,
    变体名 = 初始值,
    ...
}
```
### 示例
```mio
enum Color {
    Red,
    Green,
    Blue
}

enum Status {
    Ok = 0,
    Error = -1
}
```
### 使用
```mio
var c = Color.Red;
var s = Status.Ok;
```
## 联合体
**关键字：** `union`
### 定义
```mio
union 联合体名 {
    字段名: 类型;
    字段名: 类型;
    ...
}
```
### 示例
```mio
union Value {
    int_val: i32;
    float_val: f64;
    bool_val: bool;
}
```
### 使用
```mio
var v: Value;
v.int_val = 42;
printf("%d\n", v.int_val);

v.float_val = 3.14;
printf("%f\n", v.float_val);
```
> [!NOTE]
> 联合体所有字段共享同一块内存，一次只能使用其中一个字段。
## 作用域规则
### 顶层作用域（文件级别）
以下内容只能出现在文件顶层，不能在函数内部定义：
- `import`
- `struct`
- `enum`
- `union`
- `def`
### 块作用域（函数内部）
用 `{}` 包围的代码块可以嵌套，内部定义的变量在块结束后销毁：
```mio
def void test() {
    var x = 10;
    {
        var y = 20;   # y 只在此块内有效
        var z = 30;
    }
    # y 和 z 在这里不可见
}
```
# mio 编程语言
mio 是一种编译型编程语言，是 mio 解释器的进化版。首个编译版本使用 C 语言实现，第二版使用 C++ 语言实现。编译器将 `.mio` 源文件翻译为 LLVM IR，再直接生成原生可执行文件。
## 获取
### 下载预编译二进制文件
访问 [Releases](https://github.com/mioLanguage/mio/releases) 页面下载对应平台的二进制文件。
>[!NOTE]
>如果遇到依赖库缺失问题，建议从源代码编译。

### 从源代码编译
**环境要求：** c++17 兼容编译器（GCC / Clang / MSVC），LLVM 22.1.x
```bash
g++ -std=c++17 -o mioc src/main.cpp -lLLVM
```
## 使用方法
```bash
# 查看版本与许可证
mioc -v
mioc --version
# 编译 mio 源文件（自动生成同名可执行文件）
mioc hello.mio
# 指定输出文件
mioc hello.mio -o hello.exe
# 仅生成汇编代码
mioc hello.mio -S
# 仅编译为目标文件（不链接）
mioc hello.mio -c
# 添加头文件搜索路径
mioc hello.mio -I ./include
# 定义宏
mioc hello.mio -D DEBUG
# 优化级别（-O0 / -O1 / -O2 / -O3）
mioc hello.mio -O2
# 发布模式（自动 -O2 + 缓存）
mioc hello.mio --release
# 静态链接
mioc hello.mio -static
```
# 鸣谢
- [HZY1618yzh](https://hzy1618yzh.github.io)
- [lindan5563](https://lindan5563.github.io/)
- [blvmwd](https://blvmwd.us.kg/)
- ZHANG_CHI
- [dFszheng](https://github.com/dFszheng)

排名按照加入 mio 团队的顺序排列。
# Wiki
## 注释
`#` 后面的内容为注释，直到行尾：

```mio
# 这是一行注释
var x = 10;  # 行末注释
```
## 导入

**关键字：** `import`

**使用方法：** 导入 mio 头文件，必须写在文件顶层。可以用 -I 命令指定头文件路径。
```bash
mioc hello.mio -I ./include
```
```mio
import stdio;              # 导入 C 标准库
import "mylib.mio";          # 导入 mio 文件（用引号包裹）
import stdio, "lib";   # 同时导入多个
```
## 宏与条件编译

**关键字：** `macro`，`@[if,elif,else,end]`

**使用方法：** 定义宏来替换代码和条件编译防止重复引入。可以用 -D 命令定义宏。
```bash
mioc hello.mio -D DEBUG
```
```mio
macro DEBUG;
macro RELEASE;
@if DEBUG
	printf("DEBUG\n");
@elif RELEASE
	printf("RELEASE\n");
@else
	printf("UNKNOWN\n");
@end
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
| `i8` / `i16` / `i32` / `i64` / `i128` | 有符号整数 |
| `u8` / `u16` / `u32` / `u64` / `u128` | 无符号整数 |
| `isize` / `usize` | 指针宽度整数（32/64 位自适应）|
| `f32` / `f64` | 浮点数 |
| `bool` | 布尔值（`true` / `false`）|
| `char` | 单个字符 |
| `T[N]` | 长度为 N 的数组 |
| `T*` | 指针类型 |

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
### 字符串与字符
```mio
var s: char* = "hello";    # 字符串字面量
var ch: char = 'A';        # 字符字面量
```
### 全局变量与命名空间访问
用 `::` 前缀访问全局变量（避免与局部变量同名冲突）：
```mio
var x: i32 = 10;
void foo() {
    var x: i32 = 20;
    printf("%d\n", ::x);   # 访问全局 x，输出 10
    printf("%d\n", x);     # 访问局部 x，输出 20
}
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
for: i = 0; i < 10; i += 1 {
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
## 运算符
### 算术运算符
| 运算符 | 说明 |
|--------|------|
| `+` `-` `*` `/` `%` | 加减乘除取模 |
| `-` | 取负（一元）|
| `~` | 按位取反 |

### 比较运算符
| 运算符 | 说明 |
|--------|------|
| `==` `!=` | 等于 / 不等于 |
| `<` `>` `<=` `>=` | 小于 / 大于 / 小于等于 / 大于等于 |

### 逻辑运算符
| 运算符 | 说明 |
|--------|------|
| `&&` `\|\|` `!` | 逻辑与 / 逻辑或 / 逻辑非 |

### 位运算符
| 运算符 | 说明 |
|--------|------|
| `&` `\|` `^` | 按位与 / 按位或 / 按位异或 |
| `<<` `>>` | 左移 / 右移 |

### 赋值运算符
| 运算符 | 说明 |
|--------|------|
| `=` | 赋值 |
| `+=` `-=` `*=` `/=` `%=` | 复合赋值 |
| `&=` `\|=` `^=` `<<=` `>>=` | 位复合赋值 |

### 运算符重载（结构体/类）
在结构体或类中定义 `operator+`、`operator-` 等方法实现运算符重载：
```mio
struct Point {
    x: f64;
    y: f64;
    Point operator+(other: Point) {
        return Point(this.x + other.x, this.y + other.y);
    }
}
```
## 类型转换
使用 C 风格的类型转换语法：
```mio
var x: f64 = 3.14;
var y: i32 = i32(x);        # 浮点数转整数（截断）
var z: i64 = i64(y);        # 整数扩展
var n: u32 = u32(-1);       # 有符号转无符号
```
## 函数
### 定义格式
```mio
返回类型 函数名(参数名: 参数类型, ...) {
    函数体
}
```
### 示例

```mio
# 显式返回
i32 add(a: i32, b: i32) {
    return a + b;
}

# 隐式返回（最后一行不加分号）
i32 add(a: i32, b: i32) {
    a + b
}

# 无返回值
void say_hello() {
    printf("hello\n");
}

# 静态函数（仅当前文件可见）
static void helper() {
    printf("helper\n");
}

# 仅声明函数（其他文件定义）
extern i32 printf(fmt: char*, ...);
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
    结构体名(参数列表): 字段名(参数), ... {}

    # 方法（this 为指针）
    返回类型 方法名(其他参数) {
        函数体
    }

    # 静态方法
    static 返回类型 方法名(参数) {
        函数体
    }

    # 运算符重载
    结构体名 operator+(other: 结构体名) {
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
    Point(xx: f64, yy: f64): x(xx), y(yy) {}

    # 方法
    f64 distance() {
        return this.x * this.x + this.y * this.y;
    }

    # 运算符重载
    Point operator+(other: Point) {
        return Point(this.x + other.x, this.y + other.y);
    }

    # 静态方法
    static void info() {
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

## 类
**关键字：** `class`

类与结构体类似，但支持继承、虚函数和访问控制。类默认使用引用语义（通过指针操作）。

### 定义
```mio
class 类名 {
    访问控制:
    字段名: 类型;
    方法定义...
}
```
### 访问控制
| 关键字 | 说明 |
|--------|------|
| `public:` | 公开成员，外部可访问 |
| `private:` | 私有成员，仅类内部可访问 |
| `protected:` | 受保护成员，类及其子类可访问 |

### 构造函数与析构函数
构造函数名与类名相同，析构函数以 `~` 开头：
```mio
class Animal {
public:
    name: char*;

    Animal(name: char*) {
        this.name = name;
    }
    ~Animal() {
        printf("Animal destroyed\n");
    }
}
```
### 继承
使用 `类名(父类:访问控制)` 语法继承父类：
```mio
class Dog(Animal:public) {
public:
    Dog(name: char*) {
        this.name = name;
    }
}
```
### 虚函数与重写
用 `virtual` 声明虚函数，子类用 `override` 重写：
```mio
class Animal {
public:
    virtual void speak() {
        printf("Animal speak\n");
    }
};

class Dog(Animal:public) {
public:
    override void speak() {
        printf("Dog: woof!\n");
    }
};
```
### 完整示例
```mio
class Animal {
public:
    name: char*;

    Animal(name: char*) {
        this.name = name;
        printf("Animal ctor: %s\n", name);
    }
    ~Animal() {
        printf("Animal dtor: %s\n", this.name);
    }
    virtual void speak() {
        printf("Animal speak\n");
    }
};

class Cat(Animal:public) {
public:
    Cat(name: char*) {
        this.name = name;
        printf("Cat ctor: %s\n", name);
    }
    override void speak() {
        printf("Cat %s: meow!\n", this.name);
    }
};

i32 main() {
    var dog = Dog("Buddy");
    var cat = Cat("Kitty");
    dog.speak();   # Dog: woof!
    cat.speak();   # Cat: meow!
    return 0;
}
```
> [!NOTE]
> 类与结构体的主要区别：类支持继承和虚函数，默认使用引用语义；结构体不支持继承，默认使用值语义。

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

## 命名空间
**关键字：** `namespace`

命名空间用于组织代码，避免名称冲突。命名空间可以嵌套。
```mio
namespace math {
    i32 add(a: i32, b: i32) {
        return a + b;
    }
    i32 sub(a: i32, b: i32) {
        return a - b;
    }
}

i32 main() {
    var x = math::add(10, 20);   # 用 :: 访问命名空间成员
    return x;
}
```
> [!NOTE]
> 命名空间必须在文件顶层定义，不能出现在函数内部。

## 模板
**关键字：** `template`、`typename`

模板用于编写泛型代码，支持多个模板参数、指定类型和默认值，以及自动类型推导和显式类型参数。

### 语法
```
template<T:typename>
template<T:typename, len:i32=100>
```

- `T:typename` — 类型参数（简写：`T` 等同于 `T:typename`）
- `len:i32=100` — 值参数，类型为 `i32`，默认值为 `100`

### 定义
```mio
template<T:typename>
T max(a: T, b: T) {
    if: a > b {
        return a;
    }
    return b;
}
```
### 使用
```mio
i32 main() {
    var x = max(10, 20);           # 自动推导 T = i32
    var y = max<f64>(3.14, 2.71);  # 显式指定 T = f64
    return 0;
}
```

## 函数参数默认值
函数参数可以指定默认值，调用时可以省略有默认值的参数：
```mio
i32 add(a: i32, b: i32 = 10) {
    return a + b;
}

i32 main() {
    var x = add(5);       # x = 15 (b 使用默认值 10)
    var y = add(5, 20);   # y = 25
    return 0;
}
```

## 作用域规则
### 顶层作用域（文件级别）
以下内容只能出现在文件顶层，不能在函数内部定义：
- `import`
- `struct`
- `class`
- `enum`
- `union`
- `namespace`
- `template`
- `def`
### 块作用域（函数内部）
用 `{}` 包围的代码块可以嵌套，内部定义的变量在块结束后销毁：
```mio
void test() {
    var x = 10;
    {
        var y = 20;   # y 只在此块内有效
        var z = 30;
    }
    # y 和 z 在这里不可见
}
```
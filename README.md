# mio 语言编译器

mio 是一种编译型编程语言，前身是mio解释器&虚拟机，它采用自举方式开发，首个编译版本使用 C 语言实现。mio 编译器将 `.mio` 源文件编译为 C 代码，再通过 C 编译器生成可执行文件。

## 构建

### 环境要求

- C11 兼容的编译器（GCC / Clang / mSVC）
- make（可选）

### 编译 mio 编译器

```bash
# 使用 make
make

# 或手动编译
gcc -std=c11 -o mioc.exe src/main.c src/lexer.c src/parser.c \
    src/ast.c src/token.c src/types.c src/codegen.c
```

## 使用方法

```bash
# 查看版本和许可证
mioc -v
mioc --version

# 编译 mio 源文件（自动生成 .c 文件）
mioc hello.mio

# 指定输出文件
mioc hello.mio -o output.c

# 编译生成的 C 代码
gcc output.c -o hello
./hello
```

## 语言语法

### 导入头文件

```mio
import stdio.h;
import "/include/xxx.mio";
import stdio.h ,"/include/xxx.mio";
```

### 数据类型

| 类型 | 说明 | C 对应类型 |
|------|------|-----------|
| `i32` | 32 位有符号整数 | `int32_t` |
| `i64` | 64 位有符号整数 | `int64_t` |
| `u32` | 32 位无符号整数 | `uint32_t` |
| `u64` | 64 位无符号整数 | `uint64_t` |
| `f32` | 32 位浮点数 | `float` |
| `f64` | 64 位浮点数 | `double` |
| `bool` | 布尔值 | `bool` |
| `char` | 字符 | `char` |
| `T[N]` | 数组 | `T[N]` |

### 变量声明

```mio
var x:i32=10;          # 可变变量
const mAX:i32=100;     # 常量
var a:i32[5];            # 数组
var a:i64,b:i32;            # 同时声明多个
```

### 函数定义

```mio
def i32 add(a:i32,b:i32) {
    return a+b;
}
def i32 add(a:i32,b:i32) {
    a+b #没有分号，就是返回值a+b
}

def void main() {
    printf("hello\n");
}
```

### 控制流

#### if / elif / else

```mio
if:x > 0 {
    printf("正数\n");
} elif:x < 0 {
    printf("负数\n");
} else {
    printf("零\n");
}
```

#### while 循环

```mio
var i: i32 = 0;
while:i < 10 {
    i = i + 1;
}
```

#### for 循环

```mio
var sum: i32 = 0;
for:i = 0; i < 10; i = i + 1 {
    sum = sum + i;
}
```

### 结构体

```mio
struct Point {
    x: f64;
    y: f64;

    # 构造函数（写法一：使用成员初始化列表）
    def Point(xx: f64, yy: f64):x(xx),y(yy) {}

    # 构造函数（写法二：在函数体中赋值）
    def Point(xx: f64, yy: f64) {
        this.x = xx;
        this.y = yy;
    }

    # 方法（this 是指针，自动转为 -> 访问）
    def f64 get_x(this: Point) {
        return this.x;
    }

    # 运算符重载
    def Point operator+(this: Point, other: Point) {
        return Point(this.x + other.x, this.y + other.y);
    }
}
```

### 递归函数

```mio
def i32 factorial(n: i32) {
    if:n <= 1{
        return 1;
    }else{
        return n * factorial(n - 1);
    }
}
```

### 大括号的必要性

mio 中 `{}` 的使用规则如下：

| 场景 | 大括号是否必需 | 示例 |
|------|---------------|------|
| 函数体 | **必需** | `def void main() { ... }` |
| 结构体/枚举体 | **必需** | `struct Point { ... }` |
| if/elif/else 体 | 多语句时必需 | `if:x>0 { ... } else { ... }` |
| while/for 体 | 多语句时必需 | `while:i<10 { i=i+1; }` |
| 单语句体 | **可选** | `if:cond stmt;` |
| 空体 | **必需** | `if:cond {}` / `def void nop() {}` |

**单语句示例：**
```mio
if:x > 0 printf("正数\n");
while:i < 10 i = i + 1;
```

**多语句示例（必须加 `{}`）：**
```mio
if:x > 0 {
    printf("正数\n");
    flag = 1;
}
```

## 编译流程

1. mio 源文件 (.mio) → `mioc` 编译器 → C 源文件 (.c)
2. C 源文件 → C 编译器（gcc 等）→ 可执行文件

## 鸣谢：
[HZY](https://hzy.us.kg)、LIN_DAN、CHE_CHI.

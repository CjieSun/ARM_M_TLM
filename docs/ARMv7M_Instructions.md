# ARM Cortex-M 架构版本控制功能说明

## 概述
已经实现了完整的ARM Cortex-M架构版本控制系统，支持从ARMv6-M到ARMv8-M的不同指令集特性。

## 配置系统

### 核心类型支持
- **Cortex-M0**: ARMv6-M架构
- **Cortex-M0+**: ARMv6-M架构  
- **Cortex-M3**: ARMv7-M架构（向下兼容ARMv6-M）
- **Cortex-M4/M7**: ARMv7E-M架构（DSP扩展）
- **Cortex-M33/M55**: ARMv8-M架构（安全扩展）

### 配置文件
- **ARM_CortexM_Config.h**: 主配置头文件，包含所有架构版本的宏定义

## 新增的ARMv7-M Thumb-16指令支持

### 1. CBZ/CBNZ - 比较并分支零/非零
```assembly
CBZ  Rn, label    ; 如果Rn==0则分支
CBNZ Rn, label    ; 如果Rn!=0则分支
```
- **编码**: `1011xx01 xxxxxxxx`
- **特点**: 提供更紧凑的条件分支，无需显式比较指令
- **宏控制**: `HAS_CBZ_CBNZ`

### 2. IT - If-Then条件执行块
```assembly
IT{x{y{z}}} cond  ; 条件执行后续1-4条指令
```
- **编码**: `10111111 xxxxxxxx`
- **特点**: 允许条件执行多条指令，提高代码密度
- **宏控制**: `HAS_IT_BLOCKS`
- **状态**: 基础框架已实现，完整IT状态机待完善

### 3. 扩展的Hint指令
```assembly
WFI              ; 等待中断
WFE              ; 等待事件
SEV              ; 发送事件
YIELD            ; 让出处理器
```
- **编码**: `10111111 xxxxxxxx` (不同的立即数)
- **特点**: 提供电源管理和多处理器同步支持
- **宏控制**: `HAS_EXTENDED_HINTS`

## 新增的ARMv7-M Thumb-32指令支持

### 4. TBB/TBH - 表分支指令
```assembly
TBB [Rn, Rm]        ; 字节表分支
TBH [Rn, Rm, LSL#1] ; 半字表分支
```
- **编码**: `E8D0 F00x` (x=0:TBB, x=1:TBH)
- **特点**: 高效实现switch语句，从查找表获取分支偏移
- **宏控制**: `SUPPORTS_ARMV7_M`
- **用途**: 编译器优化多路分支

### 5. CLREX - 清除独占监视器
```assembly
CLREX            ; 清除独占访问状态
```
- **编码**: `F3BF 8720`
- **特点**: 与LDREX/STREX配合实现原子操作
- **宏控制**: `SUPPORTS_ARMV7_M`

### 6. 硬件除法指令
```assembly
UDIV Rd, Rn, Rm  ; 无符号除法
SDIV Rd, Rn, Rm  ; 有符号除法
```
- **编码**: `FBB0/FB90 F00x`
- **特点**: 硬件加速除法运算，避免软件实现
- **宏控制**: `HAS_HARDWARE_DIVIDE`
- **错误处理**: 除零时结果为0

### 7. 位字段操作指令
```assembly
BFI  Rd, Rn, #lsb, #width  ; 位字段插入
BFC  Rd, #lsb, #width      ; 位字段清零
UBFX Rd, Rn, #lsb, #width  ; 无符号位字段提取
SBFX Rd, Rn, #lsb, #width  ; 有符号位字段提取
```
- **编码**: `F360/F36F/F3C0/F340`
- **特点**: 高效的位操作，常用于寄存器字段处理
- **宏控制**: `HAS_BITFIELD_INSTRUCTIONS`
- **应用**: 外设寄存器位字段操作

### 8. 饱和运算指令
```assembly
SSAT Rd, #imm, Rn{,shift}  ; 有符号饱和
USAT Rd, #imm, Rn{,shift}  ; 无符号饱和
```
- **编码**: `F300/F380`
- **特点**: 防止算术溢出，结果限制在指定范围内
- **宏控制**: `HAS_SATURATING_ARITHMETIC`
- **标志**: 饱和时设置APSR.Q标志位

## 编译配置

### 默认配置
```cmake
# 默认为Cortex-M0+
cmake .. 
```

### 指定核心类型
```cmake
# 编译为Cortex-M3 (支持ARMv7-M指令)
cmake .. -DARM_CORE_TYPE=CORTEX_M3

# 编译为Cortex-M4 (支持ARMv7E-M指令)  
cmake .. -DARM_CORE_TYPE=CORTEX_M4
```

### 使用构建脚本
```bash
# 构建单个核心
./build_cores.sh CORTEX_M3

# 构建多个核心
./build_cores.sh CORTEX_M0_PLUS CORTEX_M3 CORTEX_M4

# 构建所有支持的核心
./build_cores.sh all
```

## 特性矩阵

| 指令/特性 | M0 | M0+ | M3 | M4/M7 | M33/M55 |
|-----------|----|----|----| ------|---------|
| T16基础指令 | ✓ | ✓ | ✓ | ✓ | ✓ |
| BLX寄存器 | ✗ | ✗ | ✓ | ✓ | ✓ |
| CBZ/CBNZ | ✗ | ✗ | ✓ | ✓ | ✓ |
| IT块 | ✗ | ✗ | ✓ | ✓ | ✓ |
| 扩展Hints | ✗ | ✗ | ✓ | ✓ | ✓ |
| 硬件除法 | ✗ | ✗ | ✓ | ✓ | ✓ |
| DSP扩展 | ✗ | ✗ | ✗ | ✓ | ✓ |
| 安全扩展 | ✗ | ✗ | ✗ | ✗ | ✓ |

## 实现状态

### 已完成
- ✓ 配置系统和宏控制
- ✓ CBZ/CBNZ指令解码和执行（T16）
- ✓ 扩展Hint指令（WFI/WFE/SEV/YIELD）
- ✓ IT指令基础框架
- ✓ TBB/TBH表分支指令（T32）
- ✓ CLREX独占监视器清除（T32）
- ✓ 硬件除法指令UDIV/SDIV（T32）
- ✓ 位字段操作指令BFI/BFC/UBFX/SBFX（T32）
- ✓ 饱和运算指令SSAT/USAT（T32）
- ✓ 构建系统集成

### 待实现
- ⏳ IT状态机完整实现
- ⏳ ARMv7E-M的DSP指令
- ⏳ ARMv8-M的安全扩展指令
- ⏳ 完整的独占监视器功能（LDREX/STREX支持）

## 验证方法

1. **编译验证**: 不同配置下应正常编译
2. **指令识别**: 对应架构的指令应正确解码
3. **特性隔离**: 不支持的指令应被标记为UNKNOWN
4. **执行行为**: CBZ/CBNZ应正确进行条件分支

## 使用示例

```cpp
// 在测试代码中检查特性支持
#if HAS_CBZ_CBNZ
    // 使用CBZ指令的测试代码
    test_cbz_instruction();
#endif

#if HAS_IT_BLOCKS
    // 使用IT块的测试代码  
    test_it_blocks();
#endif
```

这个架构版本控制系统为后续添加更多ARM架构特性提供了强大的基础框架。

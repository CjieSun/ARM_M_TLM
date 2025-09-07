# ARMv7-M 指令集完整性分析

## 当前实现状态

### ✅ 已实现的ARMv7-M指令

#### T16扩展指令
- ✓ CBZ/CBNZ - 比较并分支零/非零
- ✓ IT - If-Then条件执行块
- ✓ WFI/WFE/SEV/YIELD - 扩展Hint指令

#### T32扩展指令  
- ✓ TBB/TBH - 表分支指令
- ✓ CLREX - 清除独占监视器
- ✓ UDIV/SDIV - 硬件除法
- ✓ BFI/BFC/UBFX/SBFX - 位字段操作
- ✓ SSAT/USAT - 饱和运算
- ✓ MSR/MRS - 系统寄存器访问（基础版本）
- ✓ DSB/DMB/ISB - 内存屏障指令

### ❌ 缺失的重要ARMv7-M指令

#### 1. 独占访问指令（原子操作）
**优先级：高** - 多线程/RTOS支持的关键指令
```assembly
LDREX   Rt, [Rn]           ; 独占加载
LDREXB  Rt, [Rn]           ; 独占加载字节  
LDREXH  Rt, [Rn]           ; 独占加载半字
STREX   Rd, Rt, [Rn]       ; 独占存储
STREXB  Rd, Rt, [Rn]       ; 独占存储字节
STREXH  Rd, Rt, [Rn]       ; 独占存储半字
```
- **编码**: T32指令
- **用途**: 实现原子操作、自旋锁、信号量
- **状态**: 已定义HAS_EXCLUSIVE_ACCESS标志，但指令未实现

#### 2. T32数据处理指令扩展
**优先级：中** - 提供更大的立即数范围和更多操作数选项
```assembly
ADD.W   Rd, Rn, #imm12     ; 32位加法（大立即数）
SUB.W   Rd, Rn, #imm12     ; 32位减法
ADC.W   Rd, Rn, Rm        ; 32位带进位加法
SBC.W   Rd, Rn, Rm        ; 32位带进位减法  
RSB.W   Rd, Rn, #imm12     ; 32位反向减法
MOV.W   Rd, #imm16         ; 32位移动（大立即数）
MVN.W   Rd, #imm12         ; 32位按位取反
```

#### 3. T32比较和测试指令
```assembly
CMP.W   Rn, #imm12         ; 32位比较（大立即数）
CMN.W   Rn, #imm12         ; 32位比较负数
TST.W   Rn, #imm12         ; 32位测试
TEQ.W   Rn, #imm12         ; 32位测试等于
```

#### 4. T32逻辑指令
```assembly
AND.W   Rd, Rn, #imm12     ; 32位按位与
ORR.W   Rd, Rn, #imm12     ; 32位按位或
EOR.W   Rd, Rn, #imm12     ; 32位按位异或
BIC.W   Rd, Rn, #imm12     ; 32位位清除
```

#### 5. T32移位指令
```assembly
LSL.W   Rd, Rm, #imm       ; 32位逻辑左移
LSR.W   Rd, Rm, #imm       ; 32位逻辑右移  
ASR.W   Rd, Rm, #imm       ; 32位算术右移
ROR.W   Rd, Rm, #imm       ; 32位循环右移
```

#### 6. 位操作指令
**优先级：中** - 提高位操作效率
```assembly
CLZ     Rd, Rm             ; 计算前导零个数
RBIT    Rd, Rm             ; 位反转
REV.W   Rd, Rm             ; 32位字节序反转
REV16.W Rd, Rm             ; 32位半字字节序反转  
REVSH.W Rd, Rm             ; 32位有符号半字字节序反转
```

#### 7. 多寄存器加载/存储扩展
**优先级：低** - T16版本已足够
```assembly
LDMDB   Rn!, reglist       ; 递减前加载多寄存器
STMDB   Rn!, reglist       ; 递减前存储多寄存器
LDM.W   Rn!, reglist       ; T32多寄存器加载
STM.W   Rn!, reglist       ; T32多寄存器存储
```

#### 8. CBZ/CBNZ的T32变体
**优先级：低** - T16版本覆盖大部分用例
```assembly
CBZ.W   Rn, label          ; 32位版本（更大偏移范围）
CBNZ.W  Rn, label          ; 32位版本
```

## 建议实现优先级

### 第一阶段：独占访问指令（高优先级）
- LDREX/STREX系列指令
- 独占监视器状态管理
- 原子操作支持

### 第二阶段：常用T32数据处理指令（中优先级）  
- ADD.W/SUB.W/MOV.W等大立即数版本
- CMP.W/TST.W等测试指令
- 逻辑指令AND.W/ORR.W/EOR.W/BIC.W

### 第三阶段：位操作指令（中优先级）
- CLZ计算前导零
- RBIT位反转
- REV系列字节序反转

### 第四阶段：其他指令（低优先级）
- T32移位指令
- 多寄存器操作的T32变体
- CBZ/CBNZ的T32变体

## 实现建议

1. **独占访问指令**最重要，是RTOS和多线程支持的基础
2. **T32数据处理指令**提供更大的立即数范围，提高编译器生成代码的效率
3. **位操作指令**在嵌入式系统中使用频繁，建议优先实现CLZ和RBIT
4. **其他指令**可以根据实际需求逐步添加

总的来说，当前ARMv7-M指令集实现已经覆盖了主要的新增特性，缺失的主要是独占访问指令和T32数据处理指令的完整版本。

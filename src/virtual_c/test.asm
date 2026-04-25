# test.asm - 简单测试程序
# 功能：计算 5 + 3，结果存入内存 0x100

MOV R0, $5       # R0 = 5
MOV R1, $3       # R1 = 3
ADD R0, R1       # R0 = R0 + R1  (8)
STORE R0, $0x100 # 将 R0 的值存入内存地址 0x100
HALT             # 停机
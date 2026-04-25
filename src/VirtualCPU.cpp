#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include<stddef.h>
#include<stdlib.h>
#include<vector>
#include<string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <iomanip>
#include <limits>
namespace fs=std::filesystem;
using namespace std;
int NAND(int x, int y)/*从与非门开始吧*/
{
    return !(x && y);
}
/*希望您没有忘记
    NAND(1,1)=0
    NAND(1,0)=1
    NAND(0,1)=1
    NAND(0,0)=1
*/
/*以下是基础门电路的构建*/
int NOT(int x)
{
    return NAND(x, x);
}
int AND(int x, int y)
{
    return NOT(NAND(x, y));
}
int OR(int x, int y)
{
    return NAND(NOT(x), NOT(y));
}
int XOR(int x, int y)
{
    int a = NAND(x, y);
    int b = NAND(x, a);
    int c = NAND(y, a);
    return NAND(b, c);
}
/*标准的门电路搭建*/
/*接下来是一位全加器*/
typedef struct
{
    int sum;
    int cout;
}FullAdder;/*sum 代表和，cout代表进位*/
/*希望您始终没有忘记
一个一位全加器是一个三个输入两个输出的函数
int a,int b,int carry carry是从其它地方来的进位
return sum=a+b+carry的低位值
cout为进位
*/
FullAdder full_adder(int x, int y, int cin)
{
    int s1 = XOR(x, y);/*sum=x+y*/
    int sum = XOR(s1, cin);
    int c1 = AND(x, y);/*carry of x+y*/
    int c2 = AND(s1, cin);
    int cout = OR(c1, c2);
    return { sum, cout };
}
/*十六位加法器,对一位全加器的扩展*/

uint16_t add16(uint16_t x, uint16_t y, uint16_t* carry_out)
{
    uint16_t result = 0;/*总和*/
    uint16_t carry = 0;/*总进位*/
    for (int i = 0; i < 16; i++)
    {
        int bit_x = (x >> i) & 1;/*取第i位*/
        int bit_y = (y >> i) & 1;
        FullAdder fa = full_adder(bit_x, bit_y, carry);
        if (fa.sum)
        {
            result = result | (1 << i);/*将第i位的bit赋给result*/
        }
        carry = fa.cout;
    }
    if (carry_out)
    {
        *carry_out = carry;/*如果传入了非空指针，则取出最高位的进位*/
    }
    return result;
}
uint16_t sub16(uint16_t x, uint16_t y, uint16_t* borrow_out)
{
    uint16_t neg_y = add16(-y, 1, NULL);
    uint16_t result = add16(x, neg_y, borrow_out);
    return result;
}
uint16_t and16(uint16_t x, uint16_t y)
{
    uint16_t res = 0;
    for (int i = 0; i < 16; i++)
    {
        if (AND((x >> i) & 1, (y >> i) & 1))
        {
            res = res | (1 << i);
        }
    }
    return res;
}
uint16_t or16(uint16_t x, uint16_t y)
{
    uint16_t res = 0;
    for (int i = 0; i < 16; i++)
    {
        if (OR((x >> i) & 1, (y >> i) & 1))
        {
            res = res | (1 << i);
        }
    }
    return res;
}
uint16_t xor16(uint16_t x, uint16_t y)
{
    uint16_t res = 0;
    for (int i = 0; i < 16; i++)
    {
        if (XOR((x >> i) & 1, (y >> i) & 1))
        {
            res = res | (1 << i);
        }
    }
    return res;
}
/*以上是十六位逻辑和运算的实现*/
/*接下来是虚拟算术逻辑单元的搭建*/

#define ALU_ADD 0
#define ALU_SUB 1
#define ALU_AND 2
#define ALU_OR 3
#define ALU_XOR 4

typedef struct
{
    uint16_t result;
    uint16_t carry;
    uint16_t zero;
    uint16_t negative;
}ALUout;/*ALU计算的结果只有四个
    result
    carry 进位
    zero 结果是不是0
    negative 结果是不是负数
*/
ALUout alu(uint16_t x, uint16_t y, uint16_t op)
{
    ALUout out = { 0,0,0,0 };
    uint16_t carry = 0;
    if (op == ALU_ADD)
    {
        out.result = add16(x, y, &carry);
        out.carry = carry;
    }
    else if (op == ALU_SUB)
    {
        out.result = sub16(x, y, &carry);
        out.carry = carry;
    }
    else if (op == ALU_AND)
    {
        out.result = and16(x, y);
    }
    else if (op == ALU_OR)
    {
        out.result = or16(x, y);
    }
    else if (op == ALU_XOR)
    {
        out.result = xor16(x, y);
    }
    out.zero = (out.result == 0);
    out.negative = ((out.result & 0x8000) != 0);
    return out;
}/*ALU计算的核心代码*/
/*cpu的定义*/
#define MEM_SIZE 65536 /*classic 64KB*/
#define REG_COUNT 8/*R0 ~ R7,通用寄存器*/

typedef struct
{
    uint16_t reg[REG_COUNT];/*寄存器数组*/
    uint32_t pc;/*下一条指令*/
    uint8_t flags[3];/*标志位寄存器 0=>Carry 1=>Zero 2=>Negative*/
    uint8_t  mem[MEM_SIZE];/*内存*/
}CPU;

uint16_t mem_read16(CPU* cpu, uint16_t addr)
{
    if (addr + 1 >= MEM_SIZE)
    {
        return 0;/*超出范围，返回空单元*/
    }
    return  (uint16_t)cpu->mem[addr] | ((uint16_t)cpu->mem[addr + 1] << 8);
}
/*我们假设是小端序:
内存数组下标越小，其内存越高
低位放高地址
高位放低地址
mem[addr]:高地址
mem[addr+1]:低地址
mem[addr]|(mem[addr+1]<<8)
*/
void mem_write16(CPU* cpu, uint16_t addr, uint16_t value)
{
    if (addr + 1 > MEM_SIZE)
    {
        return;
    }
    cpu->mem[addr] = (value & 0xFF);
    cpu->mem[addr + 1] = ((value >> 8) & 0xFF);
}
/*
0x01	MOV	Rd = Rs 或 Rd = imm16
0x02	ADD	Rd = Rd + Rs/imm
0x03	SUB	Rd = Rd - Rs/imm
0x04	AND	Rd = Rd & Rs/imm
0x05	OR	Rd = Rd | Rs/imm
0x06	XOR	Rd = Rd ^ Rs/imm
0x10	CMP	计算 Rd - Rs/imm，只改标志
0x20	JMP	无条件跳转到imm16（注意：此指令忽略Rd）
0x21	JZ	若Z=1，跳转到imm16
0x22	JNZ	若Z=0，跳转到imm16
0x30	LOAD	Rd = MEM[Rs/imm]（内存读取16位）
0x31	STORE	MEM[Rs/imm] = Rd（内存写入16位）
0xFF	HALT	停机
机器码五个字节:
OPCODE DST_REG IS_REG SRC_REG 0x00或者是
OPCODE DST_REG IS_REG IMML IMMH
*/
typedef struct
{
    uint8_t opcode;
    uint8_t dst_reg;
    uint8_t src_type;
    uint8_t src_reg;
    uint16_t imm;
}Instruction;

Instruction fetch_instruction(CPU* cpu)
{
    uint16_t pc = (uint16_t)(cpu->pc);
    Instruction ins = { 0 };
    ins.opcode = cpu->mem[pc];
    ins.dst_reg = (cpu->mem[pc + 1]) & 0x0F;
    ins.src_type = (cpu->mem[pc + 2]) & 0x0F;
    if (ins.src_type == 0)
    {
        ins.src_reg = (cpu->mem[pc + 3]) & 0x0F;
        ins.imm = 0;/*也可以写成ins.imm=(cpu->mem[pc+4])一般会填充0x00*/
    }
    else if (ins.src_type == 1)
    {
        ins.src_reg = 0;
        ins.imm = (uint16_t)cpu->mem[pc + 3] | ((uint16_t)cpu->mem[pc + 4] << 8);
    }
    else
    {
        ins.src_reg = 0;
        ins.imm = 0;
    }
    return ins;
}
void execute_instruction(CPU* cpu, Instruction ins)
{
    uint16_t src_val;
    if (ins.src_type == 1)
    {
        src_val = ins.imm;
    }
    else
    {
        src_val = cpu->reg[ins.src_reg];
    }
    ALUout alu_out;
    uint8_t opcode = ins.opcode;
    if (opcode == 0x01)//MOV
    {
        cpu->reg[ins.dst_reg] = src_val;
    }
    else if (opcode == 0x02)//ADD
    {
        alu_out = alu(cpu->reg[ins.dst_reg], src_val, ALU_ADD);
        cpu->reg[ins.dst_reg] = alu_out.result;
        cpu->flags[0] = alu_out.carry;
        cpu->flags[1] = alu_out.zero;
        cpu->flags[2] = alu_out.negative;
    }
    else if (opcode == 0x03)//SUB
    {
        alu_out = alu(cpu->reg[ins.dst_reg], src_val, ALU_SUB);
        cpu->reg[ins.dst_reg] = alu_out.result;
        cpu->flags[0] = alu_out.carry;
        cpu->flags[1] = alu_out.zero;
        cpu->flags[2] = alu_out.negative;
    }
    else if (opcode == 0x04)
    {
        alu_out = alu(cpu->reg[ins.dst_reg], src_val, ALU_AND);
        cpu->reg[ins.dst_reg] = alu_out.result;
        cpu->flags[1] = alu_out.zero;
        cpu->flags[2] = alu_out.negative;
    }
    else if (opcode == 0x05)
    {
        alu_out = alu(cpu->reg[ins.dst_reg], src_val, ALU_OR);
        cpu->reg[ins.dst_reg] = alu_out.result;
        cpu->flags[1] = alu_out.zero;
        cpu->flags[2] = alu_out.negative;
    }
    else if (opcode == 0x06)
    {
        alu_out = alu(cpu->reg[ins.dst_reg], src_val, ALU_XOR);
        cpu->reg[ins.dst_reg] = alu_out.result;
        cpu->flags[1] = alu_out.zero;
        cpu->flags[2] = alu_out.negative;
    }
    else if (opcode == 0x10)
    {
        alu_out = alu(cpu->reg[ins.dst_reg], src_val, ALU_SUB);
        cpu->flags[0] = alu_out.carry;
        cpu->flags[1] = alu_out.zero;
        cpu->flags[2] = alu_out.negative;
    }
    else if (opcode == 0x20)/*处理JMP*/
    {
        if (ins.src_type == 1)
        {
            cpu->pc = src_val;
            return;
        }
    }
    else if (opcode == 0x21)
    {
        if (cpu->flags[1] == 1 && ins.src_type == 1)
        {
            cpu->pc = src_val;
            return;
        }
    }
    else if (opcode == 0x22)
    {
        if (cpu->flags[1] == 0 && ins.src_type == 1)
        {
            cpu->pc = src_val;
            return;
        }
    }
    else if (opcode == 0x30)/*LOAD*/
    {
        uint16_t addr = src_val;  // 源操作数是内存地址（立即数或寄存器值）
        if (ins.src_type == 0)    // 寄存器模式：从寄存器取地址
        {
            addr = cpu->reg[ins.src_reg];
        }
        uint16_t value = mem_read16(cpu, addr);
        cpu->reg[ins.dst_reg] = value;
    }
    else if (opcode == 0x31)/*STORE*/
    {
        uint16_t addr = src_val;
        if (ins.src_type == 0)
        {
            addr = cpu->reg[ins.src_reg];
        }
        mem_write16(cpu, addr, cpu->reg[ins.dst_reg]);
    }
    else if (opcode == 0xFF)/*停机*/
    {
        cpu->pc = MEM_SIZE;
        return;
    }
    else/*默认停机*/
    {
        cpu->pc = MEM_SIZE;
        return;
    }
    cpu->pc = cpu->pc + 5;
}
void cpu_run(CPU* cpu, int max_steps) {
    int steps = 0;
    while (cpu->pc < MEM_SIZE && steps < max_steps) {
        Instruction ins = fetch_instruction(cpu);
        execute_instruction(cpu, ins);
        steps++;
    }
}
void cpu_init(CPU* cpu)
{
    memset(cpu->reg, 0, sizeof(cpu->reg));
    cpu->pc = 0;
    memset(cpu->flags, 0, sizeof(cpu->flags));
    memset(cpu->mem, 0, sizeof(cpu->mem));
}
int line;/*行数*/
int token;/*current token*/
char* src;/*next char*/
enum
{
    OPERATOR,
    REGISTER,
    IMM,
    CL,
    NOTE,
    COMMA,
    ERR,
    END
};
/*
0x01	MOV	Rd = Rs 或 Rd = imm16
0x02	ADD	Rd = Rd + Rs/imm
0x03	SUB	Rd = Rd - Rs/imm
0x04	AND	Rd = Rd & Rs/imm
0x05	OR	Rd = Rd | Rs/imm
0x06	XOR	Rd = Rd ^ Rs/imm
0x10	CMP	计算 Rd - Rs/imm，只改标志
0x20	JMP	无条件跳转到imm16（注意：此指令忽略Rd）
0x21	JZ	若Z=1，跳转到imm16
0x22	JNZ	若Z=0，跳转到imm16
0x30	LOAD	Rd = MEM[Rs/imm]（内存读取16位）
0x31	STORE	MEM[Rs/imm] = Rd（内存写入16位）
0xFF	HALT	停机
机器码五个字节:
OPCODE DST_REG IS_REG SRC_REG 0x00或者是
OPCODE DST_REG IS_REG IMML IMMH
*/
vector<uint16_t> next()
{
        vector<uint16_t>result = { (uint16_t)(ERR),0 };
        string msg;
        while (*src == ' ' || *src == '\t')
            src++;

        // 2. 检查是否到达文件末尾
        if (*src == '\0') {
            result[0] = (uint16_t)END;
            result[1] = 0;
            return result;
        }
        token=*src++;
        if (token == '\n')/*遇到换行符返回{CL,0}*/
        {
            result[0] = (uint16_t)(CL);
            result[1] = 0;
            line++;
            return result;
        }
        else if (token == '#')
        {
            result[0] = (uint16_t)(NOTE);
            result[1] = 0;
            while (*src != '\0' && *src != '\n')
            {
                src++;
            }
            return result;
        }
        else if (token == ',')
        {
            result[0] = (uint16_t)(COMMA);
            result[1] = 0;
            return result;
        }
        else if (token == '\0')
        {
            result[0] = (uint16_t)(END);
            result[1] = 0;
            return result;
        }
        else if ((token >= 'A' && token <= 'Z') || (token >= 'a' && token <= 'z'))
        {
            msg.push_back((char)(token));/*把第一个字符push进去*/
            while ((*src >= 'A' && *src <= 'Z') || (*src >= 'a' && *src <= 'z') || (*src >= '0' && *src <= '9'))
            {
                msg.push_back(*src++);
            }
            if (msg == "MOV" || msg == "mov")
            {
                result[0] = (uint16_t)(OPERATOR);
                result[1] = (uint16_t)(0x01);
            }
            else if (msg == "ADD" || msg == "add")
            {
                result[0] = (uint16_t)(OPERATOR);
                result[1] = (uint16_t)(0x02);
            }
            else if (msg == "SUB" || msg == "sub")
            {
                result[0] = (uint16_t)(OPERATOR);
                result[1] = (uint16_t)(0x03);
            }
            else if (msg == "AND" || msg == "and")
            {
                result[0] = (uint16_t)(OPERATOR);
                result[1] = (uint16_t)(0x04);
            }
            else if (msg == "OR" || msg == "or")
            {
                result[0] = (uint16_t)(OPERATOR);
                result[1] = (uint16_t)(0x05);
            }
            else if (msg == "XOR" || msg == "xor")
            {
                result[0] = (uint16_t)(OPERATOR);
                result[1] = (uint16_t)(0x06);
            }
            else if (msg == "CMP" || msg == "cmp")
            {
                result[0] = (uint16_t)(OPERATOR);
                result[1] = (uint16_t)(0x10);
            }
            else if (msg == "JMP" || msg == "jmp")
            {
                result[0] = (uint16_t)(OPERATOR);
                result[1] = (uint16_t)(0x20);
            }
            else if (msg == "JZ" || msg == "jz")
            {
                result[0] = (uint16_t)(OPERATOR);
                result[1] = (uint16_t)(0x21);
            }
            else if (msg == "JNZ" || msg == "jnz")
            {
                result[0] = (uint16_t)(OPERATOR);
                result[1] = (uint16_t)(0x22);
            }
            else if (msg == "LOAD" || msg == "load")
            {
                result[0] = (uint16_t)(OPERATOR);
                result[1] = (uint16_t)(0x30);
            }
            else if (msg == "STORE" || msg == "store")
            {
                result[0] = (uint16_t)(OPERATOR);
                result[1] = (uint16_t)(0x31);
            }
            else if (msg == "HALT" || msg == "halt")
            {
                result[0] = (uint16_t)(OPERATOR);
                result[1] = (uint16_t)(0xFF);
            }
            else if (msg == "R0" || msg == "r0")
            {
                result[0] = (uint16_t)(REGISTER);
                result[1] = (uint16_t)(0x00);
            }
            else if (msg == "R1" || msg == "r1")
            {
                result[0] = (uint16_t)(REGISTER);
                result[1] = (uint16_t)(0x01);
            }
            else if (msg == "R2" || msg == "r2")
            {
                result[0] = (uint16_t)(REGISTER);
                result[1] = (uint16_t)(0x02);
            }
            else if (msg == "R3" || msg == "r3")
            {
                result[0] = (uint16_t)(REGISTER);
                result[1] = (uint16_t)(0x03);
            }
            else if (msg == "R4" || msg == "r4")
            {
                result[0] = (uint16_t)(REGISTER);
                result[1] = (uint16_t)(0x04);
            }
            else if (msg == "R5" || msg == "r5")
            {
                result[0] = (uint16_t)(REGISTER);
                result[1] = (uint16_t)(0x05);
            }
            else if (msg == "R6" || msg == "r6")
            {
                result[0] = (uint16_t)(REGISTER);
                result[1] = (uint16_t)(0x06);
            }
            else if (msg == "R7" || msg == "r7")
            {
                result[0] = (uint16_t)(REGISTER);
                result[1] = (uint16_t)(0x07);
            }
            else
            {
                result[0] = (uint16_t)(ERR);
                result[1] = (uint16_t)(0x00);
            }
            return result;
        }
        else if (token == '$')
        {
            result[0] = (uint16_t)(IMM);
            uint16_t sum = 0;

            // 检查是否为十六进制数：以 0x 或 0X 开头
            if (src[0] == '0' && (src[1] == 'x' || src[1] == 'X'))
            {
                src += 2;  // 跳过 "0x" 前缀

                // 解析十六进制数字
                while ((*src >= '0' && *src <= '9') ||
                    (*src >= 'A' && *src <= 'F') ||
                    (*src >= 'a' && *src <= 'f'))
                {
                    char c = *src++;
                    uint16_t digit = 0;
                    if (c >= '0' && c <= '9')
                        digit = c - '0';
                    else if (c >= 'a' && c <= 'f')
                        digit = c - 'a' + 10;
                    else if (c >= 'A' && c <= 'F')
                        digit = c - 'A' + 10;
                    else
                    {
                        result[0] = (uint16_t)(ERR);
                        break;
                    }
                    sum = sum * 16 + digit;
                }
                result[1] = sum;
            }
            else  // 否则按十进制解析
            {
                while (*src >= '0' && *src <= '9')
                {
                    char c = *src++;
                    uint16_t digit = c - '0';
                    sum = sum * 10 + digit;
                }
                result[1] = sum;
            }
            return result;
        }
        else
        {
            result[0] = (uint16_t)(ERR);
            result[1] = (uint16_t)(0x00);
            return result;
        }
    return result;
}
vector<vector<uint16_t>> fetch_line_instruction()
{
    vector<vector<uint16_t>> line_instruction;
    vector<uint16_t> next_token = next();
    
    if (next_token[0] == (uint16_t)CL)
        return line_instruction;               // 空行，返回空向量
    
    if (next_token[0] == (uint16_t)END) {
        line_instruction.push_back(next_token); // 文件结束，将 END 放入向量并返回
        return line_instruction;
    }
    
    line_instruction.push_back(next_token);
    
    while (1) {
        next_token = next();
        line_instruction.push_back(next_token);
        // 遇到换行或文件末尾立即停止（但仍保留该 token 在向量中）
        if (next_token[0] == (uint16_t)CL || next_token[0] == (uint16_t)END)
            break;
    }
    
    return line_instruction;
}
vector<uint8_t>line_instruction_interpretor(vector<vector<uint16_t>>& line_instruction)
{
    size_t n = line_instruction.size();
    vector<uint8_t>result;
    int reg_have_seen = 0;
    for (int i = 0; i < n; i++)
    {
        vector<uint16_t>current_token = line_instruction[i];
        // 忽略逗号、注释、换行符（它们不参与机器码生成）
        if (current_token[0] == (uint16_t)COMMA ||
            current_token[0] == (uint16_t)NOTE ||
            current_token[0] == (uint16_t)CL)
            continue;
        if (current_token[0] == (uint16_t)(OPERATOR))
        {
            result.push_back((uint8_t)(current_token[1]&0xFF));/*取低8位*/
            if (current_token[1] == (uint16_t)(0x20))/*对于JMP $0x1234*/
            {
                result.push_back((uint8_t)(0x00));/*自动填充DST_REG=0x00*/
            }
            else if (current_token[1] == (uint16_t)(0x21))
            {
                result.push_back((uint8_t)(0x00));/*自动填充DST_REG=0x00*/
            }
            else if (current_token[1] == (uint16_t)(0x22))
            {
                result.push_back((uint8_t)(0x00));
            }
            else if (current_token[1] == (uint16_t)(0xFF))
            {
                for (int i = 0; i < 4; i++)
                {
                    result.push_back((uint8_t)(0x00));/*停机指令直接填充0x00,4个*/
                }
            }
        }
        else if (current_token[0] == (uint16_t)(REGISTER))
        {
            reg_have_seen++;
            if (reg_have_seen == 1)
            {
                result.push_back((uint8_t)((current_token[1]) & 0xFF));
            }
            else if(reg_have_seen==2)
            {
                result.push_back(0x00);/*非立即数模式*/
                result.push_back((uint8_t)((current_token[1]) & 0xFF));
                result.push_back((uint8_t)(0x00));
            }
        }
        else if (current_token[0] == (uint16_t)(IMM))
        {
            result.push_back((uint8_t)(0x01));/*立即数模式*/
            result.push_back((uint8_t)((current_token[1]) & 0xFF));/*取低8位*/
            result.push_back((uint8_t)((current_token[1]>>8)& 0xFF));/*取高八位*/
        }
        else if (current_token[0] == (uint16_t)(END))
        {
            for (int i = 0; i < 5; i++)
            {
                result.push_back((uint8_t)(0x00));
            }
        }
    }
    return result;
}
vector<uint8_t> assemble(const char* source) {
    src = (char*)source;      // 设置全局 src 指针
    line = 1;
    token = 0;

    vector<uint8_t> machine_code;
    while (*src != '\0') {
        // 跳过空行或注释行（next 已经处理了注释，但可能返回 NOTE 或 CL）
        vector<vector<uint16_t>> line_tokens = fetch_line_instruction();

        // 如果该行只有换行符或注释，跳过
        if (line_tokens.empty()) continue;

        // 判断是否到文件尾
        if (line_tokens.size() == 1 && line_tokens[0][0] == (uint16_t)END)
            break;

        // 将一行 token 翻译为 5 字节机器码
        vector<uint8_t> bytes = line_instruction_interpretor(line_tokens);
        if(bytes.empty())
        {
            continue;
        }

        // 确保每条指令严格产生 5 字节（不足则补 0，超出则截断）
        while (bytes.size() < 5) bytes.push_back(0x00);
        if (bytes.size() > 5) bytes.resize(5);

        machine_code.insert(machine_code.end(), bytes.begin(), bytes.end());
    }
    return machine_code;
}
void cpu_step(CPU* cpu)
{
    if(cpu->pc>=MEM_SIZE)
    {
        return;
    }
    Instruction ins=fetch_instruction(cpu);
    execute_instruction(cpu,ins);
}
void print_register(CPU* cpu)
{
    cout<<"PC=0x"<<hex<<setw(4)<<setfill('0')<< cpu->pc << "  ";
    for(int i=0;i<REG_COUNT;i++)
    {
        cout<<"R"<<dec<<i<<"=0x"<<hex<<setw(4)<<setfill('0')<<cpu->reg[i]<<" ";
    }
    std::cout << "Flags: C=" << (int)cpu->flags[0]
              << " Z=" << (int)cpu->flags[1]
              << " N=" << (int)cpu->flags[2] << std::dec << "\n";
}
void dump_memory(const CPU* cpu, uint16_t start_addr, size_t length = 64) {
    std::cout << "Memory dump from 0x" << std::hex << std::setw(4) << std::setfill('0') << start_addr << ":\n";
    size_t dumped = 0;
    const size_t bytes_per_line = 16;

    for (size_t offset = 0; offset < length && start_addr + offset < MEM_SIZE; ++offset) {
        if (offset % bytes_per_line == 0) {
            if (offset != 0) std::cout << "\n";
            std::cout << std::hex << std::setw(4) << std::setfill('0') << (start_addr + offset) << ": ";
        }
        uint8_t byte = cpu->mem[start_addr + offset];
        std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)byte << " ";
        dumped++;
    }
    std::cout << std::dec << "\n(dumped " << dumped << " bytes)\n";
}
string read_file(string& filename)
{
    ifstream file(filename);
    if(!file.is_open())
    {
        throw runtime_error("Can't open source file");
    }
    ostringstream oss;
    oss<<file.rdbuf();
    return oss.str();
}
void load_program(CPU* cpu,vector<uint8_t>& machine_code)/*把机器码烧录到内存中*/
{
    size_t size=machine_code.size();
    if(size>=MEM_SIZE)
    {
        throw runtime_error("Program too large to fit memory");
    }
    for(int i=0;i<size;i++)
    {
        cpu->mem[i]=machine_code[i];
    }
    cpu->pc=0;
}
void debug_session(string& asm_filename)
{
    CPU cpu;
    cpu_init(&cpu);
     try {
        string source = read_file(asm_filename);
        vector<uint8_t> code = assemble(source.c_str());
        load_program(&cpu, code);
        cout << "Loaded " << code.size() << " bytes from " << asm_filename << "\n";
    } catch (const std::exception& e) {
        cerr << "Error: " << e.what() << "\n";
        return;
    }
    string line;
    while(true)
    {
        cout<<"(debug)";
        if(!getline(cin,line))break;
        if(line.empty())continue;
        istringstream iss(line);
        string cmd;
        iss>>cmd;
        if(cmd=="t")
        {
            if(cpu.pc>=MEM_SIZE)
            {
                cout<<"program halted or PC out of memory\n";
                print_register(&cpu);
            }
            cpu_step(&cpu);
            print_register(&cpu);
        }
        else if(cmd=="u")
        {
            int steps=0;
            int MAX_STEPS=1000000;
            while (cpu.pc < MEM_SIZE && steps < MAX_STEPS) {
                // 检查是否为 HALT 指令（0xFF）
                if (cpu.mem[cpu.pc] == 0xFF) {
                    // 执行 HALT 指令以设置 PC 等
                    cpu_step(&cpu);
                    break;
                }
                cpu_step(&cpu);
                steps++;
            }
            if (steps == MAX_STEPS) {
                std::cout << "Execution stopped after " << MAX_STEPS << " steps (possible infinite loop).\n";
            } else {
                std::cout << "Execution finished.\n";
            }
            print_register(&cpu);
        }
        else if(cmd=="d")
        {
            string addr_str;
            iss>>addr_str;
            if(addr_str.empty())
            {
                cout<<"Usage d <address>\n";
                continue;
            }
            uint16_t addr=0;
            try {/*支持两种寻址 0xhex+dec*/
                if (addr_str.size() > 2 && addr_str[0] == '0' && (addr_str[1] == 'x' || addr_str[1] == 'X'))
                    addr = std::stoi(addr_str, nullptr, 16);
                else
                    addr = std::stoi(addr_str, nullptr, 0);
            } catch (...) {
                std::cout << "Invalid address.\n";
                continue;
            }
            dump_memory(&cpu, addr, 64);
        }
        else if(cmd=="q")
        {
            cout<<"Exiting debug mode\n";
            break;
        }
        else
        {
            cout<<"Unknown Commad,command available==t,u,d,q\n";
        }
    }
}
void list_asm_files() {
    const std::string dir_name = "virtual_c";
    if (!fs::exists(dir_name) || !fs::is_directory(dir_name)) {
        std::cout << "Directory 'virtual_c' does not exist.\n";
        return;
    }
    std::cout << "Assembly files in ./virtual_c:\n";
    for (const auto& entry : fs::directory_iterator(dir_name)) {
        if (entry.is_regular_file() && entry.path().extension() == ".asm") {
            std::cout << "  " << entry.path().filename().string() << "\n";
        }
    }
}
int main() {
    std::cout << "Simple CPU Emulator with Debugger\n";
    std::cout << "Commands: ls, debug <file.asm>, exit\n";

    std::string line;
    while (true) {
        std::cout << "> ";
        if (!std::getline(std::cin, line)) break;
        if (line.empty()) continue;

        std::istringstream iss(line);
        std::string cmd;
        iss >> cmd;

        if (cmd == "ls") {
            list_asm_files();
        }
        else if (cmd == "debug") {
            std::string filename;
            iss >> filename;
            if (filename.empty()) {
                std::cout << "Usage: debug <filename.asm>\n";
                continue;
            }
            // 如果未指定完整路径，假设文件在 virtual_c 目录下
            std::string full_path = "virtual_c/" + filename;
            debug_session(full_path);
        }
        else if (cmd == "exit") {
            std::cout << "Goodbye.\n";
            break;
        }
        else {
            std::cout << "Unknown command. Available: ls, debug, exit\n";
        }
    }
    return 0;
}
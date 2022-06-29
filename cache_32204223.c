#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "need.h"

//global vars
//architectural states
int memory[0x400000]; // 16MB memory
int pc = 0;
int regs[32] = { 0 };
int cycle = 0;
int exe = 0;
int nope_exe = 0;
int num_Itype = 0;
int num_Rtype = 0;
int num_Jtype = 0;
int mem_access = 0;
int hit_n = 0;
int miss_n = 0;
double hit_ratio = 0;
int finish = 0;

struct ContSig {
	int pc;
	int RegDest;
	int ALUSrc;
	int MemtoReg;
	int RegWrite;
	int MemRead;
	int MemWrite;
	int ALUop;
	int Branch;
	int j_;
	int jr_;
	int jal_;
};

typedef struct IF_ID_latch {
	int pc_4;
	int inst;
	int valid;
}IF_ID;

IF_ID IF_ID_latch[2]; // if_id_latch[0] = output, if_id_latch[1] = input
IF_ID IF_ID_;

typedef struct ID_EX_latch {
	int opcode;
	int rs;
	int rs_v; //value1
	int rt;
	int rt_v; //value2
	int rd;
	int shamt;
	int func;
	int imm;
	int simm;
	int address;
	int Btarget;
	int Jtarget;
	int tReg;
	int pc_4;
	struct ContSig cs;
	int valid;
} ID_EX;

ID_EX ID_EX_latch[2];
ID_EX ID_EX_;

typedef struct EX_M_latch {
	int btarget;
	int bcond;
	int ALUResult;
	int tReg;
	int rt_v;
	struct ContSig cs;
	int valid;
} EX_M;

EX_M EX_M_latch[2];
EX_M EX_M_;

typedef struct M_WB_latch {
	int LMD;
	int tReg;
	struct ContSig cs;
	int valid;
}M_WB;

M_WB M_WB_latch[2];
M_WB M_WB_;

void mux() {
	if (ID_EX_latch[0].rs == M_WB_latch[1].tReg && M_WB_latch[1].cs.RegWrite) {
		ID_EX_latch[0].rs_v = M_WB_latch[1].LMD;
	}
	else {
		ID_EX_latch[0].rs_v = regs[ID_EX_latch[0].rs];
	}

	if (ID_EX_latch[0].rt == M_WB_latch[1].tReg&&M_WB_latch[1].cs.RegWrite) {
		ID_EX_latch[0].rt_v = M_WB_latch[1].LMD;
	}
	else {
		ID_EX_latch[0].rt_v = regs[ID_EX_latch[0].rt];
	}
}

void SrcTarget() {
	if (ID_EX_latch[0].cs.RegDest == 1) {
		ID_EX_latch[0].tReg = ID_EX_latch[0].rd;
	}
	else if (ID_EX_latch[0].cs.RegDest == 0) {
		ID_EX_latch[0].tReg = ID_EX_latch[0].rt;
	}

	if (ID_EX_latch[0].opcode == jal) {
		ID_EX_latch[0].tReg = 31;
	}
}

void DSignal() {
	//RegWrite
	if (ID_EX_latch[0].opcode != sw && ID_EX_latch[0].opcode != beq
		&& ID_EX_latch[0].opcode != bne&& ID_EX_latch[0].opcode != j
		&& !(ID_EX_latch[0].opcode == 0 && ID_EX_latch[0].func == jr)) {
		ID_EX_latch[0].cs.RegWrite = 1;
	}
	else {
		ID_EX_latch[0].cs.RegWrite = 0;
	}

	//RegDest
	if (ID_EX_latch[0].opcode == 0) {
		ID_EX_latch[0].cs.RegDest = 1;
	}
	else {
		ID_EX_latch[0].cs.RegDest = 0;

	}

	//MemRead
	if (ID_EX_latch[0].opcode == lw) {
		ID_EX_latch[0].cs.MemRead = 1;
	}
	else {
		ID_EX_latch[0].cs.MemRead = 0;
	}

	//MemWrite
	if (ID_EX_latch[0].opcode == sw) {
		ID_EX_latch[0].cs.MemWrite = 1;
	}
	else {
		ID_EX_latch[0].cs.MemWrite = 0;
	}

	//Branch
	if (ID_EX_latch[0].opcode == beq || ID_EX_latch[0].opcode == bne) {
		ID_EX_latch[0].cs.Branch = 1;
	}
	else {
		ID_EX_latch[0].cs.Branch = 0;
	}

	//ALUSrc
	if (ID_EX_latch[0].opcode != 0 && ID_EX_latch[0].opcode != beq
		&& ID_EX_latch[0].opcode != bne) {
		ID_EX_latch[0].cs.ALUSrc = 1;
	}
	else {
		ID_EX_latch[0].cs.ALUSrc = 0;
	}
}

void updateLatch() {

	IF_ID_latch[0] = IF_ID_;
	ID_EX_latch[0] = ID_EX_;
	EX_M_latch[0] = EX_M_;
	M_WB_latch[0] = M_WB_;
}

struct CacheLine {
	int data; //Data store
	unsigned sca, valid, dirty;
	unsigned tag, idx, offset;
};

struct CacheLine Cache[128]; //8KB
struct CacheLine Cache_;


int ReadMem(int addr) {
	int returnResult = 0;

	Cache_.idx = (addr >> 2) & 0x3ff;

	if (Cache[Cache_.idx].valid == 1) {
		if (Cache[Cache_.idx].tag == addr >> 12) {
			printf("# ReadMem [Hit]                      #\n");
			returnResult = Cache[Cache_.idx].data;
			hit_n++;
		}
		else {
			printf("# ReadMem [Miss]: Need to replace    #\n");
			if (Cache[Cache_.idx].dirty == 1) {
				int beforeAddr = (Cache[Cache_.idx].tag << 12) + (Cache_.idx << 2) + Cache[Cache_.idx].offset;
				memory[beforeAddr / 4] = Cache[Cache_.idx].data;
				mem_access++;
				Cache[Cache_.idx].dirty = 0;
			}
			Cache[Cache_.idx].tag = addr >> 12;
			Cache[Cache_.idx].offset = addr & 0x2;
			Cache[Cache_.idx].data = memory[addr / 4];
			Cache[Cache_.idx].dirty = 1;

			returnResult = Cache[Cache_.idx].data;
			miss_n++;
		}
	}
	else {
		printf("# ReadMem [Miss]: Block is empty     #\n");
		Cache[Cache_.idx].tag = addr >> 12;
		Cache[Cache_.idx].offset = addr & 0x2;
		Cache[Cache_.idx].data = memory[addr / 4];
		Cache[Cache_.idx].valid = 1;
		mem_access++;

		returnResult = Cache[Cache_.idx].data;
		miss_n++;
	}
	printf("# index: 0x%08x                  #\n", Cache_.idx);
	printf("# tag: 0x%08x                    #\n", Cache[Cache_.idx].tag);
	printf("# offset: 0x%08x                 #\n", Cache[Cache_.idx].offset);
	printf("# data: 0x%08x                   #\n", Cache[Cache_.idx].data);
	printf("######################################\n");
	printf("\n");

	return returnResult;
}

void WriteMem(int addr, int value) {
	Cache_.idx = (addr >> 2) & 0x3ff;

	printf("\n");
	printf("##############StoreWord###############\n");
	if (Cache[Cache_.idx].valid == 1) {
		if (Cache[Cache_.idx].dirty == 1) {
			printf("# WriteMem : Update M & Replace C     #\n");
			int beforeAddr = (Cache[Cache_.idx].tag << 12) + (Cache_.idx << 2) + Cache[Cache_.idx].offset;
			memory[beforeAddr / 4] = Cache[Cache_.idx].data;
			printf("# C(0x%08x) -> M[0x%08x]    #\n", Cache[Cache_.idx].data, beforeAddr / 4);
			mem_access++;
			Cache[Cache_.idx].dirty = 0;
		}
		else {
			printf("# WriteMem : Replace Cache           #\n");
			Cache[Cache_.idx].tag = addr >> 12;
			Cache[Cache_.idx].offset = addr & 0x2;
			Cache[Cache_.idx].data = value;
			Cache[Cache_.idx].dirty = 1;
		}
	}
	else {
		printf("# WriteMem : Replace Cache           #\n");
		Cache[Cache_.idx].tag = addr >> 12;
		Cache[Cache_.idx].offset = addr & 0x2;
		Cache[Cache_.idx].data = value;
		Cache[Cache_.idx].dirty = 1;
	}

	printf("# index: 0x%08x                  #\n", Cache_.idx);
	printf("# tag: 0x%08x                    #\n", Cache[Cache_.idx].tag);
	printf("# offset: 0x%08x                 #\n", Cache[Cache_.idx].offset);
	printf("# data: 0x%08x                   #\n", Cache[Cache_.idx].data);
	printf("######################################\n");
	printf("\n");
}

void fetch() {
	printf("\n");
	printf("################fetch#################\n");
	IF_ID_latch[0].inst = ReadMem(pc);
	IF_ID_latch[0].pc_4 = pc + 4;

	if (pc == 0xffffffff) {
		IF_ID_latch[0].valid = 0;
		finish++;
		return;
	}

	printf("index : [0x%08x] pc: [0x%08x]\n", exe, pc);
	printf("Instruction: 0x%08x \n", IF_ID_latch[0].inst);

	IF_ID_latch[0].valid = 1;

	pc = pc + 4;
	memcpy(&IF_ID_latch[1], &IF_ID_latch[0], sizeof(struct IF_ID_latch));
}

void decode() {
	int ra;

	if (IF_ID_latch[1].valid == 0) {
		ID_EX_latch[0].valid = 0;
		return;
	}

	if (IF_ID_latch[1].inst == 0x00000000) {
		printf("STEP 2  :  Nope\n");
		nope_exe++;
	}
	else {
		ID_EX_latch[0].opcode = (IF_ID_latch[1].inst >> 26) & 0x3f;
		if (ID_EX_latch[0].opcode == 0) {
			num_Rtype++;
			ID_EX_latch[0].rs = (IF_ID_latch[1].inst >> 21) & 0x1f;
			ID_EX_latch[0].rt = (IF_ID_latch[1].inst >> 16) & 0x1f;
			ID_EX_latch[0].rd = (IF_ID_latch[1].inst >> 11) & 0x1f;
			ID_EX_latch[0].shamt = (IF_ID_latch[1].inst >> 6) & 0x1f;
			ID_EX_latch[0].func = IF_ID_latch[1].inst & 0x3f;
		}
		else if (ID_EX_latch[0].opcode == j || ID_EX_latch[0].opcode == jal) {
			num_Jtype++;
			ID_EX_latch[0].Jtarget = ((IF_ID_latch[1].inst & 0x3ffffff) << 2) | (pc & 0xf0000000);
		}
		else {
			num_Itype++;
			ID_EX_latch[0].rs = (IF_ID_latch[1].inst >> 21) & 0x1f;
			ID_EX_latch[0].rt = (IF_ID_latch[1].inst >> 16) & 0x1f;
			ID_EX_latch[0].imm = IF_ID_latch[1].inst & 0x0000ffff;
			ID_EX_latch[0].simm = ((ID_EX_latch[0].imm >> 15) == 1) ? (ID_EX_latch[0].imm | 0xffff0000) : ID_EX_latch[0].imm;
			ID_EX_latch[0].Btarget = ID_EX_latch[1].pc_4 + (ID_EX_latch[0].simm << 2);
		}

		switch (ID_EX_latch[0].opcode) {
			//R-type
		case 0:
			printf("R-type, opcode = [%d] rs = [%d] rt = [%d] rd[%d] func = [%d]\n", ID_EX_latch[0].opcode, ID_EX_latch[0].rs, ID_EX_latch[0].rt, ID_EX_latch[0].rd, ID_EX_latch[0].func);
			break;
		case j:
			printf("J-type[J], opcode = [%d] jump address = [%d]\n", ID_EX_latch[0].opcode, ID_EX_latch[0].Jtarget);
			pc = ID_EX_latch[0].Jtarget;
			IF_ID_latch[0].valid = 0;
			printf("PC = [0x%08x]\n", pc);

			break;
		case jal:
			printf("J-type[JAL], opcode = [%d] jump address = [%d], next inst is stored in ra\n", ID_EX_latch[0].opcode, ID_EX_latch[0].Jtarget);
			pc = ID_EX_latch[0].Jtarget;
			IF_ID_latch[0].valid = 0;
			printf("PC = [0x%08x]\n", pc);
			break;
		default:
			printf("I-type, opcode = [%d] rs = [%d] rt = [%d] simm = [0x%08x](%d)\n", ID_EX_latch[0].opcode, ID_EX_latch[0].rs, ID_EX_latch[0].rt, ID_EX_latch[0].simm, ID_EX_latch[0].simm);
			break;
		}
	}

	DSignal(); //Control Signal 값 설정
	SrcTarget();//target Register 설정
	mux(); //data dependency

		   //func가 jr일 때 pc 업데이트 및 래치 unvalid
	if (ID_EX_latch[0].opcode == 0 && (ID_EX_latch[0].func == jr || ID_EX_latch[0].func == jalr)) {
		if (M_WB_latch[1].tReg == ID_EX_latch[0].rs) {
			regs[ID_EX_latch[0].rs] = M_WB_latch[1].LMD;
		}
		pc = regs[ID_EX_latch[0].rs];
		IF_ID_latch[0].valid = 0;
	}

	ID_EX_latch[0].pc_4 = IF_ID_latch[1].pc_4;
	ID_EX_latch[0].valid = 1;

	memcpy(&ID_EX_latch[1], &ID_EX_latch[0], sizeof(struct ID_EX_latch));
}

void execute() {
	int operandA;
	int operandB;

	if (ID_EX_latch[1].valid == 0) {
		EX_M_latch[0].valid = 0;
		return;
	}

	if (ID_EX_latch[1].opcode != j && ID_EX_latch[1].opcode != jal && ID_EX_latch[1].func != jr) {
		//operandA = rs_v = value 1을 결정하는 조건문
		if (EX_M_latch[1].tReg == ID_EX_latch[1].rs && EX_M_latch[1].cs.RegWrite) {
			operandA = EX_M_latch[1].ALUResult;
		}
		else if (M_WB_latch[1].tReg == ID_EX_latch[1].rs && M_WB_latch[1].cs.RegWrite) {
			operandA = M_WB_latch[1].LMD;
		}
		else {
			operandA = ID_EX_latch[1].rs_v;
		}

		//operandB = rt_v = value 2를 결정하는 조건문
		if (EX_M_latch[1].tReg == ID_EX_latch[1].rt && EX_M_latch[1].cs.RegWrite) {
			operandB = EX_M_latch[1].ALUResult;
		}
		else if (M_WB_latch[1].tReg == ID_EX_latch[1].rt && M_WB_latch[1].cs.RegWrite) {
			operandB = M_WB_latch[1].LMD;
		}
		else {
			operandB = ID_EX_latch[1].rt_v;
		}

		EX_M_latch[0].rt_v = operandB;

		if (ID_EX_latch[1].cs.ALUSrc == 1) {
			operandB = ID_EX_latch[1].simm;
		}
	}

	switch (ID_EX_latch[1].opcode) {
		EX_M_latch[0].ALUResult = 0;
		//R-type
	case 0:
		switch (ID_EX_latch[1].func) {
		case add:
			printf("ADD regs[%d](rd) <- regs[%d](rs) + regs[%d](rt)\n", ID_EX_latch[1].rd, ID_EX_latch[1].rs, ID_EX_latch[1].rt);
			EX_M_latch[0].ALUResult = operandA + operandB;
			break;

		case addu:
			printf("ADDU regs[%d](rd) <- regs[%d](rs) + regs[%d](rt)\n", ID_EX_latch[1].rd, ID_EX_latch[1].rs, ID_EX_latch[1].rt);
			EX_M_latch[0].ALUResult = operandA + operandB;
			break;

		case and:
			printf("AND regs[%d](rd) <- regs[%d](rs) + regs[%d](rt)\n", ID_EX_latch[1].rd, ID_EX_latch[1].rs, ID_EX_latch[1].rt);
			EX_M_latch[0].ALUResult = operandA & operandB;
			break;

		case nor:
			printf("NOR regs[%d](rd) <- ~ ( regs[%d](rs) | regs[%d](rt) )\n", ID_EX_latch[1].rd, ID_EX_latch[1].rs, ID_EX_latch[1].rt);
			EX_M_latch[0].ALUResult = ~(operandA | operandB);
			break;

		case or :
			printf("OR regs[%d](rd) <- regs[%d](rs) | regs[%d](rt)\n", ID_EX_latch[1].rd, ID_EX_latch[1].rs, ID_EX_latch[1].rt);
			EX_M_latch[0].ALUResult = operandA | operandB;
			break;

		case slt:
			if (operandA < operandB) {
				printf("SLT regs[%d](rs) < regs[%d](rt)\n", ID_EX_latch[1].rs, ID_EX_latch[1].rt);
				EX_M_latch[0].ALUResult = 1;
			}
			else {
				printf("SLT regs[%d](rs) > regs[%d](rt)\n", ID_EX_latch[1].rs, ID_EX_latch[1].rt);
				EX_M_latch[0].ALUResult = 0;
			}
			break;

		case sltu:
			if (operandA < operandB) {
				printf("SLTU regs[%d](rs) < regs[%d](rt)\n", ID_EX_latch[1].rs, ID_EX_latch[1].rt);
				EX_M_latch[0].ALUResult = 1;
			}
			else {
				printf("SLTU regs[%d](rs) > regs[%d](rt)\n", ID_EX_latch[1].rs, ID_EX_latch[1].rt);
				EX_M_latch[0].ALUResult = 0;
			}
			break;

		case sll:
			printf("SLL regs[%d](rd) <- regs[%d](rt) << shamt[%d]\n", ID_EX_latch[1].rd, ID_EX_latch[1].rt, ID_EX_latch[1].shamt);
			EX_M_latch[0].ALUResult = operandB << ID_EX_latch[1].shamt;
			break;

		case srl:
			printf("SRL regs[%d](rd) <- regs[%d](rt) >> shamt[%d]\n", ID_EX_latch[1].rd, ID_EX_latch[1].rt, ID_EX_latch[1].shamt);
			EX_M_latch[0].ALUResult = operandB >> ID_EX_latch[1].shamt;
			break;

		case sub:
			printf("SUB regs[%d](rd) <- regs[%d](rt) - regs[%d](rt)\n", ID_EX_latch[1].rd, ID_EX_latch[1].rs, ID_EX_latch[1].rt);
			EX_M_latch[0].ALUResult = operandA - operandB;
			break;

		case subu:
			printf("SUBU regs[%d](rd) <- regs[%d](rt) - regs[%d](rt)\n", ID_EX_latch[1].rd, ID_EX_latch[1].rs, ID_EX_latch[1].rt);
			EX_M_latch[0].ALUResult = operandA - operandB;
			break;

		}
		break;

		//I-type
	case addi:
		printf("ADDI regs[%d](rt) <- regs[%d](rs) + simm[0x%08x]\n", ID_EX_latch[1].rt, ID_EX_latch[1].rs, ID_EX_latch[1].simm);
		EX_M_latch[0].ALUResult = operandA + ID_EX_latch[1].simm;
		break;

	case addiu:
		printf("ADDIU regs[%d](rt) <- regs[%d](rs) + simm[0x%08x]\n", ID_EX_latch[1].rt, ID_EX_latch[1].rs, ID_EX_latch[1].simm);
		EX_M_latch[0].ALUResult = operandA + ID_EX_latch[1].simm;
		break;

	case andi:
		printf("ANDI regs[%d](rt) <- regs[%d](rs) & imm[0x%08x]\n", ID_EX_latch[1].rt, ID_EX_latch[1].rs, ID_EX_latch[1].imm);
		EX_M_latch[0].ALUResult = operandA & ID_EX_latch[1].imm;
		break;

	case ori:
		printf("ORI regs[%d](rt) <- regs[%d](rs) | imm[0x%08x]\n", ID_EX_latch[1].rt, ID_EX_latch[1].rs, ID_EX_latch[1].imm);
		EX_M_latch[0].ALUResult = operandA | ID_EX_latch[1].imm;
		break;

	case lui:
		EX_M_latch[0].ALUResult = ID_EX_latch[1].imm << 16;
		printf("LUI imm[0x%08x] << 16\n", ID_EX_latch[1].imm);
		break;

	case slti:
		if (operandA < ID_EX_latch[1].simm) {
			printf("SLTI regs[%d](rs) < simm[0x%08x]\n", ID_EX_latch[1].rs, ID_EX_latch[1].simm);
			EX_M_latch[0].ALUResult = 1;
		}
		else {
			printf("SLTI regs[%d](rs) > simm[0x%08x]\n", ID_EX_latch[1].rs, ID_EX_latch[1].simm);
			EX_M_latch[0].ALUResult = 0;
		}
		break;

	case sltiu:
		if (operandA < ID_EX_latch[1].simm) {
			printf("SLTIU regs[%d](rs) < simm[0x%08x]\n", ID_EX_latch[1].rs, ID_EX_latch[1].simm);
			EX_M_latch[0].ALUResult = 1;
		}
		else {
			printf("SLTIU regs[%d](rs) > simm[0x%08x]\n", ID_EX_latch[1].rs, ID_EX_latch[1].simm);
			EX_M_latch[0].ALUResult = 0;
		}
		break;

	case beq:
		if (operandA == operandB) {
			EX_M_latch[0].bcond = 1;
			printf("BEQ rs = rt\n");
		}
		else {
			EX_M_latch[0].bcond = 0;
			printf("BEQ rs != rt\n");
		}
		break;

	case bne:
		if (operandA != operandB) {
			EX_M_latch[0].bcond = 1;
			printf("BNE rs != rt\n");
		}
		else {
			EX_M_latch[0].bcond = 0;
			printf("BNE rs = rt\n");
		}
		break;

	case lw:
		printf("LW regs[%d](rt) <-  Mem[simm[0x%08x] + regs[%d](rs)]\n", ID_EX_latch[1].rt, ID_EX_latch[1].simm, ID_EX_latch[1].rs);
		EX_M_latch[0].ALUResult = ID_EX_latch[1].simm + operandA;
		break;

	case sw:
		printf("SW Mem[simm[0x%08x] + regs[%d](rs)] <- regs[%d](rt)\n", ID_EX_latch[1].simm, ID_EX_latch[1].rs, ID_EX_latch[1].rt);
		EX_M_latch[0].ALUResult = ID_EX_latch[1].simm + operandA;
		break;
	default:
		EX_M_latch[0].ALUResult = 0;
	}

	if (EX_M_latch[0].bcond == 1 && ID_EX_latch[1].cs.Branch == 1) {
		ID_EX_latch[0].valid = 0;
		IF_ID_latch[0].valid = 0;
		pc = ID_EX_latch[1].Btarget;
		printf("PC = [0x%08x] \n", pc);
	}

	if (ID_EX_latch[1].opcode == jal) {
		EX_M_latch[0].ALUResult = ID_EX_latch[1].pc_4 + 4;
	}

	EX_M_latch[0].cs = ID_EX_latch[1].cs;
	EX_M_latch[0].tReg = ID_EX_latch[1].tReg;

	EX_M_latch[0].valid = 1;

	memcpy(&EX_M_latch[1], &EX_M_latch[0], sizeof(struct EX_M_latch));
}

void memory_access() {
	//memory access
	if (EX_M_latch[1].valid == 0) {
		M_WB_latch[0].valid = 0;
		return;
	}

	if (EX_M_latch[1].cs.MemRead == 1) {
		printf("\n");
		printf("##############Load Word###############\n");
		M_WB_latch[0].LMD = ReadMem(EX_M_latch[1].ALUResult);
		printf("[LW] Access memory[0x%08x] = 0x%08x and Read Data\n", EX_M_latch[1].ALUResult / 4, M_WB_latch[0].LMD);
	}

	if (EX_M_latch[1].cs.MemWrite != 1 && EX_M_latch[1].cs.MemRead != 1) {
		M_WB_latch[0].LMD = EX_M_latch[1].ALUResult;
		printf("No Memory Access \nData : 0x%08x \n", M_WB_latch[0].LMD);
	}

	if (EX_M_latch[1].cs.MemWrite == 1) {
		WriteMem(EX_M_latch[1].ALUResult, EX_M_latch[1].rt_v);
		printf("[SW] Access memory[0x%08x] and Write Data 0x%08x(rt)\n", (EX_M_latch[1].ALUResult / 4), memory[EX_M_latch[1].ALUResult / 4]);
	}
	M_WB_latch[0].cs = EX_M_latch[1].cs;
	M_WB_latch[0].tReg = EX_M_latch[1].tReg;

	M_WB_latch[0].valid = 1;
	memcpy(&M_WB_latch[1], &M_WB_latch[0], sizeof(struct M_WB_latch));
}

void write_back() {
	if (M_WB_latch[1].valid == 0) {
		return;
	}

	//writeback
	if (M_WB_latch[1].cs.RegWrite == 1) {
		regs[M_WB_latch[1].tReg] = M_WB_latch[1].LMD;
	}

	printf("regs[%d] = 0x%08x \n", M_WB_latch[1].tReg, M_WB_latch[1].LMD);
}

int main(int argc, char *argv[]) {

	//handling input args
	char *input_file = "C:\\Users\\he308\\OneDrive\\바탕 화면\\JAVA\\workspace\\Mips_single_cycle\\input4.bin";
	FILE *fp;
	int ret;
	int fin;
	int index = 0;
	int inst;

	//load the program
	if (argc == 2) {
		input_file = argv[1];
	}

	//open the file
	fp = fopen(input_file, "rb");
	//read from the file
	while (1) {
		unsigned char in[4];
		int *ptr;
		fin = 0;
		for (int i = 0; i < 4; i++) {
			ret = fread(&in[i], 1, 1, fp);
			if (ret == 0) {
				fin = 1;
			}
		}
		if (fin == 1) break;

		int tmp;
		tmp = in[0];
		in[0] = in[3];
		in[3] = tmp;

		tmp = in[1];
		in[1] = in[2];
		in[2] = tmp;

		ptr = &in[0];


		inst = *ptr;

		memory[index / 4] = inst;
		printf("[0x%08x] 0x%08x\n", index, memory[index / 4]);
		index += 4;
	}
	fclose(fp);

	//initialize architectural states
	regs[31] = 0xffffffff;
	regs[29] = 0x100000; // initial sp points to the end of memory

						 //begin execution loop
	while (1) {
		cycle++;

		printf("--------------------------------------------------------------------------------\n");
		printf("CYCLE --> %d\n", cycle);
		printf("STEP 1  :  Fetch\n");
		fetch();

		printf("STEP 2  :  Decode\n");
		decode();

		printf("STEP 3  :  Execute\n");
		execute();

		printf("STEP 4  :  Memory Access\n");
		memory_access();

		printf("STEP 5  :  Write Back\n");
		write_back();
		updateLatch();

		printf("--------------------------------------------------------------------------------\n");
		exe += 4;
		//end of execution

		if (finish == 3) {
			break;
		}
		else {
			continue;
		}

	}
	hit_ratio = hit_n / mem_access;
	//print out result/states
	printf("Result is %d, Stack Pointer sp = [0x%08x], Frame Pointer fp = [0x%08x]\n", regs[2], regs[29], regs[30]);
	printf("Total cycle : %d\nR-type instruction : %d\nI-type instruction : %d\nJ-type instruction : %d\n", cycle, num_Rtype, num_Itype, num_Jtype);
	printf("Nope Instruction : %d\n", nope_exe);
	printf("Memory access time : %d, Hit : %d, Miss : %d\n", mem_access, hit_n, miss_n);
	printf("Hit Ratio : %1f\n", hit_ratio);
	return 0;
}
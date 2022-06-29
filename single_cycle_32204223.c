#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>

#define add 0x20
#define addi 0x8
#define addiu 0x9
#define addu 0x21
#define and 0x24
#define andi 0xc
#define beq 0x4
#define bne 0x5
#define j 0x2
#define jal 0x3
#define jr 0x08
#define jalr 0x09
#define lui 0xf
#define lw 0x23
#define nor 0x27
#define or 0x25
#define ori 0xd
#define slt 0x2a
#define slti 0xa
#define sltiu 0xb
#define sltu 0x2b
#define sll 0x00
#define srl 0x02
#define sw 0x2b
#define sub 0x22
#define subu 0x23


//global vars
//architectural states
int memory[0x400000]; // 16MB memory
int pc = 0;
int regs[32] = { 0 };
int cycle = 1;

struct inst_ {
	int inst;
	int opcode;
	int rs;
	int rt;
	int rd;
	int shamt;
	int func;
	int imm;
	int simm;    //sign extend imm
	int btarget; //branch addr
	int jtarget; //jump addr
	int address;
	int regdes, regwrite, alusrc, memtoreg, memread, memwrite, pcsrc1, pcsrc2;
};

struct inst_ instr;

int main(int argc, char *argv[]) {

	//handling input args
	char *input_file = "C:\\Users\\he308\\OneDrive\\바탕 화면\\JAVA\\workspace\\Mips_single_cycle\\input4.bin";
	FILE *fp;
	int ret;
	int fin;
	int index = 0;
	int exe = 0;
	int nope_exe = 0;
	int inst;
	int num_Itype = 0;
	int num_Rtype = 0;
	int num_Jtype = 0;

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
		int value;
		int access_value;

		if (pc == 0xffffffff)break;
		//	fetch
		memset(&instr, 0, sizeof(instr));
		inst = memory[pc / 4];
		printf("--------------------------------------------------------------------------------\n");
		printf("CYCLE --> %d\n", cycle);
		printf("STEP 1  :  Fetch\n");
		printf("index : [0x%08x] pc: [0x%08x]\n", exe, pc);
		printf("Instruction: 0x%08x \n", inst);

		//	decode
		instr.inst = inst;
		if (instr.inst == 0x00000000) {
			printf("STEP 2  :  Nope\n");
			pc += 4;
			nope_exe++;
		}
		else {
			instr.opcode = (instr.inst >> 26) & 0x3f;
			if (instr.opcode == 0) {
				instr.rs = (instr.inst >> 21) & 0x1f;
				instr.rt = (instr.inst >> 16) & 0x1f;
				instr.rd = (instr.inst >> 11) & 0x1f;
				instr.shamt = (instr.inst >> 6) & 0x1f;
				instr.func = instr.inst & 0x3f;
			}
			else if(instr.opcode == j || instr.opcode == jal) {
				instr.address = instr.inst & 0x3ffffff;
				instr.jtarget = (instr.address << 2) | (pc & 0xf0000000);
			}
			else {
				instr.rs = (instr.inst >> 21) & 0x1f;
				instr.rt = (instr.inst >> 16) & 0x1f;
				instr.imm = instr.inst & 0x0000ffff;
				instr.simm = ((instr.imm >> 15) == 1) ? (instr.imm | 0xffff0000) : instr.imm;
				instr.btarget = instr.simm << 2;
			}
			
			printf("STEP 2  :  Decode\n");
			switch (instr.opcode) {
				//R-type
			case 0:
				printf("R-type, opcode = [%d] rs = [%d] rt = [%d] rd[%d] func = [%d]\n", instr.opcode, instr.rs, instr.rt, instr.rd, instr.func);
				break;
			case j:
				printf("J-type[J], opcode = [%d] address = [%d]\n", instr.opcode, instr.address);
				break;
			case jal:
				printf("J-type[JAL], opcode = [%d] address = [%d]\n", instr.opcode, instr.address);
				break;
			default:
				printf("I-type, opcode = [%d] rs = [%d] rt = [%d] simm = [0x%08x](%d)\n", instr.opcode, instr.rs, instr.rt, instr.simm, instr.simm);
				break;
			}
			

			//control signal
			//RegDest
			if (instr.opcode == 0) instr.regdes = 1;
			else instr.regdes = 0;
			//ALUSrc
			if ((instr.opcode != 0) && (instr.opcode != beq) && (instr.opcode != bne)) instr.alusrc = 1;
			else instr.alusrc = 0;

			//RegWrite
			if ((instr.opcode != sw) && (instr.opcode != beq) && (instr.opcode != bne) && (instr.opcode != j) && (instr.opcode != jr)) instr.regwrite = 1;
			else instr.regwrite = 0;

			//MemtoReg, MemRead
			if (instr.opcode == lw) instr.memtoreg, instr.memread = 1;
			else instr.memtoreg, instr.memread = 0;
			//MemWrite
			if (instr.opcode == sw) instr.memwrite = 1;
			else instr.memwrite = 0;
			//PCSrc1
			if ((instr.opcode == j) || (instr.opcode == jal)) instr.pcsrc1 = 1;
			else instr.pcsrc1 = 0;
			//PCRSrc2
			if ((instr.opcode == bne) || (instr.opcode == beq)) instr.pcsrc2 = 1;
			else instr.pcsrc2 = 0;
			
			printf("STEP 3  :  Execute\n");

			switch (instr.opcode) {
			//R-type
			case 0:
				switch (instr.func) {
				case add:
					printf("ADD regs[%d](rd) <- regs[%d](rs) + regs[%d](rt)\n", instr.rd, instr.rs, instr.rt);
					value = regs[instr.rs] + regs[instr.rt];
					break;

				case addu:
					printf("ADDU regs[%d](rd) <- regs[%d](rs) + regs[%d](rt)\n", instr.rd, instr.rs, instr.rt);
					value = regs[instr.rs] + regs[instr.rt];
					break;

				case and:
					printf("AND regs[%d](rd) <- regs[%d](rs) + regs[%d](rt)\n", instr.rd, instr.rs, instr.rt);
					value = regs[instr.rs] & regs[instr.rt];
					break;

				case jr:
					value = regs[instr.rs];
					printf("JR PC = 0x%08x(rs)\n", value);
					break;

				case jalr:
					value = regs[instr.rs];
					printf("JALR PC = 0x%08x(rs), next inst is stored in ra\n", value);
					break;

				case nor:
					printf("NOR regs[%d](rd) <- ~ ( regs[%d](rs) | regs[%d](rt) )\n", instr.rd, instr.rs, instr.rt);
					value = ~(regs[instr.rs] | regs[instr.rt]);
					break;

				case or:
					printf("OR regs[%d](rd) <- regs[%d](rs) | regs[%d](rt)\n", instr.rd, instr.rs, instr.rt);
					value = regs[instr.rs] | regs[instr.rt];
					break;

				case slt:
					if (regs[instr.rs] < regs[instr.rt]) {
						printf("SLT regs[%d](rs) < regs[%d](rt)\n", instr.rs, instr.rt);
						value = 1;
					}
					else {
						printf("SLT regs[%d](rs) > regs[%d](rt)\n", instr.rs, instr.rt);
						value = 0;
					}
					break;

				case sltu:
					if (regs[instr.rs] < regs[instr.rt]) {
						printf("SLTU regs[%d](rs) < regs[%d](rt)\n", instr.rs, instr.rt);
						value = 1;
					}
					else {
						printf("SLTU regs[%d](rs) > regs[%d](rt)\n", instr.rs, instr.rt);
						value = 0;
					}
					break;

				case sll:
					printf("SLL regs[%d](rd) <- regs[%d](rt) << shamt[%d]\n", instr.rd, instr.rt, instr.shamt);
					value = regs[instr.rt] << instr.shamt;
					break;

				case srl:
					printf("SRL regs[%d](rd) <- regs[%d](rt) >> shamt[%d]\n", instr.rd, instr.rt, instr.shamt);
					value = regs[instr.rt] >> instr.shamt;
					break;

				case sub:
					printf("SUB regs[%d](rd) <- regs[%d](rt) - regs[%d](rt)\n", instr.rd, instr.rs, instr.rt);
					value = regs[instr.rs] - regs[instr.rt];
					break;

				case subu:
					printf("SUBU regs[%d](rd) <- regs[%d](rt) - regs[%d](rt)\n", instr.rd, instr.rs, instr.rt);
					value = regs[instr.rs] - regs[instr.rt];
					break;

				}
				num_Rtype++;
				break;
			//J-type
			case j:
				printf("J, target address is [0x%08x]\n", instr.jtarget);
				value = instr.jtarget;
				num_Jtype++;
				break;

			case jal:
				printf("JAL, target address is [0x%08x], next inst is stored in ra\n", instr.jtarget);
				value = instr.jtarget;
				num_Jtype++;
				break;

			//I-type
			case addi:
				printf("ADDI regs[%d](rt) <- regs[%d](rs) + simm[0x%08x]\n", instr.rt, instr.rs, instr.simm);
				value = regs[instr.rs] + instr.simm;
				num_Itype++;
				break;

			case addiu:
				printf("ADDIU regs[%d](rt) <- regs[%d](rs) + simm[0x%08x]\n", instr.rt, instr.rs, instr.simm);
				value = regs[instr.rs] + instr.simm;
				num_Itype++;
				break;

			case andi:
				printf("ANDI regs[%d](rt) <- regs[%d](rs) & imm[0x%08x]\n", instr.rt, instr.rs, instr.imm);
				value = regs[instr.rs] & instr.imm;
				num_Itype++;
				break;

			case ori:
				printf("ORI regs[%d](rt) <- regs[%d](rs) | imm[0x%08x]\n", instr.rt, instr.rs, instr.imm);
				value = regs[instr.rs] | instr.imm;
				num_Itype++;
				break;

			case lui:
				value = instr.imm << 16;
				printf("LUI imm[0x%08x] << 16\n", instr.imm);
				num_Itype++;
				break;

			case slti:
				if (regs[instr.rs] < instr.simm) {
					printf("SLTI regs[%d](rs) < simm[0x%08x]\n", instr.rs, instr.simm);
					value = 1;
				}
				else {
					printf("SLTI regs[%d](rs) > simm[0x%08x]\n", instr.rs, instr.simm);
					value = 0;
				}
				num_Itype++;
				break;

			case sltiu:
				if (regs[instr.rs] < instr.simm) {
					printf("SLTIU regs[%d](rs) < simm[0x%08x]\n", instr.rs, instr.simm);
					value = 1;
				}
				else {
					printf("SLTIU regs[%d](rs) > simm[0x%08x]\n", instr.rs, instr.simm);
					value = 0;
				}
				num_Itype++;
				break;

			case beq:
				if (regs[instr.rs] == regs[instr.rt]) {
					value = instr.btarget + 4 + pc;
					printf("BEQ rs = rt\n");
				}
				else {
					printf("BEQ rs != rt\n");
				}
				num_Itype++;
				break;

			case bne:
				if (regs[instr.rs] != regs[instr.rt]) {
					value = instr.btarget + 4 + pc;
					printf("BNE rs != rt\n");
				}
				else {
					printf("BNE rs = rt\n");
				}
				num_Itype++;
				break;

			case lw:
				printf("LW regs[%d](rt) <-  Mem[simm[0x%08x] + regs[%d](rs)]\n", instr.rt, instr.simm, instr.rs);
				value = instr.simm + regs[instr.rs];
				num_Itype++;
				break;

			case sw:
				printf("SW Mem[simm[0x%08x] + regs[%d](rs)] <- regs[%d](rt)\n", instr.simm, instr.rs, instr.rt);
				value = instr.simm + regs[instr.rs];
				num_Itype++;
				break;
			}

			//memory access
			switch (instr.opcode) {

			case lw:
				access_value = memory[value / 4];
				printf("STEP 4  :  Memory Access\nlw memory[0x%08x]\n", value / 4);
				break;
			case sw:
				access_value = regs[instr.rt];
				printf("STEP 4  :  Memory Access\nsw memory[0x%08x]\n", value / 4);
				break;
			default:
				printf("STEP 4  :  No Memory Access\n");
				break;

			}

			printf("STEP 5  :  Write Back\n");
			//writeback
			switch (instr.opcode) {
			case 0:
				switch (instr.func) {
				case add:
					regs[instr.rd] = value;
					printf("regs[%d] = 0x%08x regs[%d] = 0x%08x regs[%d] = 0x%08x\n", instr.rs, regs[instr.rs], instr.rt, regs[instr.rs], instr.rd, value);
					pc += 4;
					break;

				case addu:
					regs[instr.rd] = value;
					printf("regs[%d] = 0x%08x regs[%d] = 0x%08x regs[%d] = 0x%08x\n", instr.rs, regs[instr.rs], instr.rt, regs[instr.rs], instr.rd, value);
					pc += 4;
					break;

				case and:
					regs[instr.rd] = value;
					printf("regs[%d] = 0x%08x regs[%d] = 0x%08x regs[%d] = 0x%08x\n", instr.rs, regs[instr.rs], instr.rt, regs[instr.rs], instr.rd, value);
					pc += 4;
					break;

				case jr:
					pc = value;
					printf("PC = 0x%08x(rs)\n", value);
					printf("regs[%d] = 0x%08x regs[%d] = 0x%08x regs[%d] = 0x%08x\n", instr.rs, regs[instr.rs], instr.rt, regs[instr.rs], instr.rd, value);
					break;

				case jalr:
					regs[31] = pc + 4; //pc는 주소 index 정수 pc + 4 -> index + 4와 같음, 레지스터에는 주소값을 저장하기 때문에 그냥 pc + 4대로 저장해도 된다. index가 아니기 때문 
					pc = value;
					printf("PC = 0x%08x(rs), regs[rd] = next inst\n", value);
					printf("regs[%d] = 0x%08x regs[%d] = 0x%08x regs[%d] = 0x%08x\n", instr.rs, regs[instr.rs], instr.rt, regs[instr.rs], instr.rd, value);
					break;

				case nor:
					regs[instr.rd] = value;
					printf("regs[%d] = 0x%08x regs[%d] = 0x%08x regs[%d] = 0x%08x\n", instr.rs, regs[instr.rs], instr.rt, regs[instr.rs], instr.rd, value);
					pc += 4;
					break;

				case or :
					regs[instr.rd] = value;
					printf("regs[%d] = 0x%08x regs[%d] = 0x%08x regs[%d] = 0x%08x\n", instr.rs, regs[instr.rs], instr.rt, regs[instr.rs], instr.rd, value);
					pc += 4;
					break;

				case slt:
					regs[instr.rd] = value;
					printf("regs[%d] = 0x%08x regs[%d] = 0x%08x regs[%d] = 0x%08x\n", instr.rs, regs[instr.rs], instr.rt, regs[instr.rs], instr.rd, value);
					pc += 4;
					break;

				case sltu:
					regs[instr.rd] = value;
					printf("regs[%d] = 0x%08x regs[%d] = 0x%08x regs[%d] = 0x%08x\n", instr.rs, regs[instr.rs], instr.rt, regs[instr.rs], instr.rd, value);
					pc += 4;
					break;

				case sll:
					regs[instr.rd] = value;
					printf("regs[%d] = 0x%08x regs[%d] = 0x%08x regs[%d] = 0x%08x\n", instr.rs, regs[instr.rs], instr.rt, regs[instr.rs], instr.rd, value);
					pc += 4;
					break;

				case srl:
					regs[instr.rd] = value;
					printf("regs[%d] = 0x%08x regs[%d] = 0x%08x regs[%d] = 0x%08x\n", instr.rs, regs[instr.rs], instr.rt, regs[instr.rs], instr.rd, value);
					pc += 4;
					break;

				case sub:
					regs[instr.rd] = value;
					printf("regs[%d] = 0x%08x regs[%d] = 0x%08x regs[%d] = 0x%08x\n", instr.rs, regs[instr.rs], instr.rt, regs[instr.rs], instr.rd, value);
					pc += 4;
					break;

				case subu:
					regs[instr.rd] = value;
					printf("regs[%d] = 0x%08x regs[%d] = 0x%08x regs[%d] = 0x%08x\n", instr.rs, regs[instr.rs], instr.rt, regs[instr.rs], instr.rd, value);
					pc += 4;
					break;

				}
				break;

			//J-type
			case j:
				pc = value;
				printf("PC = [0x%08x]\n", pc);
				printf("regs[%d] = 0x%08x regs[%d] = 0x%08x\n", instr.rs, regs[instr.rs], instr.rt, regs[instr.rt]);
				break;

			case jal:
				regs[31] = pc + 4;
				pc = value;
				printf("PC = [0x%08x], ra is [0x%08x]\n", pc, regs[31]);
				printf("regs[%d] = 0x%08x regs[%d] = 0x%08x\n", instr.rs, regs[instr.rs], instr.rt, regs[instr.rt]);
				break;

			//I-type
			case addi:
				regs[instr.rt] = value;
				printf("regs[%d] = 0x%08x regs[%d] = 0x%08x\n", instr.rs, regs[instr.rs], instr.rt, regs[instr.rt]);
				pc += 4;
				break;

			case addiu:
				regs[instr.rt] = value;
				printf("regs[%d] = 0x%08x regs[%d] = 0x%08x\n", instr.rs, regs[instr.rs], instr.rt, regs[instr.rt]);
				pc += 4;
				break;

			case andi:
				regs[instr.rt] = value;
				printf("regs[%d] = 0x%08x regs[%d] = 0x%08x\n", instr.rs, regs[instr.rs], instr.rt, regs[instr.rt]);
				pc += 4;
				break;

			case ori:
				regs[instr.rt] = value;
				printf("regs[%d] = 0x%08x regs[%d] = 0x%08x\n", instr.rs, regs[instr.rs], instr.rt, regs[instr.rt]);
				pc += 4;
				break;

			case lui:
				regs[instr.rt] = value;
				printf("regs[%d] = 0x%08x regs[%d] = 0x%08x\n", instr.rs, regs[instr.rs], instr.rt, regs[instr.rt]);
				pc += 4;
				break;

			case slti:
				regs[instr.rt] = value;
				printf("regs[%d] = 0x%08x regs[%d] = 0x%08x\n", instr.rs, regs[instr.rs], instr.rt, regs[instr.rt]);
				pc += 4;
				break;

			case sltiu:
				regs[instr.rt] = value;
				printf("regs[%d] = 0x%08x regs[%d] = 0x%08x\n", instr.rs, regs[instr.rs], instr.rt, regs[instr.rt]);
				pc += 4;
				break;

			case beq:
				if (regs[instr.rs] == regs[instr.rt]) {
					pc = value;
					printf("PC = [0x%08x]\n", pc);
				}
				else {
					pc += 4;
					printf("PC = [0x%08x]\n", pc);
				}
				printf("regs[%d] = 0x%08x regs[%d] = 0x%08x\n", instr.rs, regs[instr.rs], instr.rt, regs[instr.rt]);
				break;

			case bne:
				if (regs[instr.rs] != regs[instr.rt]) {
					pc = value;
					printf("PC = [0x%08x]\n", pc);
				}
				else {
					pc += 4;
					printf("PC = [0x%08x]\n", pc);
				}
				printf("regs[%d] = 0x%08x regs[%d] = 0x%08x\n", instr.rs, regs[instr.rs], instr.rt, regs[instr.rt]);
				break;

			case lw:
				regs[instr.rt] = access_value;
				printf("Load Mem[0x%08x] -> regs[%d] = 0x%08x\n", value / 4, instr.rt, regs[instr.rt]);
				pc += 4;
				printf("regs[%d] = 0x%08x regs[%d] = 0x%08x\n", instr.rs, regs[instr.rs], instr.rt, regs[instr.rt]);
				break;

			case sw:
				memory[value / 4] = access_value;
				printf("Store regs[%d] -> Mem[0x%08x] = 0x%08x\n", instr.rt, value / 4, memory[value / 4]);
				pc += 4;
				printf("regs[%d] = 0x%08x regs[%d] = 0x%08x\n", instr.rs, regs[instr.rs], instr.rt, regs[instr.rt]);
				break;

			default:
				pc += 4;
				break;
			}
		}
		printf("--------------------------------------------------------------------------------\n");
		//update-pc
		if (pc == 0xffffffff) break;
		exe += 4;
		cycle++;

		//end of execution
	}
	//print out result/states
	printf("Result is %d, Stack Pointer sp = [0x%08x], Frame Pointer fp = [0x%08x]\n", regs[2], regs[29], regs[30]);
	printf("Total execution : %d\nR-type instruction : %d\nI-type instruction : %d\nJ-type instruction : %d\n", cycle, num_Rtype, num_Itype, num_Jtype);
	printf("Nope Instruction : %d\n", nope_exe);
	return 0;
}
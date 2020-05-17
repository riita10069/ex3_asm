
#if defined INSN_ENUM
#define INSN(i, code, showMem, func) I_##i,
/// example: INSN(AND, 0x0000, 0, AC &= MEM;) ==> I_AND,
#elif defined INSN_CREATE
#define INSN(i, code, showMem, func) insn[I_##i].Set(I_##i, #i, ((code & 0x7000) != 0x7000) ? MEM_INSN : REG_INSN, showMem, OP_##i, code);
/// example: INSN(AND, 0x0000, 0, AC &= MEM;) ==> insn[I_AND].Set(I_AND, "AND", MemInsn, 0, OP_AND, 0x0000);
#elif defined INSN_FUNC_DECLARATION
#define INSN(i, code, showMem, func) static void OP_##i(CPU * cpu);
/// example: INSN(AND, 0x0000, 0, AC &= MEM;) ==> static void OP_AND(CPU * cpu);
#elif defined INSN_FUNC_DEFINITION
#define INSN(i, code, showMem, func) void EX3_InsnSet::OP_##i(CPU * cpu){ func }
/// example: INSN(AND, 0x0000, 0, AC &= MEM;) ==> void MY_InsnSet::OP_AND(CPU * cpu){ AC &= MEM; }
#else
#define INSN(i, code, showMem, func)		///	empty string
#endif

#define S			(cpu->_S)
#define PC			(cpu->_PC)
#define MEM			(cpu->mem->Operand())
#define AR			(cpu->mem->Address())
#define INPR		(cpu->_INPR)
#define OUTR		(cpu->_OUTR)
#define SetFGI(n)	(cpu->_SetFGI(n))
#define SetFGO(n)	(cpu->_SetFGO(n))
#define FGI			(cpu->_GetFGI())
#define FGO			(cpu->_GetFGO())
#define AC				(((EX3_CPU *) cpu)->_AC)
#define E				(((EX3_CPU *) cpu)->_E)
#define R				(((EX3_CPU *) cpu)->_R)
#define IEN				(((EX3_CPU *) cpu)->_IEN)
#define IMSK			(((EX3_CPU *) cpu)->_IMSK)
#define IOT				(((EX3_CPU *) cpu)->_IOT)
#define INPUT_PENDING	(((EX3_CPU *) cpu)->inputPending)

INSN(AND, 0x0000, 0, AC &= MEM;)
INSN(ADD, 0x1000, 0, int tmp = AC + MEM; E = (tmp >> 16) & 1; AC = (tmp & 0xffff);)
INSN(LDA, 0x2000, 0, AC = MEM;)
INSN(STA, 0x3000, 0, MEM = AC;)
INSN(BUN, 0x4000, 0, PC = AR;)
INSN(BSA, 0x5000, 0, MEM = PC; PC = AR + 1;)
INSN(ISZ, 0x6000, 1, MEM ++; if(MEM == 0)	PC = PC + 1;)

INSN(CLA, 0x7800, 0, AC = 0;)
INSN(CLE, 0x7400, 0, E = 0;)
INSN(CMA, 0x7200, 0, AC = ~AC;)
INSN(CME, 0x7100, 0, E = !E;)
INSN(CIR, 0x7080, 0, int tmp = E; E = AC & 1; AC = (AC >> 1) | (tmp << 15);)
INSN(CIL, 0x7040, 0, int tmp = E; E = (AC >> 15) & 1; AC = (AC << 1) | tmp;)
INSN(INC, 0x7020, 0, AC ++;)
INSN(SPA, 0x7010, 0, if((AC & 0x8000) == 0)	PC = PC + 1;)
INSN(SNA, 0x7008, 0, if(AC & 0x8000)		PC = PC + 1;)
INSN(SZA, 0x7004, 0, if(AC == 0)			PC = PC + 1;)
INSN(SZE, 0x7002, 0, if(E == 0)				PC = PC + 1;)
INSN(HLT, 0x7001, 0, S = 0;)

INSN(INP, 0xf800, 0, AC = (AC & ~0xff) | (INPR & 0xff);	SetFGI(0); INPUT_PENDING = 0;)
INSN(OUT, 0xf400, 0, OUTR = AC & 0xff;					SetFGO(0);)
INSN(SKI, 0xf200, 0, if(FGI)	PC = PC + 1; INPUT_PENDING = 1;)
INSN(SKO, 0xf100, 0, if(FGO)	PC = PC + 1;)
INSN(ION, 0xf080, 0, IEN = 1;)
INSN(IOF, 0xf040, 0, IEN = 0;)
INSN(SIO, 0xf020, 0, IOT = 1;)
INSN(PIO, 0xf010, 0, IOT = 0;)
INSN(IMK, 0xf008, 0, IMSK = AC & 0xf;)

#undef S
#undef PC
#undef MEM
#undef AR
#undef INPR
#undef OUTR
#undef FGI
#undef FGO
#undef SetFGI
#undef SetFGO
#undef AC
#undef E
#undef R
#undef IEN
#undef IMSK
#undef IOT

#undef INSN

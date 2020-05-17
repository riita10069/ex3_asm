#if !defined _EX3_ASM_H_INCLUDED_

#define _EX3_ASM_H_INCLUDED_

#include "common_asm.h"

class EX3_Memory : public Memory
{
public:
	EX3_Memory(int sz) : Memory(sz){}
	bool IsIndirectMemoryAccess(Word * w){ return (w->value & 0x8000) != 0;}
	unsigned short GetAddressField(Word * m){ return m->value & 0xfff;}
};

class EX3_CPU;
class EX3_InsnSet : public InsnSet
{
public:
	enum InsnType{ NON_INSN, MEM_INSN, REG_INSN,};
	enum InsnID
	{
		I_ANCHOR = InsnSet::I_TAIL - 1,
#define INSN_ENUM
#include "ex3_asm_def.h"
#undef INSN_ENUM
		I_COUNT
	};

#define INSN_FUNC_DECLARATION
#include "ex3_asm_def.h"
#undef INSN_FUNC_DECLARATION

	EX3_InsnSet(int icount) : InsnSet(icount){
#define INSN_CREATE
#include "ex3_asm_def.h"
#undef INSN_CREATE
	}
};

class EX3_CPU : public CPU
{
public:
	unsigned short _AC, _E, _R, _IEN, _IMSK, _IOT, inputPending;
	EX3_CPU(int mem_size) : CPU("ex3_cpu.log"){
		mem = new EX3_Memory(mem_size);
		isa = new EX3_InsnSet(EX3_InsnSet::I_COUNT);
		dbg = new Debugger(this);
	}
	void Reset(){
		CPU::Reset();
		_AC		= 0;
		_E		= 0;
		_R		= 0;	///	interrupt detection
		_IEN	= 0;	///	interrupt enable
		_IMSK	= 0;	///	interrupt mask (4 bits) : {UART_RX, UART_TX, GP_IN, GP_OUT}
		_IOT	= 0;	///	io select : 1 (SIO), 0 (PIO)
		inputPending = 0;
	}
	virtual int GetSelectedPortID(){ return (_IOT) & 1;}
	virtual bool IsWaitingForInput(){
		bool inputEnabled = (_IMSK & 0x8) || (_IMSK & 0x2) ;
		return IsInputReady() && (_IEN && inputEnabled || inputPending && !intr_pending);}
	void SetFG(unsigned short * fg, int val){
		int mask = (1 << GetSelectedPortID());
		if(mask == 2){ val <<= 1;}
		*fg = (*fg & (~mask)) | val;
	}
	virtual void _SetFGI(int val){ SetFG(&_FGI, val);}
	virtual void _SetFGO(int val){ SetFG(&_FGO, val);}
	virtual int _GetFGI(){ return (_FGI & (1 << GetSelectedPortID())) != 0;}
	virtual int _GetFGO(){ return (_FGO & (1 << GetSelectedPortID())) != 0;}
	void PrintDataMemory(){
		EMIT_MESSAGE_0(PrintSeparator, fplog);
		EMIT_MESSAGE_1(PrintMemory, fplog, CPU::PM_Null);
	}
	void PrintStatus(FILE * fp, bool intr_cycle){
		if(fp){
			if(mem->curCode == 0){ mem->ProbeInsn(_PC);}
			if(intr_cycle)
				fprintf(fp, "[%4d] INTR [%4d:A] + [%4d:I] : PC(%04x), oldPC(%04x), R(%x), IEN(%x), IMSK(%x), FGI/O(%x:%x)\n",
					cycle_count, appl_cycle_count, intr_cycle_count, _PC, _oldPC, _R, _IEN, _IMSK, _FGI, _FGO);
			else if(mem->curCode == 0){ fprintf(fp, " ERROR!!!! curCode = 0... \n");}
			else{
				if(mem->curCode->headComment){ fprintf(fp, "/%s\n", mem->curCode->headComment);}
				if(isa->insn[mem->curCode->insnID].showMemFlag){
					fprintf(fp, "[%4d] S(%d),R(%x),IO(%x:%x:%x|%x:%x|%x:%x),MEM(%04x),E(%x),PC(%03x): ",
						cycle_count, _S, _R, _IEN, _IMSK, _IOT, _FGI, _INPR, _FGO, _OUTR,
						mem->Operand(), _E, _PC);
				}
				else{
					fprintf(fp, "[%4d] S(%d),R(%x),IO(%x:%x:%x|%x:%x|%x:%x), AC(%04x),E(%x),PC(%03x): ",
						cycle_count, _S, _R, _IEN, _IMSK, _IOT, _FGI, _INPR, _FGO, _OUTR,
						_AC, _E, _PC);
				}
				PrintMemoryWord(mem->curCode, fp, _oldPC, CPU::PM_Program | CPU::PM_SkipHeadComment);
			}
			if(mem->errorFlag){ printf("Memory error occurred!!!\n");}
		}
	}
	int Execute(){
		cycle_count ++;
		bool intr_cycle = (_R == 1);
		intr_pending   |= (_R == 1);
		if(intr_cycle){	///	interrupt cycle
			mem->word[0].value = _PC;	///	MEM[0] <- PC
			_PC = 1;					///	PC	<- 1 : 1st instruction of interrupt handler
			_IEN = 0;					///	IEN <- 0 : disable interrupt upon entering interrupt handler
			_R = 0;						///	R	<- 0 : disable interrupt detection flag
		}
		else{	///	instruction cycle
			int IO_ready = (((_FGI & 0x2) != 0) << 3) | (((_FGO & 0x2) != 0) << 2) | (((_FGI & 0x1) != 0) << 1) | (((_FGO & 0x1) != 0));
			if(_IEN && (_IMSK & IO_ready)){ _R = 1;}
			mem->FetchInsn(_PC);
			if(mem->curCode)	{ isa->insn[mem->curCode->insnID].operation(this);}
		}
		if(intr_pending)	{ intr_cycle_count ++;}
		else				{ appl_cycle_count ++;}
		/// 0xc000 : BUN 0 I (indirect jump at addr 0) --> this is a return from interrupt handler
		if(!intr_cycle && mem->curCode && mem->curCode->value == 0xc000){ intr_pending = false; inputPending = false;}
		EMIT_MESSAGE_1(PrintStatus, fplog, intr_cycle);
		_oldPC = _PC;
		return (mem->errorFlag || _S == 0) ? -1 : 0;
	}
#define VERILOG_READ_MEM_SEPARATOR " "
	void PrintMemoryWord(Memory::Word * m, FILE * fp, int addr, int printMode)
	{
		bool verilogFlag = (printMode & CPU::PM_Verilog) != 0;
		bool showProgram = (printMode & CPU::PM_Program) != 0;
		bool showMonitor = (printMode & CPU::PM_Monitor) != 0;
		bool skipHeadComment = (printMode & CPU::PM_SkipHeadComment) != 0;
		if(fp){
			InsnSet::Insn * insn = &isa->insn[m->insnID];
			Label::Element * lb = label.GetLabel(addr);
			if(lb == 0 && insn->type == EX3_InsnSet::NON_INSN && m->status == 0 && m->value == 0)	{ return;}
			if(showMonitor){ if((m->status & Memory::MS_Monitor) == 0)								{ return;}}
			else if(insn->type != EX3_InsnSet::NON_INSN && !showProgram)							{ return;}
			if(!skipHeadComment && m->headComment){ fprintf(fp, "/%s\n", m->headComment);}
			if(verilogFlag){
				if(showProgram)			{ fprintf(fp, "@%03x%s%04x\t///", addr, VERILOG_READ_MEM_SEPARATOR, m->value);}
				else if(showMonitor)	{ fprintf(fp, "%1x%03x%04x\t///\t", (insn->type == EX3_InsnSet::NON_INSN), addr, m->value);}
				else					{ fprintf(fp, "%1x%03x%04x\t///\t", 0, addr, m->value);}
			}
			fprintf(fp, "%*s%s", label.maxLabelLength, (lb) ? lb->name : "", (lb) ? ": " : "  ");
			if(insn->type == EX3_InsnSet::MEM_INSN){
				int addrValue = mem->GetAddressField(m);
				Label::Element * target_lb = label.GetLabel(addrValue);
				fprintf(fp, "%04x [%04x]: %s %03x %s (%*s)", addr, m->value, insn->name, addrValue, 
					(mem->IsIndirectMemoryAccess(m)) ? "I" : " ", label.maxLabelLength, (target_lb) ? target_lb->name : "???");
			}
			else if(insn->type == EX3_InsnSet::REG_INSN)
			{ fprintf(fp, "%04x [%04x]: %s%*s", addr, m->value, insn->name, label.maxLabelLength + 9, "");}
			else if(m->status != 0){
				const char * field = (m->value >= 0x20 && m->value <= 0x7e) ? "%04x [%04x]: (%5d:  '%c')%*s" : "%04x [%04x]: (%5d:%5d)%*s";
				fprintf(fp, field, addr, m->value, (signed short) m->value, m->value, label.maxLabelLength + 1, "");
			}
			else if(m->value != 0)
			{ EMIT_MESSAGE_3(fprintf, fp, "%04x [%04x]: ERROR!!! this memory location is unused but has non-zero value!!!\n", addr, m->value);}
			if(m->tailComment){ fprintf(fp, "\t/%s", m->tailComment);}
			fprintf(fp, "\n");
		}
	}
};

class EX3_ASMParser : public ASMParser
{
public:
	EX3_ASMParser(CPU * cpu0, const char * fname) : ASMParser(cpu0, fname){}
	int ParseInsn(int passNum, const char * p, InsnSet::Insn * insn, int & addr){
		if(insn->type == EX3_InsnSet::REG_INSN || insn->type == EX3_InsnSet::MEM_INSN){
			if(passNum == 2){
				Memory::Word * m = &cpu->mem->word[addr];
				m->insnID = insn->ID;
				m->value = insn->code;
				m->SetStatus(&cpu->label.annotation, addr);
				cpu->dbg->InsertMonitorOrBreakpoint(m->status, addr, true);
				if(insn->type == EX3_InsnSet::MEM_INSN){
					SkipWhiteSpace(&p);
					int len = GetLabelLength(p);
					if(len == 0){ printf("ERROR: label(%s) length = 0 (bug in parser)\n", p); PrintErrorLocation(); return -1;}
					Label::Element * lb = cpu->label.GetLabel(p, len);
					if(lb == 0){ printf("ERROR: label(%s) not found in the 1st pass (bug in the program...)\n", p); PrintErrorLocation(); return -1;}
					if(!cpu->mem->IsValidAddress(lb->address)){ printf("ERROR: label(%s) has invalid address (%x)\n", lb->name, lb->address); PrintErrorLocation(); return -1;}
					m->value |= lb->address;
					p += lb->nlen;
					SkipWhiteSpace(&p);
					if(*p == 'I'){ m->value |= 0x8000; p ++;}	///	set bit15 to 1
				}
			}
			if(passNum == 2){ ExtractComment(p, addr, 0);}
			addr ++;
			return 1;
		}
		else{ return 0;}
	}
};

class EX3_TerminalSystem
{
public:
	EX3_CPU cpu;
	EX3_ASMParser parser;
	System::TerminalViewer termView;
	System::InputTerminal inTerm;
	System::OutputTerminal outTerm;
	EX3_TerminalSystem(const char * fname) : cpu(0x1000), parser(&cpu, fname), inTerm(&cpu), outTerm(&cpu){
		inTerm.Open(parser.OpenFile("_in.log"));
		outTerm.Open(parser.OpenFile("_out.log"));
		inTerm.termView = &termView;
		outTerm.termView = &termView;
	}
	~EX3_TerminalSystem(){ cpu.dbg->EnableAllLogs(); Close();}
	void AccessInPort(){ inTerm.AccessPort(); cpu._FGI = 0;}
	void AccessOutPort(){ cpu._FGO = 0; outTerm.AccessPort();}
	void PrintString(const char * s){ while(*s){ cpu._OUTR = *(s ++); AccessOutPort();}}
	void Close(){ cpu.PrintDataMemory(); parser.Close();}
	void RunCPUModel(){
		cpu.Reset();
		int inFlag, outFlag, cpuFlag;
		do{
			if(cpu.dbg->Debug()){ break;}
			///	1. simulate components : inTerm, outTerm, cpu
			inFlag = inTerm.Execute();
			outFlag = outTerm.Execute();
			cpuFlag = cpu.Execute();
		} while(inFlag + cpuFlag + outFlag == 0);
		inTerm.Close();
		outTerm.Close();
		termView.PrintView();
	}
	///	below functions are defined in tic_tac_toe.cpp:
	void RunSoftModel();
	void RunCPU_vs_SoftModel();
};
#endif /// _EX3_ASM_H_INCLUDED_


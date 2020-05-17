#if !defined _COMMON_ASM_H_INCLUDED_

#define _COMMON_ASM_H_INCLUDED_

#ifdef WIN32
#include <conio.h>
#else	/// LINUX
#include <termios.h>
#endif

#define COMMENT '/'

#define ABORT_PROGRAM(msg)	msg; printf("hit return : "); getchar(); exit(-1)

#if !defined(WIN32)
class GetChSetting
{
public:
	termios oldTermios, newTermios;
	GetChSetting(){
		tcgetattr(0, &oldTermios);
		newTermios = oldTermios;
		newTermios.c_lflag &= ~ICANON;	/// disable buffered i/o
		tcsetattr(0, TCSANOW, &newTermios);
	}
	~GetChSetting(){ tcsetattr(0, TCSANOW, &oldTermios);}
};
extern int _getche(void);
#endif

class Label
{
public:
	class Element
	{
	public:
		char * name;
		int nlen;
		unsigned short address;
		Element(){ name = 0; nlen = 0; address = 0;}
		void Set(const char * n, int len, unsigned short addr){
			if(name){ delete [] name;}
			nlen = len;
			name = new char[nlen + 1];
			strncpy(name, n, nlen);
			name[nlen] = 0;
			address = addr;
		}
		~Element(){ if(name) delete [] name;}
		void PrintInfo(FILE * fp, int maxlen){ if(fp) fprintf(fp, "label(%*s) : %5d(0x%04x)\n", maxlen, name, address, address);}
	};
	enum AnnotationLabel{
		AL_Null       = 0x0,
		AL_Breakpoint = 0x1,
		AL_Monitor    = 0x2,
	};
	class AnnotationStatus
	{
	public:
		int curAddr, annotation;
		AnnotationStatus(){ curAddr = 0; annotation = AL_Null;}
		AnnotationLabel CheckAnnotationLabel(const char * p, int len){
			if(len != 3 || p[0] != '_' || p[2] != '_'){ return AL_Null;}
			switch(p[1]){
			case 'B': return AL_Breakpoint;
			case 'M': return AL_Monitor;
			default : return AL_Null;
			}
		}
		bool AddAnnotation(const char * p, int len, unsigned short addr){
			if(curAddr != addr){ annotation = AL_Null; curAddr = addr;}
			AnnotationLabel al = CheckAnnotationLabel(p, len);
			annotation |= al;
			return (al != AL_Null);
		}
	} annotation;
#define MAX_LABEL_COUNT	1000
	Element element[MAX_LABEL_COUNT];
	int count, maxLabelLength;
	Label() : count(0), maxLabelLength(0){}
	Element * AddLabel(const char * n, int len, unsigned short addr){
		Element * lb;
		if(count >= MAX_LABEL_COUNT)	{ printf("ERROR: labelCount exceeds limit %d\n", MAX_LABEL_COUNT); return 0;}
		if((lb = GetLabel(n, len)))		{ printf("ERROR: label(%s) already exists!!\n", lb->name); return 0;}
		lb = &element[count ++];
		lb->Set(n, len, addr);
		if(maxLabelLength < len){ maxLabelLength = len;}
		return lb;
	}
	Element * GetLabel(const char * p, int len){
		int i;
		for(i = 0; i < count; i ++){ if(element[i].nlen == len && strncmp(element[i].name, p, len) == 0) return &element[i];}
		return 0;
	}
	Element * GetLabel(int addr){
		int i;
		for(i = 0; i < count; i ++){ if(element[i].address == addr) return &element[i];}
		return 0;
	}
	void PrintLabels(FILE * fp){
		if(fp){
			int i;
			for(i = 0; i < count; i ++){ element[i].PrintInfo(fp, maxLabelLength);}
		}
	}
};

class CPU;
class Memory
{
public:
	enum Status
	{	///	bit[0] : 1 (used as instruction or data), 0 (unused)
		MS_Null				= 0x0,	///	00
		MS_Used				= 0x1,	///	01
		MS_Monitor          = 0x2,	///	for ISS and Verilog simulations
		MS_Breakpoint		= 0x4,	///	for ISS (breakpoint set)
	};
	class Word
	{
	public:
		unsigned char status;
		unsigned char insnID;
		unsigned short value;
		char * headComment, * tailComment;
		Word(){ status = MS_Null; insnID = 0; value = 0; headComment = 0; tailComment = 0;}
		~Word(){
			if(headComment){ delete [] headComment;}
			if(tailComment){ delete [] tailComment;}
		}
		void SetStatus(Label::AnnotationStatus * labelAnnotation, int addr){
			status = MS_Used;
			if(labelAnnotation->curAddr == addr){
				if(labelAnnotation->annotation & Label::AL_Monitor)		{ status |= MS_Monitor;}
				if(labelAnnotation->annotation & Label::AL_Breakpoint)	{ status |= MS_Breakpoint;}
			}
		}
		void SetComment(const char * c, int len, int headFlag){
			if(headFlag){
				int len0 = (headComment) ? strlen(headComment) + 2: 0;
				int len2 = len0 + len;
				char * comment2 = new char[len2 + 1];
				char * p = comment2;
				if(headComment){ sprintf(p, "%s\n/", headComment); p += strlen(headComment) + 2;}
				strncpy(p, c, len);
				comment2[len2] = 0;
				if(headComment){ delete [] headComment;}
				headComment = comment2;
			}
			else if(tailComment == 0){
				char * comment2 = new char[len + 1];
				char * p = comment2;
				strncpy(p, c, len);
				comment2[len] = 0;
				tailComment = comment2;
			}
		}
	};
	Word * word, * curCode, bogusWord;	///	bogusWord : for returning memory word upon access error
	int size, maxAddr;
	unsigned short errorFlag;
	Memory(int sz){
		size = sz;
		maxAddr = 0;
		errorFlag = 0;
		curCode = 0;
		word = new Word[size];
	}
	~Memory(){ delete [] word;}
	bool IsValidAddress(int addr){ return addr >= 0 && addr < size;}
	Word * GetWord(unsigned int addr){
		if(!IsValidAddress(addr) || errorFlag){ errorFlag = 1; return &bogusWord;}
		word[addr].status |= MS_Used;
		return &word[addr];
	}
	void ProbeInsn(unsigned short pc){ curCode = GetWord(pc);}
	void FetchInsn(unsigned short & pc){ curCode = GetWord(pc); pc ++;}
	unsigned short Address(){
		Word * m = (IsIndirectMemoryAccess(curCode)) ? GetWord(GetAddressField(curCode)) : curCode;
		return GetAddressField(m);
	}
	unsigned short & Operand(){ return GetWord(Address())->value;}

	///	virtual abstract functions : must be defined in actual class
	virtual bool IsIndirectMemoryAccess(Word * w)			= 0;
	virtual unsigned short GetAddressField(Word * m)		= 0;
};

class InsnSet
{
public:
	class Insn
	{
	public:
		unsigned char ID;
		unsigned char type;
		unsigned char nlen;
		unsigned char showMemFlag;
		const char * name;
		unsigned short code;
		void (* operation)(CPU *);
		Insn(){ ID = 0; type = 0; nlen = 0; showMemFlag = 0; name = 0; operation = OP_DUMMY;}
		void Set(int id, const char * n, int t, int showMem, void (* op)(CPU *), unsigned short c){
			ID = id; name = n; type = t; showMemFlag = showMem;
			nlen = strlen(name);
			operation = op;
			code = c;
		}
	};
	Insn * SearchInsn(const char ** iname_p){
		const char * iname = *iname_p;
		int i;
		for(i = 0; i < ICount; i ++){
			Insn * ii = &insn[i];
			if(ii->nlen > 0 && strncmp(ii->name, iname, ii->nlen) == 0){ *iname_p += ii->nlen; return ii;}
		}
		return 0;
	}
	Insn * insn;
	int ICount;
	enum InsnID
	{
		I_INVALID, /// valid InsnID is non-zero
		I_ORG, I_END, I_DEC, I_HEX, I_CHR, I_SYM,
		I_TAIL,
	};
	InsnSet(int icount){
		ICount = icount;
		insn = new Insn[ICount];
#define NI(i)	insn[I_##i].Set(I_##i, #i, 0, 0, OP_DUMMY, 0)
		NI(ORG); NI(END); NI(DEC); NI(HEX); NI(CHR); NI(SYM);
#undef  NI
	}
	~InsnSet(){ delete [] insn;}
	static void OP_DUMMY(CPU * cpu){}
};

class Debugger;

#define ECHO	(Debugger::globalDBG->Echo())
#define FDMP	(Debugger::globalDBG->FileLog())

#define EMIT_MESSAGE_0(func, fp)							if(FDMP) func(fp);							if(ECHO) func(stdout)
#define EMIT_MESSAGE_1(func, fp, arg0)						if(FDMP) func(fp, arg0);					if(ECHO) func(stdout, arg0)
#define EMIT_MESSAGE_2(func, fp, arg0, arg1)				if(FDMP) func(fp, arg0, arg1);				if(ECHO) func(stdout, arg0, arg1)
#define EMIT_MESSAGE_3(func, fp, arg0, arg1, arg2)			if(FDMP) func(fp, arg0, arg1, arg2);		if(ECHO) func(stdout, arg0, arg1, arg2)
#define EMIT_MESSAGE_4(func, fp, arg0, arg1, arg2, arg3)	if(FDMP) func(fp, arg0, arg1, arg2, arg3);	if(ECHO) func(stdout, arg0, arg1, arg2, arg3)
#define EMIT_MESSAGE_5(func, fp, arg0, arg1, arg2, arg3, arg4)	\
	if(FDMP) func(fp, arg0, arg1, arg2, arg3, arg4);	if(ECHO) func(stdout, arg0, arg1, arg2, arg3, arg4)

#define PROGRAM_ENTRY_POINT	0x10
class CPU
{
public:
	Label label;
	Memory * mem;
	InsnSet * isa;
	unsigned short _S, _PC, _oldPC, _INPR, _OUTR, _FGI, _FGO;
	int cycle_count, appl_cycle_count, intr_cycle_count, intr_pending;
	FILE * fplog;
	Debugger * dbg;
	CPU(const char * logfilename) : mem(0), isa(0), dbg(0){ fplog = fopen(logfilename, "w"); Reset();}
	~CPU(){
		if(mem)		{ delete mem;}
		if(isa)		{ delete isa;}
		if(fplog)	{ fclose(fplog);}
	}
	void PrintSeparator(FILE * fp){ fprintf(fp, "----------------\n");}
	virtual void Reset(){
		cycle_count			= 0;
		appl_cycle_count	= 0;
		intr_cycle_count	= 0;
		intr_pending		= 0;
		_S					= 1;
		_oldPC				= PROGRAM_ENTRY_POINT;
		_PC					= PROGRAM_ENTRY_POINT;
		_INPR				= 0;
		_OUTR				= 0;
		_FGI				= 0x0;
		_FGO				= 0x3;
	}
	void PrintMemory(FILE * fp, int printMode){
		int i;
		for(i = 0; i < mem->maxAddr; i ++){ PrintMemoryWord(&mem->word[i], fp, i, printMode);}
	}
	bool IsInputReady(){ return _GetFGI() == 0;}
	bool IsOutputReady(){ return _GetFGO() == 0;}
	void SetInput(unsigned char val){
		if(IsInputReady())		{ _SetFGI(1); _INPR = val;}
		else					{ printf("ERROR!!! SetInput() called when input is not ready...\n");}
	}
	unsigned char GetOutput(){
		if(IsOutputReady())		{ _SetFGO(1); return (unsigned char) _OUTR;}
		else					{ printf("ERROR!!! GetInput() called when output is not ready...\n"); return -1;}
	}
	///	virtual abstract functions : must be defined in actual class
	virtual int GetSelectedPortID() = 0;
	virtual int Execute() = 0;
	virtual bool IsWaitingForInput() = 0;
	virtual void _SetFGI(int val) = 0;
	virtual void _SetFGO(int val) = 0;
	virtual int _GetFGI() = 0;
	virtual int _GetFGO() = 0;
	virtual	void PrintStatus(FILE * fp, bool intr_cycle) = 0;
	enum PrintMemoryWordMode{
		PM_Null            = 0x0,
		PM_Verilog         = 0x1,
		PM_Program         = 0x2,
		PM_Monitor         = 0x4, 
		PM_SkipHeadComment = 0x8,
	};
	virtual void PrintMemoryWord(Memory::Word * m, FILE * fp, int addr, int printMode) = 0;
};

class Debugger
{
public:
	enum BreakpointType
	{
		BT_PC,		///	break at PC
		BT_ICOUNT,	/// break at insnCount
		BT_MEM,		///	break at memory change
	};
	class Breakpoint
	{
	public:
		BreakpointType btype;
		int val, ID, addr;
		unsigned short * mem;
		void Set(int id, BreakpointType bt, int value, unsigned short * mem0){
			ID = id, btype = bt;
			mem = 0; val = 0;
			if(btype == BT_PC)			{ addr = value;}
			else if(btype == BT_ICOUNT)	{ val = value;}
			else						{ addr = value; mem = mem0;}
		}
		const char * GetBreakpointTypeName(){
			switch(btype){
			case BT_PC:		return "PC";
			case BT_ICOUNT:	return "ICOUNT";
			case BT_MEM:	return "MEM";
			}
		}
		void PrintInfo(Debugger * dbg = 0){
			printf("Breakpoint(%d) at ", ID);
			if(btype == BT_PC)			{ printf("PC (0x%03x)", addr);}
			else if(btype == BT_ICOUNT)	{ printf("insnCount (%d)", val);}
			else						{ printf("MEM (0x%03x) = (%04x : %04x)", addr, val, *mem);}
			if(dbg)	{ printf("\n"); dbg->ShowCPUStatus();}
			else	{ printf("\n");}
		}
	};
	enum CommandType
	{
		CT_Start,				/// initial state
		CT_Run,					/// 'r'
		CT_Step,				/// 's'
	};
#define BREAKPOINT_COUNT 100
#define MONITOR_COUNT 100
	Breakpoint breakpoints[BREAKPOINT_COUNT], monitors[MONITOR_COUNT];
	int breakpointCount, monitorCount, bpID;
	int verboseMode, fileLogMode;
	CommandType com;
	CPU * cpu;
	static Debugger * globalDBG;
	Debugger(CPU * cpu0){
		if(globalDBG){ ABORT_PROGRAM(printf("globalDBG != 0\n"));}
		globalDBG = this;
		cpu = cpu0; breakpointCount = 0; monitorCount = 0; bpID = 0; com = CT_Start; verboseMode = 0; fileLogMode = 0;
	}
	~Debugger(){ globalDBG = 0;}
	bool Echo(){ return (com == CT_Step || verboseMode || DetectMonitor());}
	bool FileLog(){ return (com == CT_Start || fileLogMode || DetectMonitor());}
	void ShowCommand(){
		printf(	"r : run\n"
				"s : step\n"
				"b : show breakpoints\n"
				"d : delete breakpoint\n"
				"p : set PC breakpoint\n"
				"i : set InsnCount breakpoint\n"
				"m : set Memory Monitor breakpoint\n"
				"l : toggle verbose CPU-log mode (verboseMode = %d)\n"
				"f : toggle file CPU-log mode (fileLogMode = %d)\n"
				"w : show CPU status\n"
				"n : show PC monitors\n"
				"h : show commands\n"
				"q : quit\n"
				, verboseMode, fileLogMode
				);
	}
	int Debug(){
		UpdateMonitor();
		switch(com){
		case CT_Run: if(!DetectBreakpoint())	return 0;
		case CT_Step:							break;
		case CT_Start: ShowCommand();			break;
		}
		return Command();
	}
	int Command(){
		while(1){
			printf("EX3-DBG > ");
			int ch = _getche();
			printf("\n");
			switch(ch){
			case 'r': com = CT_Run;						return 0;
			case '\n':
			case 's': com = CT_Step;					return 0;
			case 'b': ShowBreakpoints();				break;
			case 'd': DeleteBreakpoint();				break;
			case 'p': InsertBreakpoint(BT_PC);			break;
			case 'i': InsertBreakpoint(BT_ICOUNT);		break;
			case 'm': InsertBreakpoint(BT_MEM);			break;
			case 'n': ShowMonitors();					break;
			case 'l': ToggleVerboseMode();				break;
			case 'f': ToggleFileLogMode();				break;
			case 'w': ShowCPUStatus();					break;
			case 'h': ShowCommand();					break;
			case 'q': return 1;
			default : ShowCommand();					break;
			}
		}
		return 0;
	}
	void ToggleVerboseMode(){ verboseMode = 1 - verboseMode; printf("verboseMode = %d\n", verboseMode);}
	void ToggleFileLogMode(){ fileLogMode = 1 - fileLogMode; printf("fileLogMode = %d\n", fileLogMode);}
	void EnableAllLogs(){ verboseMode = 1; fileLogMode = 1;}
	void ShowBreakpoints(){
		printf("%d breakpoints\n", breakpointCount);
		int i;
		for(i = 0; i < breakpointCount; i ++){ breakpoints[i].PrintInfo();}
	}
	void GetValue(const char * msg, const char * field, int * value){ printf(msg); scanf(field, value);}
	void DeleteBreakpoint(){
		int ID;
		GetValue("delete breakpoint ID = ", "%d", &ID);
		int i;
		for(i = 0; i < breakpointCount; i ++){
			if(breakpoints[i].ID == ID){
				breakpointCount --;
				for(; i < breakpointCount; i ++){ breakpoints[i] = breakpoints[i + 1];}
				break;
			}
		}
		ShowBreakpoints();
	}
	void InsertBreakpoint(BreakpointType bt){
		if(breakpointCount >= BREAKPOINT_COUNT){ printf("Too many breakpoints... (delete some to add new ones)\n"); return;}
		int value;
		unsigned short * mem0 = 0;
		if(bt == BT_ICOUNT){ GetValue("insn count (decimal) = ", "%d", &value);}
		else{
			GetValue("address (hex) = ", "%x", &value);
			if(value >= cpu->mem->size){ printf("address is out of range...\n"); return;}
			if(bt == BT_MEM){ mem0 = &cpu->mem->word[value].value;}
		}
		breakpoints[breakpointCount ++].Set(bpID ++, bt, value, mem0);
		ShowBreakpoints();
	}
	void InsertMonitorOrBreakpoint(int status, int addr, bool isInsn){
		if(addr < 0 || addr >= cpu->mem->size){ ABORT_PROGRAM("Error in InsertMonitorOrBreakpoint!!!\n");}
		BreakpointType bt = (isInsn) ? BT_PC : BT_MEM;
		unsigned short * mem0 = 0;
		if(bt == BT_MEM){ mem0 = &cpu->mem->word[addr].value;}
		if(status & Memory::MS_Monitor){
			if(monitorCount >= MONITOR_COUNT){ printf("[InsertMonitorOrBreakpoint] Too many monitors... \n"); return;}
			monitors[monitorCount].Set(monitorCount, bt, addr, mem0);
			monitorCount ++;
		}
		if(status & Memory::MS_Breakpoint){
			if(breakpointCount >= BREAKPOINT_COUNT){ printf("[InsertMonitorOrBreakpoint] Too many breakpoints... (delete some to add new ones)\n"); return;}
			breakpoints[breakpointCount ++].Set(bpID ++, bt, addr, mem0);
		}
	}
	bool DetectBreakpoint(){
		int i;
		Breakpoint * bp = breakpoints;
		for(i = 0; i < breakpointCount; i ++, bp ++){
			switch(bp->btype){
			case BT_PC:		if(cpu->_PC == bp->addr)		{ bp->PrintInfo(this); return true;} break;
			case BT_ICOUNT:	if(cpu->cycle_count == bp->val)	{ bp->PrintInfo(this); return true;} break;
			case BT_MEM:	if(*bp->mem != bp->val)			{ bp->PrintInfo(this); bp->val = *bp->mem; return true;} break;
			}
		}
		return false;
	}
	void ShowMonitors(){
		int i;
		Breakpoint * mon = monitors;
		printf("monitors = {");
		for(i = 0; i < monitorCount; i ++, mon ++){
			if(i){ printf(", ");}
			printf("0x%03x(%s)", mon->addr, (mon->btype == BT_PC) ? "PC" : "MEM");
		}
		printf("}\n");
	}
	bool DetectMonitor(){
		int i;
		Breakpoint * mon = monitors;
		for(i = 0; i < monitorCount; i ++, mon ++){
			switch(mon->btype){
			case BT_PC:		if(cpu->_PC == mon->addr)		{ return true;} break;
			case BT_MEM:	if(*mon->mem != mon->val)		{ return true;} break;
			}
		}
		return false;
	}
	void UpdateMonitor(){
		int i;
		Breakpoint * mon = monitors;
		for(i = 0; i < monitorCount; i ++, mon ++){ if(mon->mem){ mon->val = *mon->mem;}}
	}
	void ShowCPUStatus(){ cpu->PrintStatus(stdout, 0);}
};

#define MAX_LINE 1000
#define VERILOG_DIR "../verilog/"
class ASMParser
{
public:
	CPU * cpu;
	FILE * fp;
	const char * asm_name;
	static const char * tool_name;
	int asmLineNum, blockCommentPending;
	ASMParser(CPU * cpu0, const char * fname){ cpu = cpu0; fp = 0; asm_name = fname;}
	void Close(){
		if(fp){ fclose(fp);}
		fp = 0;
		WriteVerilogMemProbeFile();
	}
	~ASMParser(){ if(fp) fclose(fp);}
	int Open(){
		if(strcmp(asm_name + strlen(asm_name) - 4, ".asm") != 0){ printf("Incorrect ASM file extension : %s\n", asm_name); return -1;}
		fp = fopen(asm_name, "r");
		if(fp == 0){ printf("Cannot open (%s)\n", asm_name); return -1;}
		///	1st pass : extract labels
		if(Parse(1) != 0){ return -1;}
		EMIT_MESSAGE_0(cpu->PrintSeparator, cpu->fplog);
		EMIT_MESSAGE_0(cpu->label.PrintLabels, cpu->fplog);
		rewind(fp);
		///	2nd pass : extract instructions
		if(Parse(2) != 0){ return -1;}
		EMIT_MESSAGE_0(cpu->PrintSeparator, cpu->fplog);
		EMIT_MESSAGE_1(cpu->PrintMemory, cpu->fplog, CPU::PM_Program);
		EMIT_MESSAGE_0(cpu->PrintSeparator, cpu->fplog);
		fclose(fp);
		fp = 0;
		if(WriteVerilogMemFile() != 0 || WriteVerilogMonitorFile() != 0){ return -1;}
		return 0;
	}
	FILE * OpenFile(const char * extname){
		FILE * fp0 = 0;
		char * mem_fname = new char[strlen(asm_name) + strlen(VERILOG_DIR) + strlen(extname) + 1];
		sprintf(mem_fname, "%s%s", VERILOG_DIR, asm_name);
		char * p = mem_fname + strlen(mem_fname) - 4;
		if(*p != '.')	{ printf("sorry, something is wrong... (please contact admin)\n");}
		else			{ strcpy(p, extname); fp0 = fopen(mem_fname, "w");}
		delete [] mem_fname;
		return fp0;
	}
	int WriteVerilogMemFile(){
		FILE * fp0 = OpenFile(".mem");
		if(fp0){
			fprintf(fp0, "/// Verilog Memory Initialization File (.mem) generated by %s\n\n"
						 "/// 12-bit address\n"
						 "/// 16-bit data\n\n"
						 , tool_name);
			cpu->PrintMemory(fp0, CPU::PM_Verilog | CPU::PM_Program);
			fclose(fp0);
			return 0;
		}
		else{ printf("Cannot open memory output file (skipped memory output)\n"); return -1;}
	}
	int WriteVerilogMemProbeFile(){
		FILE * fp0 = OpenFile(".prb");
		if(fp0){
			fprintf(fp0, "/// Verilog Memory Probe File (.prb) generated by %s\n\n"
						 "/// bit[31:28] : memory type (0000 : data, 1111 : end-of-memory\n"
						 "/// bit[27:16] : address\n"
						 "/// bit[15:0]  : data\n\n"
						 , tool_name);
			cpu->PrintMemory(fp0, CPU::PM_Verilog);
			fprintf(fp0, "%1x%03x%04x\t///\t%s\n", 0xf, 0, 0, "end-of-memory");
			fclose(fp0);
			return 0;
		}
		else{ printf("Cannot open memory output file (skipped memory output)\n"); return -1;}
	}
	int WriteVerilogMonitorFile(){
		FILE * fp0 = OpenFile(".mon");
		if(fp0){
			fprintf(fp0, "/// Verilog Monitor File (.mon) generated by %s\n\n"
						 "/// bit[31:28] : memory type (0000 : prog, 0001 : data, 1111 : end-of-memory\n"
						 "/// bit[27:16] : address\n"
						 "/// bit[15:0]  : data\n\n"
						 , tool_name);
			cpu->PrintMemory(fp0, CPU::PM_Verilog | CPU::PM_Monitor);
			fprintf(fp0, "%08x\t///\t%s\n", (unsigned int)(-1), "end-of-memory");
			fclose(fp0);
			return 0;
		}
		else{ printf("Cannot open memory output file (skipped memory output)\n"); return -1;}
	}
	void SkipWhiteSpace(const char ** pp){
		const char * p = *pp;
		while(*p == ' ' || *p == '\t'){ p ++;}
		*pp = p;
	}
	int GetNum(const char ** pp, int insnType){
		if(insnType == InsnSet::I_CHR){ return **pp;}
		const char * p = *pp;
		int val = 0;
		int first = 1;
		int base = 0;
		switch(insnType){
			case InsnSet::I_ORG:
			case InsnSet::I_HEX:	base = 16;	break;
			case InsnSet::I_DEC:	base = 10;	break;
			default:				return -1;
		}
		int sign = 0;
		if(*p == '-'){ sign = 1; p ++;}
		while(*p){
			if(*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == COMMENT){ break;}
			val *= base;
			int num = *p - '0';
			if(num >= 0 && num <= 9){ val += num;}
			else if(base == 16){
				int upper = *p - 'A';
				int lower = *p - 'a';
				if		(upper >= 0 && upper <= 5)	{ val += upper + 10;}
				else if	(lower >= 0 && lower <= 5)	{ val += lower + 10;}
				else								{ return -1;}	///	error
			}
			else{ return -1;}	/// error
			p ++;
			first = 0;
		}
		*pp = p;
		if(sign){ val = -val;}
		return (first) ? -1 : val;
	}
	int ParseLabel(const char * p){
		int nlen = 0;
		while(*p){
			switch(*p){
				case ' ':
				case '\t':	return 0;
				case ',':	return nlen;
				default:	break;
			}
			nlen ++;
			p ++;
		}
		return 0;
	}
	int GetLabelLength(const char * p){
		int nlen = 0;
		while(*p){
			switch(*p){
				case ' ':
				case '\r':
				case '\t':
				case '\n':
				case ',':	return nlen;
				default:	break;
			}
			nlen ++;
			p ++;
		}
		return 0;
	}
	int ExtractComment(const char * p, int addr, int headFlag){
		if(addr >= 0 && addr < cpu->mem->size){
			Memory::Word * m = &cpu->mem->word[addr];
			SkipWhiteSpace(&p);
			if(*p == '/'){
				const char * pp = p;
				while(*pp && *pp != '\r' && *pp != '\n'){ pp ++;}
				m->SetComment(p, pp - p, headFlag);
				return 1;
			}
		}
		return 0;
	}
	void PrintErrorLocation(){ printf("Error occurred on line %d\n", asmLineNum);}
	int ParseLabel(int passNum, const char ** p, int & addr){
		if(passNum == 2 && ExtractComment(*p, addr, 1)){ return 1;}
		SkipWhiteSpace(p);
		if(**p == '/'){ return 1;}	///	comment...
		int labelLength = ParseLabel(*p);
		if(labelLength > 0){
			if(cpu->label.annotation.AddAnnotation(*p, labelLength, (unsigned short) addr)){}
			else if(passNum == 1){	///	create label
				if(cpu->label.AddLabel(*p, labelLength, (unsigned short) addr) == 0){ PrintErrorLocation(); return -1;}
			}
			else if(cpu->label.GetLabel(*p, labelLength) == 0)
			{ printf("ERROR: label(%s) not found in 1st pass (bug in the parser...)\n", p); PrintErrorLocation(); return -1;}
			*p += labelLength + 1;
			SkipWhiteSpace(p);
			if(passNum == 2){ ExtractComment(*p, addr, 1);}
			if(**p == '/'){ return 1;}
		}
		return (**p == '\n' || **p == '\r') ? 1 : 0;
	}
	int ParseNonInsn(int passNum, const char * p, int insnID, int & addr){
		SkipWhiteSpace(&p);
		int param = GetNum(&p, insnID);
		Memory::Word * m = &cpu->mem->word[addr];
		int endReached = 0;
		switch(insnID){
			case InsnSet::I_ORG:
				addr = param;
				if(!cpu->mem->IsValidAddress(addr))
				{ printf("ERROR: ORG has invalid address %d (0x%x)\n", addr, addr); PrintErrorLocation(); return -1;}
				if(passNum == 2){ ExtractComment(p, addr, 1);}
				break;
			case InsnSet::I_END:
				endReached = 1;
				break;
			case InsnSet::I_SYM:
				if(passNum == 2){
					SkipWhiteSpace(&p);
					int len = GetLabelLength(p);
					if(len == 0)
					{ printf("ERROR: label(%s) length = 0 (bug in parser)\n", p); PrintErrorLocation(); return -1;}
					Label::Element * lb = cpu->label.GetLabel(p, len);
					if(lb == 0)
					{ printf("ERROR: label(%s) not found in the 1st pass (bug in the program...)\n", p); PrintErrorLocation(); return -1;}
					if(!cpu->mem->IsValidAddress(lb->address))
					{ printf("ERROR: label(%s) has invalid address (%x)\n", lb->name, lb->address); PrintErrorLocation(); return -1;}
					param = lb->address;
				}
			case InsnSet::I_CHR:
			case InsnSet::I_HEX:
			case InsnSet::I_DEC:
				if(passNum == 2){
					m->SetStatus(&cpu->label.annotation, addr);
					cpu->dbg->InsertMonitorOrBreakpoint(m->status, addr, false);
					m->value = param;
				}
				if(passNum == 2){ ExtractComment(p, addr, 0);}
				addr ++;
				break;
			default:
				break;
		}
		return (endReached) ? 1 : 0;
	}
	int ParseBlockCommentStart(const char ** p){
		if(!blockCommentPending){
			SkipWhiteSpace(p);
			if(strncmp(*p, "/*", 2) == 0){ blockCommentPending = 1; (*p) += 2; return 1;}
		}
		return 0;
	}
	int ParseBlockCommentEnd(const char ** p){
		if(blockCommentPending){
			while(**p){
				if(strncmp(*p, "*/", 2) == 0){ (*p) += 2; blockCommentPending = 0; return 1;}
				else{ (*p) ++;}
			}
		}
		return 0;
	}
	int ParseBlockComment(const char ** p){
		while(ParseBlockCommentStart(p) || ParseBlockCommentEnd(p)){}
		return (blockCommentPending != 0);
	}
	int Parse(int passNum){
		if(passNum != 1 && passNum != 2){ printf("passNum = %d is invalid\n", passNum); return -1;}
		char buf[MAX_LINE];
		int addr = 0, endReached = 0;
		blockCommentPending = 0;
		asmLineNum = 0;
		while(!endReached){
			if(fgets(buf, MAX_LINE, fp) == 0){ printf("ERROR: End of file reached before END\n"); return -1;}
			asmLineNum ++;
			const char * p = buf;
			if(ParseBlockComment(&p)){ continue;}
			switch(ParseLabel(passNum, &p, addr)){
				case -1:	return -1;
				case  1:	continue;
				default:	break;
			}
			InsnSet::Insn * insn = cpu->isa->SearchInsn(&p);
			if(insn == 0){ printf("ERROR: Invalid instruction (%s)\n", p); PrintErrorLocation(); return -1;}
			switch(ParseInsn(passNum, p, insn, addr)){
				case -1:	return -1;
				case  1:	continue;
				default:	break;
			}
			switch(ParseNonInsn(passNum, p, insn->ID, addr)){
				case -1:	return -1;
				case  1:	endReached = 1;
				default:	break;
			}
		}
		if(passNum == 1){
			if(!cpu->mem->IsValidAddress(addr)){ printf("ERROR: program extends to invalid address %d (0x%x)\n", addr, addr); return -1;}
			cpu->mem->maxAddr = addr;
		}
		return 0;
	}
	///	virtual abstract functions : must be defined in actual class
	virtual int ParseInsn(int passNum, const char * p, InsnSet::Insn * insn, int & addr) = 0;
};

class String
{
public:
	char * string;
	int length, allocLength;
#define DEFAULT_ALLOC_LENGTH 100
	void Allocate(){
		char * s = new char[allocLength];
		if(string){
			strncpy(s, string, length);
			delete [] string;
		}
		s[length] = 0;
		string = s;
	}
	String(){
		allocLength = DEFAULT_ALLOC_LENGTH;
		string = 0;
		length = 0;
		Allocate();
	}
	~String(){ delete [] string;}
	void Insert(int c){
		if(c == 0){ Insert('\\'); Insert('0'); return;}
		if(allocLength - length <= 1){
			allocLength = allocLength + (allocLength >> 1) + 1;
			Allocate();
		}
		string[length ++] = (char) c;
		string[length] = 0;
	}
	void Reset(){ length = 0; string[0] = 0;}
};

class System
{
public:
#define MAX_INTERVAL 50
	class RandomPeripheral
	{
	public:
		CPU * cpu;
		const char * name;
		int type;	/// type = 0 : output, type = 1 : input 
		int interval;
		FILE * fp;
		RandomPeripheral(CPU * cpu0, const char * n, int t) : cpu(cpu0), name(n), type(t), interval(-1), fp(0){}
		virtual ~RandomPeripheral(){ if(fp){ fclose(fp);}}
		enum TraceType{ TT_Interval = 0, TT_Data = 1 };
		void Open(FILE * fp0){
			fp = fp0;
			if(fp){
				fprintf(fp, "/// RandomPeripheral trace (%s) :\n", name);
				fprintf(fp, "/// bit[17]   (port ID) : 0 = gpio, 1 = uart\n");
				fprintf(fp, "/// bit[16]   (trace type) : 0 = interval cycles, 1 = data\n");
				fprintf(fp, "/// bit[15:0] (value)\n\n");
			}
		}
		void Close(){
			if(type == 0 && IsPortReady())	{ AccessPort();}	///	access output ready port before closing...
			if(fp)							{ fclose(fp);}
			fp = 0;
		}
		int GetPortID(){ return cpu->GetSelectedPortID();}
		const char * GetPortName(){ return (GetPortID()) ? "SIO" : "PIO";}
		void RecordTrace(int rtype, unsigned char value){ if(fp){ fprintf(fp, "%x%04x\n", (GetPortID() << 1) | rtype, value);}}
		virtual int GetInterval(){ return rand() % MAX_INTERVAL;}
		void SetRandomInterval(){
			interval = GetInterval();
			EMIT_MESSAGE_4(fprintf, cpu->fplog, 
				"-------------------------------------\n"
				"%s[%s].interval = %d\n"
				"-------------------------------------\n"
				, name, GetPortName(), interval);
			RecordTrace(TT_Interval, interval);
		}
		int Execute(void (* accessPortHook)(int) = 0){
			if(interval < 0 && IsPortReady())	{ SetRandomInterval();}
			if(interval == 0)					{ (accessPortHook && AccessHookEnabled()) ? accessPortHook(type) : AccessPort();}
			if(interval >= 0)					{ interval --;}
			return 0;
		}
		///	virtual abstract functions : must be defined in actual class
		virtual bool IsPortReady() = 0;
		virtual void AccessPort() = 0;
		virtual bool AccessHookEnabled() = 0;
	};
	class TerminalViewer
	{
		void Reset(){ str->Reset();}
	public:
		String * str;
		TerminalViewer(){ str = new String;}
		~TerminalViewer(){ if(str) delete str;}
		void PrintView(){
			printf("------------ TERMINAL VIEWER -------------\n");
			printf("%s\n", str->string);
			printf("------------------------------------------\n");
			Reset();
		}
	};
	class InputTerminal : public RandomPeripheral
	{
	public:
		TerminalViewer * termView;
		InputTerminal(CPU * cpu0) : RandomPeripheral(cpu0, "in", 1){ termView = 0;}
		bool IsPortReady(){ return cpu->IsWaitingForInput();}
		virtual void AccessPort(){
			termView->PrintView();
			printf("IN-term [%s] > ", GetPortName());
			int value = _getche();
			printf("\n");
			switch(value){
			case 0x03:	/// ctrl-C
				ABORT_PROGRAM(printf("Program terminated on Input terminal (key = %02x)\n", value););
			case 0x1B:	/// ESC
				cpu->dbg->Command();
				return;
			}
			PutInput(value);
		}
		void PutInput(int value){
			termView->str->Insert(value);
			cpu->SetInput((unsigned char) value);
			EMIT_MESSAGE_5(fprintf, cpu->fplog, 
				"-------------------------------------\n"
				"cpu->input[%s] = 0x%02x (%2d : '%c')\n"
				"-------------------------------------\n"
				, GetPortName(), value, value, value);
			RecordTrace(TT_Data, value);
		}
		virtual bool AccessHookEnabled(){ return true;}
	};
	class OutputTerminal : public RandomPeripheral
	{
	public:
		TerminalViewer * termView;
		OutputTerminal(CPU * cpu0) : RandomPeripheral(cpu0, "out", 0){ termView = 0;}
		virtual int GetInterval(){ return 0;}
		bool IsPortReady(){ return cpu->IsOutputReady();}
		virtual void AccessPort(){
			int value = GetOutput();
			RecordTrace(TT_Data, value);
			termView->str->Insert(value);
		}
		int GetOutput(){
			int value = cpu->GetOutput();
			EMIT_MESSAGE_5(fprintf, cpu->fplog,
				"-------------------------------------\n"
				"cpu->output[%s] = 0x%02x (%2d : '%c')\n"
				"-------------------------------------\n"
				, GetPortName(), value, value, value);
			return value;
		}
		virtual bool AccessHookEnabled(){ return cpu->IsOutputReady();}
	};
};

#endif /// _COMMON_ASM_H_INCLUDED_


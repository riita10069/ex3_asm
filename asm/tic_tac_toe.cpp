
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "ex3_asm.h"

class TicTacToe
{
public:
	enum StateType{
		ST_Start,
		ST_You,
		ST_First,
		ST_Second,
		ST_Win,
		ST_BlockOrFork,
		ST_Others,
	};
#define YOUR_MARK_STR "O"
#define MY_MARK_STR   "X"
#define YOUR_MARK     YOUR_MARK_STR[0]
#define MY_MARK       MY_MARK_STR[0]
	EX3_TerminalSystem * sys;
	int OneGame();
	enum PlayerType
	{
		PT_Program,
		PT_Terminal,
		PT_CPU,
	};
	enum ResultType
	{
		RT_Win,
		RT_Lose,
		RT_Tie,
	};
	class Player
	{
	public:
		TicTacToe * ttt;
		char * name, * myMarkStr, myMark, yourMark;
		char * brd, * p_brd, * n_p_brd;
		int playerType, skillLevel;
		int brd_pos, myLine, myBlock, state;
		int score[3];
		Player(TicTacToe * ttt0, char * n, char * mark, int pt, int level = 0){
			ttt = ttt0;
			brd = ttt->brd;
			name = n;
			myMarkStr = mark;
			myMark = *myMarkStr;
			yourMark = (myMark == 'O') ? 'X' : 'O';
			playerType = pt;
			score[RT_Win] = 0;
			score[RT_Lose] = 0;
			score[RT_Tie] = 0;
			skillLevel = level;
		}
		void PutResult(ResultType rt){ score[rt] ++;}
		void Move(){ (playerType == PT_Terminal) ? MoveFromTerminal() : MoveFromProgram();}
		void MoveFromProgram();
		void MoveFromTerminal();
		int WinBlockForkPos(int pos1, int pos2);
		int WinBlockFork();
		void PutBoard();
		void PutBoardWithPos(int pos);
	} * player1, * player2;
	int CheckWinner();
	void ShowGame(bool forceVerbose);
	int Check3(int pos, int offset);
	void PrintBoard(char * b, int pos);
	void ShowScore();
	char brd[9], history[3 * 9 + 2];
	static char hvIdx[9 * 7 + 1], * h1, * h2, * v1, * v2, * d1, * d2, * d3, * d4;
	int turn, moveCount, verboseFlag, tttState;
#define TIC_TAC_TOE_TEST
	TicTacToe(){
		sys = 0;
		player1 = new Player(this, "player1", "O", PT_Program, 0);
#if defined(TIC_TAC_TOE_TEST)
		player2 = new Player(this, "player2", "X", PT_Program, 1);
#else
		player2 = new Player(this, "player2", "X", PT_Terminal);
#endif
		verboseFlag = (player1->playerType == PT_Terminal || player2->playerType == PT_Terminal);
		tttState = 0;	///	for SoftModel vs. CPU
	}
	void Run(EX3_TerminalSystem * sys0){
		sys = sys0;
		sys->cpu.Reset();
		sys->cpu._IEN = 1;		// enable interrupt
		sys->cpu._IMSK = 0x3;	// enable input/outout device
		while(OneGame()){}
	}
	void RunAgainstCPU(int portType);
};

TicTacToe ticTacToe;

void RunTicTacToe(int portType){ ticTacToe.RunAgainstCPU(portType);}

void EX3_TerminalSystem::RunSoftModel()
{
	ticTacToe.Run(this);
	inTerm.Close();
	outTerm.Close();
	termView.PrintView();
}

void EX3_TerminalSystem::RunCPU_vs_SoftModel()
{
	ticTacToe.sys = this;
	cpu.Reset();
	int inFlag, outFlag, cpuFlag;
	do{
		if(cpu.dbg->Debug()){ break;}
		///	1. simulate components : inputModel, outputModel, my_cpu
		inFlag = inTerm.Execute(RunTicTacToe);
		outFlag = outTerm.Execute(RunTicTacToe);
		cpuFlag = cpu.Execute();
	} while(inFlag + cpuFlag + outFlag == 0);
	inTerm.Close();
	outTerm.Close();
	termView.PrintView();
}

void TicTacToe::PrintBoard(char * b, int pos)
{
	if(brd[pos] == 0)	*b = '0' + pos + 1;
	else				*b = brd[pos];
}

void TicTacToe::ShowGame(bool forceVerbose)
{
	if(!forceVerbose && !verboseFlag){ return;}
	char b[22] = "\n---\n   \n   \n   \n---\n";
	PrintBoard(b + 5,  0); PrintBoard(b + 6,  1); PrintBoard(b + 7,  2);
	PrintBoard(b + 9,  3); PrintBoard(b + 10, 4); PrintBoard(b + 11, 5);
	PrintBoard(b + 13, 6); PrintBoard(b + 14, 7); PrintBoard(b + 15, 8);

	sys->PrintString(b);
	sys->PrintString(history);
	Player * p = (turn == 1) ? player1 : player2;
	sys->PrintString(p->name);
	sys->PrintString(" : ");
	switch(p->state){
	case ST_Start:	sys->PrintString("<START>\n");	break;
	case ST_You:	sys->PrintString("<YOU>\n");	break;
	case ST_First:	sys->PrintString("<FIRST>\n");	break;
	case ST_Second:	sys->PrintString("<SECOND>\n");	break;
	case ST_Win:	sys->PrintString("<WIN>\n");	break;
	case ST_BlockOrFork:	sys->PrintString("<BLOCK OR FORK>\n");	break;
	case ST_Others:	sys->PrintString("<OTHERS>\n");	break;
	default:		sys->PrintString("<?????>\n");	break;
	}
}

int TicTacToe::Check3(int pos, int offset)
{
	char * b = brd + pos;
	if(b[0] == b[offset] && b[0] == b[offset * 2]) return b[0];
	return 0;
}

int TicTacToe::CheckWinner()
{
	int chk;
	if ((chk = Check3(0, 1)) ||	(chk = Check3(3, 1)) || (chk = Check3(6, 1)) ||	///	horizontal
		(chk = Check3(0, 3)) || (chk = Check3(1, 3)) || (chk = Check3(2, 3)) ||	/// vertical
		(chk = Check3(0, 4)) || (chk = Check3(2, 2)))							/// diagonal
		return chk;
	return 0;
}

void TicTacToe::Player::MoveFromTerminal()
{
	while(1){
		ttt->sys->AccessInPort();
		int pos = ttt->sys->cpu._INPR - '1';
		if(pos >= 0 && pos <= 8){
			if(brd[pos]){ ttt->sys->PrintString("\noccupied!! (try again)...\n");}
			else{ PutBoardWithPos(pos); break;}
		}
		else{
			ttt->sys->PrintString("\nInvalid position!! [val = ");
			char value[10];
			sprintf(value, "%02x", ttt->sys->cpu._INPR);
			ttt->sys->PrintString(value);
			ttt->sys->PrintString("] (try again)...\n");
		}
	}
	state = ST_You;
}

int TicTacToe::Player::WinBlockForkPos(int pos1, int pos2)
{
	char b1 = brd[pos1], b2 = brd[pos2];
	int sum = b1 + b2;
	if(sum == myMark * 2)			{ return 1;}	/// my win!!
	else if(sum == yourMark * 2)	{ myBlock ++;}	/// block your win
	else if(sum == myMark)			{ myLine ++;}	/// my line
	return 0;
}

int TicTacToe::Player::WinBlockFork()
{
	/// secure win (my two-in-a-row) or block win (your two-in-a-row)
	myLine = 0;
	myBlock = 0;
	/// horizontal
	if(WinBlockForkPos(h1[brd_pos], h2[brd_pos]) ||
	/// vertical
		WinBlockForkPos(v1[brd_pos], v2[brd_pos]) ||
	/// diagonal
		d1[brd_pos] >= 0 && WinBlockForkPos(d1[brd_pos], d2[brd_pos]) ||
		d3[brd_pos] >= 0 && WinBlockForkPos(d3[brd_pos], d4[brd_pos]))
	{ return 1;}
	if(myBlock > 0 || myLine >= 2 && n_p_brd == 0)
	{ n_p_brd = p_brd;}
	return 0;
}

void TicTacToe::Player::MoveFromProgram()
{
	int i;
	/// 1. opening move
	if(ttt->moveCount == 0){ state = ST_First; p_brd = brd + (rand() % 9); PutBoard(); return;}
	///	2. golden strategies on 1st opposition move
	else if(ttt->moveCount == 1){
		state = ST_Second;
		if(brd[4])
		{ p_brd = brd + d2[(rand() & 0x3) * 2];}	///	center is occupied by you --> next must be a corner
		else if(brd[0] || brd[2] || brd[6] || brd[8])
		{ p_brd = brd + 4;}							///	corner is occupied --> next must be center
		else{	///	edge is occupied --> need to overlap horizontally or vertically
				///	simple strategy is to have the center (but let's try all possibilities)
			brd[(i = 1)] || brd[(i = 3)] || brd[(i = 5)] || brd[(i = 7)];
			p_brd = brd + hvIdx[(rand() & 0x3) * 9 + i];
		}
		PutBoard();
		return;
	}
	///	3. secure my win (three-in-a-row), block your win (two-in-a-row), fork (double two-in-a-row)
	n_p_brd = 0;
	for(brd_pos = 0, p_brd = brd; brd_pos < 9; brd_pos ++, p_brd ++)
	{ if(*p_brd == 0 && WinBlockFork())	{ PutBoard(); state = ST_Win; return;}}		///	my win!!
	if((p_brd = n_p_brd) != 0)			{ PutBoard(); state = ST_BlockOrFork; return;}	/// block a win or fork

	///	4. rest of move strategy
	state = ST_Others; 
	if(skillLevel == 0){	/// skillLevel = 0 : relies on random chance to test opponent's skill
		int remaining = 9 - ttt->moveCount;
		int ii = rand() % remaining;
		for(p_brd = brd; ; p_brd ++){ if(*p_brd == 0){ if(ii == 0){ PutBoard(); return;} ii --;}}
	}
	else{	/// skillLevel > 0 : implement your own strategy here (below is just a dummy...)
		for(p_brd = brd; ; p_brd ++){ if(*p_brd == 0){ PutBoard(); return;}}		///	any other moves....
	}
}

void TicTacToe::Player::PutBoardWithPos(int pos)
{
	p_brd = brd + pos;
	PutBoard();
}

void TicTacToe::Player::PutBoard()
{
	int pos = p_brd - brd;
	if(pos < 0 || pos > 8){ ABORT_PROGRAM(printf("ERROR!!! pos = %d is invalid!!\n", pos));}
	if(*p_brd){ ABORT_PROGRAM(printf("ERROR!!! board[%d] = '%c' is occupied\n", pos, *p_brd));}
	*p_brd = myMark;
	char * h = ttt->history + 3 * ttt->moveCount;
	sprintf(h, "%c%d \n", myMark, pos + 1);
}

/*
	0, 1, 2,
	3, 4, 5,
	6, 7, 8,
*/
char TicTacToe::hvIdx[9 * 7 + 1] = {
	///	h1[9]
	1, 2, 0,
	4, 5, 3,
	7, 8, 6,
	///	h2[9]
	2, 0, 1,
	5, 3, 4,
	8, 6, 7,
	///	v1[9]
	3, 4, 5,
	6, 7, 8,
	0, 1, 2,
	///	v2[9]
	6, 7, 8,
	0, 1, 2,
	3, 4, 5,
	///	d1[9]
	 4,-1, 4,
	-1, 8,-1,
	 4,-1, 4,
	///	d2[9]
	 8,-1, 6,
	-1, 0,-1,
	 2,-1, 0,
	///	d3[9]
	-1,-1,-1,
	-1, 2,-1,
	-1,-1,-1,
	/// d4[4]
	6,
};

char * TicTacToe::h1 = TicTacToe::hvIdx;
char * TicTacToe::h2 = TicTacToe::h1 + 9;
char * TicTacToe::v1 = TicTacToe::h2 + 9;
char * TicTacToe::v2 = TicTacToe::v1 + 9;
char * TicTacToe::d1 = TicTacToe::v2 + 9;
char * TicTacToe::d2 = TicTacToe::d1 + 9;
char * TicTacToe::d3 = TicTacToe::d2 + 9;
char * TicTacToe::d4 = TicTacToe::d3 + 5;

void TicTacToe::ShowScore()
{
	//                        0         1         2         3
	//                        0123456789012345678901234567890123456789
	static char scoreMsg[200] = "P1(------), P2(------), tie(------)        \n";
	char * p1Msg = scoreMsg + 3;
	char * p2Msg = scoreMsg + 15;
	char * tieMsg = scoreMsg + 28;
	static char msg[100];
	sprintf(msg, "%6d", player1->score[RT_Win]);	strncpy(p1Msg, msg, strlen(msg));
	sprintf(msg, "%6d", player1->score[RT_Lose]);	strncpy(p2Msg, msg, strlen(msg));
	sprintf(msg, "%6d", player1->score[RT_Tie]);	strncpy(tieMsg, msg, strlen(msg));
	sys->PrintString(scoreMsg);
}

int TicTacToe::OneGame()
{
	int i, chk;
	///	initialize board
	for(i = 0; i < 9; i ++){ brd[i] = 0;}
	history[0] = 0;
	turn = (rand() & 1);
	player1->state = ST_Start;
	player2->state = ST_Start;
	ShowGame(false);	///	forceVerbose = false
	for(moveCount = 0; moveCount < 9; moveCount ++){
		Player * p = (turn) ? player1 : player2;
		if(verboseFlag){
			sys->PrintString(p->name);
			sys->PrintString("'s turn (");
			sys->PrintString(p->myMarkStr);
			sys->PrintString("):\n");
		}
		p->Move();
		ShowGame(false);	///	forceVerbose = false
		if((chk = CheckWinner())){ break;}
		turn = 1 - turn;
	}
	if(!verboseFlag){ ShowGame(true);}	///	forceVerbose = true
	if(chk == 0)	{ sys->PrintString("It's a tie...\n"); player1->PutResult(RT_Tie); player2->PutResult(RT_Tie);}
	else{
		Player * pw = (turn) ? player1 : player2;
		Player * pl = (turn) ? player2 : player1;
		sys->PrintString(pw->name);
		sys->PrintString(" (");
		sys->PrintString(pw->myMarkStr);
		sys->PrintString(") won!!!\n");
		pw->PutResult(RT_Win);
		pl->PutResult(RT_Lose);
	}
	ShowScore();
	static int count = 0;
	count = (count >= 999) ? 0 : count + 1;
	if(chk || verboseFlag || count == 0){
		sys->PrintString("Do you want to continue ? (y-n) : \n");
		sys->AccessInPort();
		return sys->cpu._INPR == 'y';
	}
	else{
		sys->termView.PrintView();
		return 1;
	}
}

void TicTacToe::RunAgainstCPU(int portType)
{
//	sys->my_cpu.dbg->com = Debugger::CT_Step;
	sys->termView.PrintView();
	printf("tttState = %d, portType = %d\n", tttState, portType);
	int portValue = 0;
	if(portType == 0){ portValue = sys->outTerm.GetOutput();}
	int i;
	if(tttState > 0){
		int chk = CheckWinner();
		if(chk || moveCount == 9){
			if(chk){
				Player * pw = (chk == player1->myMark) ? player1 : player2;
				Player * pl = (pw == player1) ? player2 : player1;
				pw->PutResult(RT_Win);
				pl->PutResult(RT_Lose);
			}
			else{ player1->PutResult(RT_Tie); player2->PutResult(RT_Tie);}
			sys->PrintString(history);
			ShowScore();
			sys->PrintString("Do you want to continue ? (y-n) : \n");
			sys->AccessInPort();
			sys->PrintString("\n");
			if(sys->cpu._INPR == 'y'){ tttState = 0;}
			else{ sys->inTerm.PutInput('q'); return;}
		}
	}
	if(tttState == 0){
		///	initialize board
		for(i = 0; i < 9; i ++){ brd[i] = 0;}
		history[0] = 0;
		turn = (rand() & 1);
		player1->state = ST_Start;
		tttState = (turn) ? 1 : 2;	/// tttState = 1 (myTurn), tttState = 2 (yourTurn)
		moveCount = 0;
		sys->inTerm.PutInput((turn) ? '0' : '1');
		return;
	}
	else if(tttState == 1){	/// myTurn
		/// turn = 1
		player1->Move();
		sys->inTerm.PutInput('1' + player1->p_brd - brd);
	}
	else if(tttState == 2){
		/// turn = 0
		player2->PutBoardWithPos(portValue - '1');
	}
	moveCount ++;
	turn = 1 - turn;
	tttState = (turn) ? 1 : 2;	/// tttState = 1 (myTurn), tttState = 2 (yourTurn)
}

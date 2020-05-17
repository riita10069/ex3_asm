ORG 10
LDA A
STA Num
JP,BSA JugdePrime
LDA PFlag
SZA
BUN SavePrime
LDA Num
CMA
INC
CMA
STA Num
BUN JP 
SavePrime,LDA Num
STA B
HLT

JugdePrime, SYM JugdePrime
//init
CLA
STA KP
//if num==2, True
CLE
LDA Two
CMA
INC
ADD Num
SZA
BUN NotTwo
BUN True
//if num<2, False
NotTwo,SZE
BUN OverTwo
BUN False
//if num>2 and num%2==0, False
OverTwo,LDA Num
STA Dividend
LDA Two
STA Divisor
BSA MOD
LDA Remainder
SZA
BUN SetKPM
BUN False
//if num>2 and num%2==1
//SetKPM
SetKPM,CLE
LDA Num
CMA
INC
ADD KPMM
LDA KPMM
SZE
LDA Num
STA KPM
NotEven,ISZ KP
LDA Two
STA Multiplicand
LDA KP
STA Multiplier
BSA Multiplication
LDA Product
INC
STA TMPP
LDA KPM
CMA
CLE
INC
ADD TMPP
SZE
BUN True
LDA TMPP
STA Divisor
LDA Num
STA Dividend
BSA MOD
LDA Remainder
SZA
BUN NotEven
BUN False

True,CLA
INC
STA PFlag
BUN JugdePrime I
False,CLA
STA PFlag
BUN JugdePrime I


//Multiplication
Multiplication, SYM Multiplication
//init
LDA BitSize
STA KM
CLA
STA Product
//Start Calculation
MulLoop, LDA Multiplier
CIR
STA Multiplier
SZE
BUN ProductIncrease
ProductIncreaseR,LDA Multiplicand
CLE
CIL
STA Multiplicand
ISZ KM
BUN MulLoop
BUN Multiplication I
ProductIncrease,LDA Product
ADD Multiplicand
STA Product
BUN ProductIncreaseR



//MOD
MOD,SYM MOD
//init
CLA
STA Quotient
LDA Dividend
STA Remainder
CLE
CLA
INC
STA KD
// WHILE
LDA Divisor
SZA
BUN DDR
BUN ToERROR

//Double Divisor
DD,ISZ KD
CLE
CIL
BUN DDR

DDR,SNA
BUN DD

STA Divisor
LDA KD
CMA
INC
STA KD
// FOR
FOR,LDA Quotient
CLE
CIL
STA Quotient
LDA Divisor
CMA
INC
ADD Remainder
CME
SZE
BUN ENDIF
STA Remainder
ISZ Quotient
ENDIF,LDA Divisor
CLE
CIR
STA Divisor
ISZ KD
BUN FOR
LDA Divisor
CIL
STA Divisor
BUN MOD I

ToERROR, SYM ToERROR
LDA EMG
ADD CNT_EMG
STA ToERROR
LDA ToERROR I
WaitOut, SKO
BUN WaitOut
OUT
ISZ CNT_EMG
BUN ToERROR
HLT



//DATA
A,DEC 65518
B,HEX 0

//JugdePrime Parameters
KP,HEX 0
KPMM,HEX 100
KPM,HEX 0
Two,HEX 2
TMPP,HEX 0
PFlag,HEX ffff //if Num==PrimeNumber, 1
Num,HEX 0

//MOD
Dividend,HEX 0
Divisor,HEX 0
Quotient,HEX 0
Remainder,HEX 0
KD,HEX 0

//ERROR DATA
CNT_EMG,DEC -6
    	CHR E
		CHR R
		CHR R
		CHR O
		CHR R
    	CHR !
EMG, SYM EMG

BitSize,DEC -16

//Multiplication Parametas
Multiplier,HEX 0
Multiplicand,HEX 0
KM,HEX 0
Product,HEX 0
END

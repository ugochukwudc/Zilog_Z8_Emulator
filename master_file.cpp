All ADC addressing modes

SECTION PROGRAM
ORIGIN %000C
!carry is initially clear
IF C, 2,1
NOP
NOP
!conditionis false, so it wont do the true part NOP's
SRP
#1
SCF
!set the carry flag
IF C, 3,3
! true part is executed, false part is skipped
ADC
R0,R1 !Add 0x01 in R1 to 0x0F in R0
ADC
R0,@R2
!R2 holds 0x13 (R3) - Add 0x81 in R3 to R0
ADC,
%10,%14
!Add 0x80 in R4 to R0
ADC
%10,@%15
!R5 holds 0x16 (R6) - Add 0x10 in R6 to R0
ADC
%10,#%20
!Add 0x20 to R0
ADC
@%17,#%FF
!R7 holds 0x10 (R0) - Add 0xFF to R0

STOP

SECTION REGISTER
ORIGIN
%0010

BVAL
%0F
BVAL
%01
BVAL
%13
BVAL
%81
BVAL
%80
BVAL
%16
BVAL
%10
BVAL
%10

END 

S0 Test of all ADC modes with IF
S10C000C1F79FFFF3101DF1F7FA2
S1140015120113021414101515101610201717FF6F5A
S30B00100F011381801610108A
S9000c

JP/JR and Condition Codes

SECTION PROGRAM
ORIGIN %000C


!Carry flag is initially clear
JP
C,%2000
!Jump to 0x2000 if Carry flag is set
!This jump should NOT be taken
SCF
!Set the carry flag
JP
C,%2000
!Jump to 0x2000 if Carry flag is set
!This time the jump SHOULD be taken

ORIGIN
%2000

!When the JP instruction is executed, the program should run from here

ADC
%10,#1
!Add 1 to R0 - the Zero flag will NOT be set
IF Z, 1,1
!The false part will be executed since the zero flag is not set
JR
Z,%04
!This jump should NOT be taken
NOP
SUB
%10,#2
!Subtract 2 from R0 - should SET Zero flag
IF Z, 1,2
! Zero flag is set so true part should be executed
JR
Z,%04
!IF Zero flag set, jump to PC + 4
!This jump SHOULD be taken
!Jump take over control of flow
!terminates execution of conditionally executed statements

NOP
NOP
NOP
NOP

STOP
!Jump should skip NOPs and execute STOP
END


S0 Jump and condition code + IF test
S10A000C7D2000DF7D2000D0
S11720001610011F65FF6B062610021F666B04FFFFFFFF6F16
S9000C


CALL, RET and table LD


SECTION
PROGRAM
ORIGIN
%000C

!Main program

SRP
#1

!Load test values into R10, R11 and R12
LD
R10,#1
 
LD
R11,#2
LD
R12,#3

!Start the index at 0
LD
R0,#0

CALL
%1000
!Jump to subroutine

STOP

!Subroutine
ORIGIN
%1000

LD
100(R0),R10
!Table load from R10 into 100 + <R0>
INC
R0
!Increment index at R0
LD
100(R0),R11
!Table load from R11 into 100 + <R0>
INC
R0
!Increment index at R0
LD
100(R0),R12
!Table load from R12 into 100 + <R0>

RET


S0 CALL, RET and LD tests
S10E000C3101AC01BC02CC030C00D610006FE7
S10C1000D70A640ED70B640ED70C64AFA9


Timer interrupt and ISR

!Timer ISR at location 0x01000
SECTION PROGRAM
ORIGIN
%1000

LD
%00,#%00
!Write 0 to port 0 to disable timer
IRET
!Return to main

!Main program
ORIGIN
%000C

LD
%FB,%81
!Enable timer interrupts in IMR
LD
%00,%8A
!Write a delay of 20 (2 * 0x0A) to the timer
!in free-running mode
NOP
NOP
NOP
NOP
NOP
NOP

STOP
END


S0 ISR Test
S1041000E60000BFA9
S10D000CE6FB81E6008AFFFFFFFFFFFF6F54


UART interrupt and ISR


/* Load address 0x1000 into UART ISR vector */
memory[PROG][0x0006] = 0x10;
memory[PROG][0x0007] = 0x00;
/* Point stack to 0xFFFF */
write_rm(SPH, 0xFF);
write_rm(SPL, 0xFF);

The code for the program is shown:

!Main program
SECTION
PROGRAM
ORIGIN
%000C

LD
%FB,#%88
!Write 0x88 to IMR to enable IRQ3 interrupts
LD
%03,#%02
!Enable RCV interrupts in PORT 3
LD
%FA,#%00
!Clear IRQ register

JR
%FE
!Infinite loop

! UART reception ISR
SECTION
PROGRAM
ORIGIN
%1000
SRP
#2
!Point to free register block
LD
R2,%F0
!Load SIO data into R0 - receive data
LD
%F0,R2
!Write data back to SIO - transmit data

IRET



S0 UART Test
S10C000CE6FB88E60302E6FA008BFE6F44
S1071000310228F029F0BF2A


Get ARgs 2 test
!Main Program
SECTION PROGRAM
ORIGIN
%000c
LD
%10,#%15
!Load R10 with 15
INC %10
!Increment R10 by 1 = 16
LD
%11,#%10
!Load R81 with 80
DEC @%11
!Decrement R10 by 1 = 15
SWAP @%11
!Swap @R11 = R10 --> 51
CLR %10
!Clear R10 = 0
STOP


S0 Test get_args2
S112000CE610152010E611100111F111B0106F5C
S9000c
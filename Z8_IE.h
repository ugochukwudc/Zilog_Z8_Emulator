/*
 Z8 EMULATOR HEADER FILE
 ECED 3403
 25 JULY 2014
 
 UGOCHUKWU CHUKWU				BOO556842
*/

#ifndef Z8_IE_H
#define Z8_IE_H

#include <stdio.h>
#include <stdlib.h>

//#define flags_test
#define WATCH
//#define VEIW_PROG_MEM
//#define DEBUG
//#define DIAGNOSTIC
#define JUMP
//#define VEIW_STACK
//#define VEIW_MEM
//#define adder_TEST
//#define get_args_2_TEST
//#define get_args_TEST
//#define IE_TEST        /* IE test */

//#define VEIW_CACHE
//#define WB
#define WT
//#define ASSOCIATIVE
#define DIRECT_MAPPING
//#define TEST_CACHE
//#define CONSISTENCY

#define OPC_ARRAY_TEST
#define IF_TEST


#define FALSE     0
#define TRUE      1

#define PD_MEMSZ    65536
#define RM_SIZE        256
#define LINE_LEN 256
#define CACHE_SIZE 32
#define OP_SZ	0x10


#define SIGN(x)     (0x80 & (x))
#define CARRY(x)    (0x100 & (x))
#define SIGN_EXT(x) (SIGN(x) ? (0xff00 | x) : x)
#define ZERO(x)		((x)==0)
#define HALF_CARRY(x)	(0x10 & (x))
#define NEG(x)		(-1* (x))
#define IMR_7(x)	( (x) ? (reg_mem[IMR].content| 0x80) : reg_mem[IMR].content & 0x7F)




/* BYTE manipulations */
#define MSN(x)      ((x) >> 4)		
#define LSN(x)      ((x) & 0x0F)
#define LSBY(x)		(0x00FF & (x))	/* used to extract the least significant byte from a word*/
#define MSBY(x)		((x)>>8)
#define LSB(x)		((x)&0x01) /* used to extract the least significant bit */

/* Special register operations */
#define FLAG_C(x)	((x)<<7 | (reg_mem[FLAGS].content & 0x7F))	/* Set/clear C bit */
#define FLAG_Z(x)   ((x)<<6 | (reg_mem[FLAGS].content & 0xBF))   /* Set/clear Z bit */
#define FLAG_S(x)	((x)>>2 | (reg_mem[FLAGS].content & 0xDF))	/* Set/clear S bit */
#define FLAG_V(x)	((x)<<4 | (reg_mem[FLAGS].content & 0xEF))	/* Set/clear V bit */
#define FLAG_D(x)	((x)<<3 | (reg_mem[FLAGS].content & 0xF7))	/* Set/clear D bit */
#define FLAG_H(x)	((x)<<2 | (reg_mem[FLAGS].content & 0xFB))	/* Set/clear H bit */
#define RPBLK       (reg_mem[RP].content << 4)                   	/* For RP | working register */
#define SPLOC		((reg_mem[P01M].content & 0x04)>>2) 		/* 0 when SP is in data mem, 1 when SP is in reg_mem */
#define SP16		((reg_mem[SPH].content<<8)|reg_mem[SPL].content)		/* 16 bit stack pointer */
#define SP			(SPLOC? reg_mem[SPL].content : SP16)			/* pointer to top of stack */

/* 8 bit register operations */
#define BIT_7(x)	((x>>7) & 0x01)
#define BIT_6(x)	((x>>6) & 0x01)
#define BIT_5(x)	((x>>5) & 0x01)
#define BIT_4(x)	((x>>4) & 0x01)
#define BIT_3(x)	((x>>3) & 0x01)
#define BIT_2(x)	((x>>2) & 0x01)
#define BIT_1(x)	((x>>1)	& 0x01)
#define BIT_0(x)	((x)	& 0x01)


/* Bus signals */
enum RDWR           {RD, WR}; 
enum MEM            {PROG, DATA}; //PROG = 0, DATA =1

/* Loader signals */
enum SREC_ERRORS   {MISSING_S, BAD_TYPE, CHKSUM_ERR};

/* Loader dialogues */
char *srec_diag[] = {
"Invalid srec - missing 'S'",
"Invalid srec - bad rec type",
"\nInvalid srec - chksum error"};


#define PRIVATE    static
#define BYTE       unsigned char
#define WORD       unsigned short
#define CARRY_BYTE  unsigned short    /* Use to determine if carry set */

#define IRQ_MASK  0x3F             /* IRQ bits 0..5 in IMR */
enum IMR_BITS     {IRQ0 = 0x01, IRQ1 = 0x02, IRQ2 = 0x04, IRQ3 = 0x08, IRQ4 = 0x10, IRQ5 = 0x20, INT_ENA = 0x80};

/* Program and data memory */
//extern BYTE memory[][]; 

/* cache memory */
struct state
{
	unsigned dirty:1; // dirty bit
	unsigned lru:5; //least recently used
};

struct cache_line
{
	WORD addr; // target address in primary memory
	BYTE content; // content of mem
	struct state cls; // state of the cache_line 
};

/* Register memory */
enum DEV_EM_IO    {REG_RD, REG_WR};

struct reg_mem_el
{
BYTE content;
int (*option)(BYTE, enum DEV_EM_IO);       
};

extern struct reg_mem_el reg_mem[];

/* Devices in register memory */
#define PORT0   0x00
#define PORT1   0x01
#define PORT2   0x02
#define PORT3   0x03
#define SIO     0xF0
#define TMR		0xF1
#define T1		0xF2
#define PRE1	0xF3
#define T0		0xF4
#define PRE0	0xF5
#define P2M		0xF6
#define P3M		0xF7
#define P01M    0xF8
#define IPR     0xF9
#define IRQ     0xFA
#define IMR     0xFB

/* Special reg_mem addresses */
#define FLAGS     0xFC
#define RP        0xFD 
#define SPH       0xFE
#define SPL       0xFF

/* Register memory functions */
extern void reg_mem_init();
extern void reg_mem_device_init(BYTE, int (*)(BYTE, enum DEV_EM_IO), BYTE);

/* Device entry points */
extern int TIMER_device(BYTE, enum DEV_EM_IO);
extern int UART_device(BYTE, enum DEV_EM_IO);

/* UART bits */
#define TXDONE     0x04

#endif

/******************************HEADER FILE***********************************/

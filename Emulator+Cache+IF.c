#include "Z8_IE.h"

/*
 Z8 Register Memory
 - Emulate register memory
 - Handle all valid registers (0..7F, E0..EF, F0..FF)
 - Register memory can be read/written
 - Accessing any register associated with a device will cause the device 
   emulator to be called with the register number and a RD/WR indication
   
 ECED 3403
 17 June 2014
*/

/* start and stop for mem display */
#define START 0x10;
#define STOP 0x20;
///////////////////////////////////

/* Define register access */
#define RM_RDWR     (int(*)(BYTE, enum DEV_EM_IO))(-1)
#define RM_RDONLY   (int(*)(BYTE, enum DEV_EM_IO))(-2)
#define RM_USERP    (int(*)(BYTE, enum DEV_EM_IO))(-3)

/* Register memory */
struct reg_mem_el reg_mem[RM_SIZE];

void reg_mem_init()
{
/* Initialize register memory to default values */
int i;

/* Assume everything is RDWR - contents is unknown */
for(i=0; i<RM_SIZE; i++)
     reg_mem[i] . option = RM_RDWR;

/* 80..DF - not supported - make RD only and contents 0xFF */
for(i=0x80; i<0xE0; i++)
{
     reg_mem[i] . content = 0xFF;
     reg_mem[i] . option = RM_RDONLY;
}

/* E0..EF - special code to indicate RP shift and prefix */
for(i=0xE0; i<0xF0; i++)
     reg_mem[i] . option = RM_USERP;

}

BYTE read_rm(BYTE reg_no)
{
/* Read specified byte and return value (RM_RDWR or RM_RDONLY)
   Extract working register and prefix RP (RM_USERP)
   Read byte and call device emulator function (none of the above)
   NOTE: RP must not equal 0x0E
*/

if (reg_mem[reg_no] . option == RM_USERP)
     reg_no = (reg_mem[RP] . content << 4) | (reg_no - 0xE0);
     
/* option requires a cast to an int because switch doesn't support pointers */
switch((int) reg_mem[reg_no].option)
{
case (int) RM_RDWR:
case (int) RM_RDONLY:
     return reg_mem[reg_no] . content;

default:
     /* Call device emulator: option(reg_no, REG_RD)
        Device can update register contents
        Return device contents (after update)
     */
     reg_mem[reg_no] . option(reg_no, REG_RD);
     return reg_mem[reg_no] . content;
}
}

void write_rm(BYTE reg_no, BYTE value)
{
/* Write value to register (RM_RDWR)
   Do nothing (RM_RDONLY)
   Extract working register and prefix RP then write value (RM_USERP)
   Write value and call device emulator function (none of the above)
*/
if (reg_mem[reg_no] . option == RM_USERP)
     /* E0..EF correct to RP | regno */
     reg_no = (reg_mem[RP] . content << 4) | (reg_no - 0xE0);

switch((int)reg_mem[reg_no] . option)
{
case (int)RM_RDWR:
     reg_mem[reg_no] . content = value;
     break;

case (int)RM_RDONLY:
     break;

default:
     /* Call device emulator: option(reg_no, REG_WR)
        Device can access register contents
     */
     reg_mem[reg_no] . content = value;
     reg_mem[reg_no] . option(reg_no, REG_WR);
}

}

void reg_mem_device_init(BYTE reg_no, 
                         int (*dev_emulator)(BYTE, enum DEV_EM_IO), 
                         BYTE value)
{
/* Initialize register memory associated with an external device to 
   initial value (contents) and option (dev_emulator)
*/
reg_mem[reg_no] . content = value;
reg_mem[reg_no] . option = dev_emulator;
}
/************************REGISTER MEMORY **********************/

PRIVATE WORD tdc;       /* Timer delay count */
PRIVATE BYTE treload;   /* Timer reload? T|F */
PRIVATE BYTE trunning;  /* Timer running? T|F */

int TIMER_device(BYTE reg_no, enum DEV_EM_IO cmd)
{
/* Emulate timer device port:
   - called after Port 0 has been read or written (reads are ignored)
   - write to port implies change - check value
   - interrupts will occur only if IMR(0) is set
*/
#ifdef DIAGNOSTIC
printf("timer emulator has been called \n");
#endif
if (cmd == REG_RD)
     return;

/* Write to port -- check port value */
if (reg_mem[PORT0] . content == 0)
     /* Timer off */
     trunning = FALSE;
else
{
     /* Non-zero write - extract tdc and treload */
     tdc = (reg_mem[PORT0] . content & 0x7F) << 1;
     treload = (reg_mem[PORT0] . content & 0x80) == 0x80;
     trunning = TRUE;
}

#ifdef DIAGNOSTIC
printf("tdc: %x treload: %x trunning: %x\n", tdc, treload, trunning);
printf("Port[0]: %02x\n", reg_mem[PORT0] . content);
#endif
}

void TIMER_check()
{
/* System clock has "ticked"
   - decrement timer if running
   - when timer reaches zero, reload if required otherwise stop timer
*/
if (!trunning)
{
#ifdef DIAGNOSTIC 
printf("not running!! \n");
#endif
     return;
}

tdc--;
#ifdef DIAGNOSTIC
printf("TIMER COUNT IS %x \n", tdc);
#endif
if (tdc == 0)
{
     reg_mem[IRQ] . content |= IRQ0;  /* Signal IRQ0 - timer interrupt */
     if (treload)
          tdc = (reg_mem[PORT0] . content & 0x7F) << 1;
     else
          trunning = FALSE;
}

}

/**********************************TIMER FILE *******************************/
/*
 Z8 Machine emulator with interrupt emulation - skeletal machine
 ECED 3403
 17 June 2014
*/
/* Author : Ugochukwu Chukwu				student Number: B00556842 */


/* Memory arrays */
BYTE memory[2][PD_MEMSZ];        /* PROG and DATA */


/* Hidden registers */
WORD pc;              /* Program counter */
WORD sp; 			  /* stack pointer 	 */
BYTE intr_ena;        /* Interrupt status */

unsigned long sys_clock; /* system clock */

/* Flag bits */ 
BYTE carry;
BYTE sign;
BYTE overflow;
BYTE zero;
BYTE half_carry;
BYTE decimal_adjust;


/*
  S19 srecord-extraction program
  - extract bytes from an s-record in a file using sscanf()
  - sscanf() appears to require ints and unsigned ints only
  - can be used to verify assignment 1 records or act as part 
    of the loader in assignment 2
  - turn on diagnostics with define DEBUG
  - sets the initial value of the program counter

*/


char srec[LINE_LEN];
FILE *fp;
void srec_error(enum SREC_ERRORS serr)
{
/* Display diagnostic and abort */
printf("%s: %s\n", srec_diag[serr], srec);
getchar();
fclose(fp);
exit(0);
}

void loader (int argc, char *argv[])
{
	/* Read and process s-records */
/* Record access variables */
unsigned pos;
unsigned int i;
/* Record fields */
int srtype;                   /* byte 1 - 0..9 */
unsigned int length;          /* byte 2 */
unsigned int ah, al, address; /* bytes 3 and 4 */
signed char chksum;           /* checksum tally */
unsigned char byte;            /* bytes 5 through checksum byte */
unsigned char temp[LINE_LEN];	/*temp storage to hold LINE_LEN bytes */

if (argc != 2)
{
     printf("Format: parse filename\n");
     exit(0);
}

if ((fp = fopen(argv[1], "r")) == NULL)
{
     printf("No file specified\n");
     exit(0);
}

while (fgets(srec, LINE_LEN, fp) > 0)
{
#ifdef DEBUG
printf("\nsrec: %s\n", srec);
#endif
     /* Should check min srec length */
     if (srec[0] != 'S')
          srec_error(MISSING_S);        
     /* Check srec type */
     srtype = srec[1] - '0';
     if (srtype < 0 || srtype > 9)
          srec_error(BAD_TYPE);
	 if(srtype == 9) /* S9 recs define the initial value of the program counter */
     {
     	sscanf(&srec[2], "%2x%2x", &ah, &al);
     	address = ah << 8 | al;
     	pc = address;
     }
     else if(srtype ==0) /* name of srec*/
     {
     	printf("%s \n", srec+3);
     }
	 else{
            
	     /* Get length (2 bytes) and two address bytes */
	     sscanf(&srec[2], "%2x%2x%2x", &length, &ah, &al);
	     address = ah<<8|al;
	#ifdef DEBUG
	printf("len: %02x ah: %02x al: %02x\n", length, ah, al);
	printf("Address: %04x\n", address);
	#endif
	     	chksum = length + ah + al;
		#ifdef DEBUG 
		printf("checksum len + al + ah : %x \n", chksum);
		#endif
	     
		     length -= 3; /* Ignore length, address and checksum bytes */
		     pos = 8;     /* First byte in data */
		     /* Read data bytes */
		     for (i=0x00; i<length; i++)
		     {
		          sscanf(&srec[pos], "%2x", &byte);
		          temp[i]=byte;
                  chksum += byte;
		          
		#ifdef DEBUG
		printf("Byte %02x ", byte);    /* Convert two chars to byte */
		printf("temp[i]: %x, i= %02x \n", temp[i], i);
		#endif
			      pos += 2;    /* Skip 2 chars (1 "byte") in srec */
		     }
		     /* Read chksum byte */
		     sscanf(&srec[pos], "%2x", &byte);
		     
		#ifdef DEBUG
		printf("Check sum before :%x \n", chksum);
		unsigned short chk;
		chk = ~chksum;
		printf("Check sum value should be:%x \n", chk);
		printf("chk + byte = %2x \n", chk+byte);
		#endif
			
		     chksum += byte;
		#ifdef DEBUG
		printf("checksum after : %x \n", chksum);
		#endif
		     /* Valid record? */
		     if (chksum != -1)
		          srec_error(CHKSUM_ERR);
		     else{
		     	/* Valid record !
				 write values of temp to memoryory */
			switch(srtype)
			     {
			     	case 1: /* s1 program_memory */
			     	case 2: /* s2 data memoryory */
			     		for(i=0; i<length; i++)
			     		{
			     			/* address still holds starting loc for loading 
                             srtype -1 = 0 for PROG memory
                                       = 1 for DATA memory */
			     			memory[(srtype-1)][address++]= temp[i];
			     		}
			     		#ifdef DEBUG
			     		if(srtype == 1){
			     			printf("Program memory \n");
			     		}
			     		else {
			     			printf("Data memory \n");
			     		}
			     		#endif
			     		break;
			     	case 3: /* s3 */
			     		for(i=0; i<length; i++)
			     		{
			     			/* address still holds starting loc for loading */
			     			/* reg_memory initiliazer called first */
			     			 reg_mem[address++].content = temp[i];
			     		}
			    	#ifdef DEBUG 
			    	printf("Register memory \n");
			    	#endif
			     	break;
			     }
			#ifdef DEBUG
			address-=length;
			printf("start printing from address is %x \n", address);
			if(srtype==3)
			{
                for(i=0; i<length; i++){
                    printf("contents of memory loc %4x is: %2x \n", address, reg_mem[address].content);
                    address++;
                }
            }
            else{
    			for(i=0; i<length; i++)
    			{
    				printf("contents of memory loc %4x is: %2x \n", address, memory[(srtype-1)][address]);
    				address++;
    			}
            }
			#endif
			}
	}
}
	 
#ifdef DEBUG
printf("pc is %4x \n", pc);
printf("\n File read and succesfully loaded - no errors detected\n");
#endif
fclose(fp);
getchar();
}


 /* To display the contents of register memory
 from 0 - 2f and f0- ff */
void disp_reg_mem()
{
BYTE i;
BYTE start = START;
BYTE stop =  STOP;
while(start<stop)
{
for (i=start; i<(start+0x10); i++)
{
	if (i==start)
	printf(" \n start: %2x \t", i);
	printf("%2x ", reg_mem[i].content);
}
start +=0x10;
}
start =0xf0;
printf("\n start: %2x \t", start);
while (start<0xff)
{
	printf("%2x ", reg_mem[start].content);
	start++;
}
printf("%2x", reg_mem[start].content);
printf("\n");
getchar();
}


void bus(WORD mar, BYTE *mbr, enum RDWR rdwr, enum MEM mem)
{
/* Bus emulation:
   - mar - 16-bit memory address register (W)
   - mbr - 8-bit memory buffer register (R|W)
   - rdwr - 1-bit read/write indication (RD or WR)
   - mem - 1-bit memory to access (PROG or DATA)
   Does not check for valid rdwr or mem values
*/

if (rdwr == RD)
   *mbr = memory[mem][mar];
else /* Assume WR */
   memory[mem][mar] = *mbr;

#ifdef DIAGNOSTICS
printf("Bus: %04x %01x %01x %01x\n", mar, *mbr, rdwr, mem);
#endif

}

/********IF ADDITION *************************************/
BYTE opcode_size[OP_SZ][OP_SZ]; // OPCODE SIZE ARRAY

/* returns no of bytes occupied by operands of a particular opcode with
MSN = hi and LSN = lo */
BYTE op_size(BYTE hi, BYTE lo)
{
	BYTE ans;
	switch (lo)
	{
		case 0x04:
			if(hi==0x0D)
				ans=1;
			else
				ans =2;
		break;
		
		case 0x05:
		case 0x06:
		case 0x07:
		case 0x0D:
			ans = 2;
		break;
		case 0x0F:
			ans =0;
		break;
		default:
			ans =1;
		break;
	}
	
	return ans;
}

/* initializes the opcode_size array called by the mainline program */
opc_size_init()
{
BYTE h,l;
for (h=0; h<=0x0f; h++)
{
	for(l=0; l<=0x0f; l++)
	{
		opcode_size[h][l]=op_size(h,l);
	}
}
#ifdef OPC_ARRAY_TEST
for (h=0; h<=0x0f; h++)
{
	for(l=0; l<=0x0f; l++)
	{
		printf( "%2x ",opcode_size[h][l]);
	}
	printf("\n");
}
#endif
}

/***************************************************************************/


struct cache_line cache_mem[CACHE_SIZE]; // cache mem

/* cache called on access to program memory
assertains if target destination is in the cache
YES - *HIT* returns destination contents and update cache
NO - *MISS* store contents of cache_line, retrieve target from primary memory
			update cache, and return to CPU
*/ 
void cache (WORD mar, BYTE* mbr, enum RDWR rdwr)
{
	int i;
	BYTE cache_addr = 0; // address of the target address in cache memory
	struct cache_line temp; // holds cache_mem element to be read/ returned
	
	#ifdef DIRECT_MAPPING
	// formula maps a specific primary address to thesame cache_addr every time
	cache_addr = mar%CACHE_SIZE;
	#ifdef TEST_CACHE
	printf("Cache_addr is: %x \n", cache_addr);
	#endif
	#endif 
	
	#ifdef ASSOCIATIVE
	/* do a linear search through the cache_mem
	if found assign to temp,
	else assign the least recently used cache_line to temp */
	for(i=0, i<CACHE_SIZE; i++)
	{
		if(cache_mem[i].addr == mar)
		{
			//retrieve index of cache_line
			cache_addr = i;
			break; // break out of for loop once match is found
		}
		else if (cache_mem[i].cls.lru == 0x00){
			//retrieve index of least recently used
			cache_addr = i;
		}
	}
	#ifdef TEST_CACHE
	printf("Cache_addr is: %x \n", cache_addr);
	#endif
	#endif
	
	/* retrieve cache line */
	temp = cache_mem[cache_addr];
	
	#ifdef ASSOCIATIVE
	//decrement all younger elements and make cache_line most recently used 
	for(i=0; i<CACHE_SIZE; i++)
	{
		if(cache_mem[i].cls.lru > temp.cls.lru)
		{
			cache_mem[i].cls.lru--;
		}
	}
	temp.cls.lru = 0x31; // assign most recent
	#endif

	if(cache_mem[cache_addr].addr == mar) /* HIT */
	{
		#ifdef TEST_CACHE
		printf("Primary addr: %x , Cache_mem[].addr: %x; HIT \n", mar, cache_mem[cache_addr].addr);
		#endif
		
		if(rdwr==RD)
		{
			*mbr = temp.content; // returns value of cache_mem[cache_addr]
			cache_mem[cache_addr] = temp; // for consistency lru
		}
		
		else{ //assumes WR
			temp.content = *mbr;
			#ifdef WB
			/* write back */
			temp.cls.dirty = 1; // indicate it has been written to
			#endif
			#ifdef WT
			/* write through */
			/* write to primary memory for cache consistency */
			bus(mar, mbr, WR, PROG);
			#endif
			cache_mem[cache_addr]=temp; // writes value to cache_mem
			#ifdef CONSISTENCY
			printf(	"             addr cont  lru dirty \n"
					"Primary mem: %4x  %2x 	--  -- \n", mar, memory[PROG][mar]);
			printf(	"Cache mem  : %4x  %2x   %2x  %1x ", temp.addr, temp.content, temp.cls.lru, temp.cls.dirty);
			#endif
		}
	}
	else{ /* MISS */
		#ifdef TEST_CACHE
		printf("Primary addr: %x , Cache_addr: %x; MISS \n", mar, cache_mem[cache_addr].addr);
		#endif
		
		#ifdef WB
		if(temp.cls.dirty == 1)
		{
			/* cache line has been written to 
			write back to primary mem to maintain cache consistency */
			bus(temp.addr, &temp.content, WR, PROG);
			#ifdef CONSISTENCY
			printf("Write back to primary memory for consistency \n ");
			printf(	"Cache mem  : %4x  %2x   %2x  %1x ", temp.addr, temp.content, temp.cls.lru, temp.cls.dirty);
			printf(" Primary mem: %4x  %2x \n", temp.addr, mem[PROG][temp.addr]);
			#endif
		}
		#endif
		/* if RD */
		if (rdwr == RD)
		{
			/* retrieve mem contents from primary memory */
			bus(mar, &temp.content, RD, PROG);
			temp.addr = mar; // assign cache_line address<- primary mem addr
			temp.cls.dirty = 0;
			/* temp.cls.lru set to most recent in #ifdef ASSOCIATIVE above */
			*mbr = temp.content;
			cache_mem[cache_addr]= temp; // write new cache line to cache_mem
		}
		else{ /* assume WR */
			/* overwrite cache loc with new values */
			temp.addr = mar;
			temp.content = *mbr;
			#ifdef WB 
			/* indicate cache location has been written to */
			temp.cls.dirty = 1;
			#endif
			#ifdef WT
			/* write through to primary memory */
			bus(temp.addr, &temp.content, WR, PROG);
			#endif
			/* store new cach_line in cache mem  */
			cache_mem[cache_addr]= temp;
		}
		#ifdef CONSISTENCY
		printf(	"             addr cont  lru dirty \n"
				"Primary mem: %4x  %2x 	--  -- \n", mar, memory[PROG][mar]);
		printf(	"Cache mem  : %4x  %2x   %2x  %1x ", temp.addr, temp.content, temp.cls.lru, temp.cls.dirty);
		#endif
	}
#ifdef DIAGNOSTICS
printf("CACHE: %04x %01x %01x %01x\n", mar, *mbr, rdwr, PROG);
#endif
	
}	

void veiw_cache (void)
{
	int i;
	printf ("loc addr cont  lru dirty \n");
	for (i = 0; i<CACHE_SIZE; i++)
	{
		printf("%2d  %4x  %2x   %2x   %1x \n", i, cache_mem[i].addr, cache_mem[i].content, cache_mem[i].cls.lru, cache_mem[i].cls.dirty);
	}
}

void cache_mem_init()
{
	BYTE i;
	for (i=0; i<CACHE_SIZE; i++)
	{
		cache_mem[i].addr = 0xffff;
		cache_mem[i].content = 0xff;
		cache_mem[i].cls.dirty = 0;
		cache_mem[i].cls.lru = i;
	}
}

BYTE prog_mem_fetch()
{
/* Call bus to access next location in program memory
   Use PC -- increment PC after access (points to next inst or next byte in 
   current instruction)
   Returns location as byte (mbr)
*/
BYTE mbr;  /* Memory buffer register */

////////////changes//////////////////
cache(pc, &mbr, RD);
////////////////////////////////////
//bus(pc, &mbr, RD, PROG);

pc = pc + 1;
#ifdef PCwatch
printf("Program counter is : %x \n", pc);
#endif
return mbr;
}

BYTE read_dm(WORD addr)
{
/* Call bus to access addr location in the data memory 
return content of the memory location as BYTE 
*/
BYTE mbr; /* Memory buffer register */

bus(addr, &mbr, RD, DATA);

return mbr;
}

BYTE write_dm(WORD addr, BYTE dat)
{
/*Call bus to access addr location in data memory
 writes a BYTE dat to it
 returns the byte that was written for consistency 
 and error checking */
 BYTE mbr;
 
 bus(addr, &dat, WR, DATA);
 bus(addr, &mbr, RD, DATA);
 
return mbr;	
}


BYTE read_pm(WORD addr)
{/* calls bus to read addr location in program memory
returns a BYTE which is the memory location content
*/
 
BYTE mbr;
/////////////changes/////////////
cache(addr, &mbr, RD);
////////////////////////////////

//bus(addr, &mbr, RD, PROG);
return mbr;
}


/* calls bus to write value to addr in program memory 
returns the byte that was written for consistency and error checking
*/
BYTE write_pm(WORD addr, BYTE value)
{
BYTE mbr;
///////////changes////////////////////
cache(addr, &value, WR);
cache(addr, &mbr, RD);
/////////////////////////////////////
//bus(addr, &value, WR, PROG);
//bus(addr, &mbr, RD, PROG);
return mbr;
}

/* get address of destination in register memory
and returns the dest addr and the source value 
Increments system clock by number_of_cycles as per addressing mode
number_of_cycles is thesame for all instructions that call the function 
*/
BYTE get_args(BYTE lnib, BYTE* dest)
{
BYTE src;
BYTE dst;
#ifdef get_args_TEST
printf("Get ARGS called \n");
#endif
switch(lnib){
case 0x02: /* r_r */
dst= prog_mem_fetch();
src =dst;
src = read_rm(RPBLK|(LSN(src))); // source's value is the content of reg_mem
dst = RPBLK|MSN(dst); // destination's address is the working register
/* increment system clock */
sys_clock += 0x06; /* increments sys_clock by number of cycles = 0x06 */
break;

case 0x03: /* r_Ir*/
dst = prog_mem_fetch();
src=dst;
src = read_rm(RPBLK|(read_rm(RPBLK|LSN(src)))); // source's value is the content of the location the adrress is pointing to
dst = RPBLK|MSN(dst); // destination address is a working register
/* increment system clock */
sys_clock += 0x06; // 6 cycles 
break;

case 0x04: /* R_R */
src = read_rm(prog_mem_fetch());
dst = prog_mem_fetch();
/* increment system clock */
sys_clock += 0x0A; // 10 cycles
break;

case 0x05: /* R_IR */
src = read_rm(read_rm(prog_mem_fetch()));
dst = prog_mem_fetch();
/* increment system clock */
sys_clock += 0x0A; // 10 cycles
break;

case 0x06: /* R_NUM*/
dst = prog_mem_fetch();
src = prog_mem_fetch();
/* increment system clock */
sys_clock +=0x0A; // 10 cycles
break;

case 0x07: /* IR_NUM */
dst = read_rm(prog_mem_fetch());
src = prog_mem_fetch();
/* increment system clock */
sys_clock +=0x0A; // 10 cycles
break;
}

#ifdef get_args_TEST
printf("Dst reg : %2x, src value: %2x \n", dst, src);
#endif

*dest = dst; // pass location of dst in reg_mem
return src;	// return the value of source obtained from reg_mem
}

/*	get dst address for instructions with 0 and 1 low nib DEC, RLC, SWAP etc 
returns dst index
Increments sys_clock by base value of 6 cycles	*/
BYTE get_args_2(BYTE lnib)
{
	BYTE ans;
	#ifdef get_args_2_TEST
	printf(" low nibble is : %2x \n", lnib);
	#endif
	switch (lnib)
	{
		case 0x00:	// R
		ans = prog_mem_fetch();
		break;
		
		case 0x01:	//IR
		ans = read_rm(prog_mem_fetch());
		break;
	}
	/* increment system clock */
	sys_clock +=6; // 6 cycles 
#ifdef get_args_2_TEST
printf("Dst reg : %2x \n", ans);
#endif
	return ans;
}



/* function does the addition of a dst and src 
 all flag manipulations are performed in the function
 a1 holds destination value
 a2 holds source value 
 a3 is 1, for ADD with carry OR 0 for not with carry 
 */
BYTE adder (BYTE a1, BYTE a2, BYTE a3)
{
	BYTE ans;
	CARRY_BYTE temp;
	temp = a1+a2+a3;
	ans = temp;	
	
	/************* SET FLAGS ***************/
	/* carry */
	carry = (CARRY(temp)>>8); // determine if there is a carry */
	write_rm(FLAGS, FLAG_C(carry));		// set carry 
	
	/* zero */
	zero = ZERO(ans);
	write_rm(FLAGS, FLAG_Z(zero));
	
	/* sign */
	sign = SIGN(ans);
	write_rm(FLAGS, FLAG_S(sign));
	
	/* overflow */
	overflow = (SIGN(a1) != sign);
	write_rm(FLAGS, FLAG_V(overflow));
	
	/* decimal adjust */
	decimal_adjust=0;
	write_rm(FLAGS, FLAG_D(decimal_adjust));
	
	/* half carry */
	half_carry = (HALF_CARRY(ans)!=HALF_CARRY(a1));
	write_rm(FLAGS, FLAG_H(half_carry));
	
#ifdef adder_TEST
	printf ("result of %2x + %2x (with carry = %2x): %x \n", a1, a2, a3, ans);
	printf ("FLAGS : %x \n", read_rm(FLAGS));
#endif
	return ans;
}

/* function does subtraction of src from dst
all flag manipulations are done in the function
a1 holds the value of destination 
a2 holds the value of src 
a3 holds carry for subtractions with carry 
returns the result of subtraction
*/
BYTE subber (BYTE a1, BYTE a2, BYTE a3)
{
	BYTE ans;
	CARRY_BYTE temp;
	temp = a1 - a2 - a3;
	ans = temp;
	#ifdef flags_test
	printf ("result : %x \n", ans);
	#endif
	/************* SET FLAGS ***************/
	/* carry */
	carry = !(CARRY(temp)>>8); // determine if there is a carry
	
#ifdef flags_test
printf("carry is %x \n", carry);
#endif
	write_rm(FLAGS, FLAG_C(carry));		// set carry 
	
	/* zero */
	zero = ZERO(ans);
	write_rm(FLAGS, FLAG_Z(zero));
#ifdef flags_test
printf("zero is %x \n", zero);
#endif

	/* sign */
	sign = SIGN(ans);
	write_rm(FLAGS, FLAG_S(sign));
#ifdef flags_test
printf("sign is %x \n", sign);
#endif
	
	/* overflow */
	overflow = (SIGN(a1) != sign);
	write_rm(FLAGS, FLAG_V(overflow));
#ifdef flags_test
	printf("Overflow is : %2x \n", overflow);
#endif
	/* decimal adjust */
	decimal_adjust=0x01;
	write_rm(FLAGS, FLAG_D(decimal_adjust));
#ifdef flags_test
printf ("decimal adjust is : %x \n", decimal_adjust);
#endif
	
	/* half carry */
	half_carry = !(HALF_CARRY(ans)!=HALF_CARRY(a1));
#ifdef flags_test
printf("half carry is : %x \n", half_carry);
#endif

	write_rm(FLAGS, FLAG_H(half_carry));	
#ifdef flags_test
printf( "FLAGS : %x \n", read_rm(FLAGS));
#endif
	return ans;
}

/* adjust_dec function called by the decimal adjust (DA) instruction
sets all affected flags in function 
a1 holds the destination value 
returns the value of the adjusted dst */
BYTE adjust_dec(BYTE a1)
{
BYTE ans;
CARRY_BYTE temp;
BYTE numb; // number to be added to BYTE 

switch (half_carry){
case 0x00:
if(LSN(a1)<=0x09){ //unsigned char no need to check for lower bound of 0
	switch(carry){
	case 0x00: 
	if (MSN(a1)<=0x09)
	{
		numb =0x00;
	}else{ /*MSN : A-F */
		numb =0x06;
	}
	break;
	
	case 0x01:
	if(MSN(a1)<=0x02){
		numb =0x60;
	}
	else if(MSN(a1)>=0x07 && MSN(a1)<=0x0F){
		numb =0XA0;
	}
	break;
	}
}
else /* LSN : A-F*/
{
	switch(carry){
		case 0x00:
		if(MSN(a1)<=0x08){
			numb = 0x06;
		}
		else{
			numb = 0x66;
		}
		break;
		case 0x01:
		if(MSN(a1)<=0x02){
			numb =0x66;
		}
		break;
	}
}
break;

case 0x01:
if(LSN(a1)<=0x03){	
switch (carry){	
case 0x00:
if(MSN(a1)<=0x09){
	numb =0x06;
}
else{
	numb = 0x66;
}
break;

case 0x01:
if(MSN(a1)>=0x03){
	numb = 0x66;
}
break;	
}
}

else if(LSN(a1)>= 0x06 && LSN(a1)<=0x0F){
switch (carry){
case 0x00:
if (MSN(a1)<=0x08){
	numb = 0xFA;
}
break;
case 0x01:
if(MSN(a1)>= 0x06 && MSN(a1)<= 0x0F){
	numb = 0x9A;
}
break;
}
}
break;
}

temp = a1 + numb;
ans =  temp;

/************* SET FLAGS ***************/
/* carry */
carry = (CARRY(temp)>>8); // determine if there is a carry */
write_rm(FLAGS, FLAG_C(carry));		// set carry 

/* zero */
zero = ZERO(ans);
write_rm(FLAGS, FLAG_Z(zero));

/* sign */
sign = SIGN(ans);
write_rm(FLAGS, FLAG_S(sign));

return ans;
}

/* function handles condition codes and returns 0x01 and 0x00 for true and false respectively
a1 holds the condition code value 
called by JP and JR to check condition
*/
BYTE cond_handler(BYTE a1)
{
BYTE ans;
/* extracting the bits */
carry = (read_rm(FLAGS) & 0x80) >>7;
zero  = (read_rm(FLAGS) & 0x40) >>6;
sign  = (read_rm(FLAGS) & 0x20) >>5;
overflow = (read_rm(FLAGS) & 0x10) >>4;
switch(a1){
	case 0x00: /* always false */
	ans = 0x00;
	break;
	
	case 0x01: /* (S XOR V) = 1*/
	ans = (sign^overflow)==0;
	break;
	
	case 0x02: /*(Z OR (S XOR V))=1 */
	ans = (zero|(sign^overflow))==1;
	break;
	
	case 0x03: /* (C OR Z) =1*/
	ans = ((carry | zero)==1);
	break;
	
	case 0x04: /* V=1 */
	ans = (overflow ==1);
	break;
	
	case 0x05: /* S = 1*/
	ans=(sign==1);
	break;
	
	case 0x06: /* Z ==1*/
	ans = (zero==1);
	break;
	
	case 0x07: /* C==1 */
	ans = (carry==1);
	break;
	
	case 0x08: /* always true */
	ans = 0x01;
	break;
	
	case 0x09: /* (S XOR V) =0 */
	ans = (sign^overflow)==0;
	break;
	
	case 0x0A: /* (Z OR (S XOR V)) = 0 */	
	ans = (zero|(sign^overflow))==0;
	break;
	
	case 0x0B: /* ((C=0) AND (Z=0)) =1 */
	ans = ((carry==0)&(zero==0))==1;
	break;
	
	case 0x0C: /* V=0 */
	ans = (overflow==0);
	break;
	
	case 0x0D: /* S = 0 */
	ans = (sign==0);
	break;
	
	case 0x0E: /* Z = 0 */
	ans = (zero ==0);
	break;
	
	case 0x0F: /* C = 0 */
	ans = (carry==0);
	break;
}

sys_clock +=10;
#ifdef JUMP
if(ans)
{
	printf("condition is true \n");
}
else{
	printf("condition is false \n");
}
#endif
return ans;
}


/* Interrupt Group functions */

/* handling priorrity for interrupts of group A
returns the index of the interrupt vector with the higher priority 
returns ff if the IRQ bit of the both interrupts in the group is not set */
BYTE group_A()
{
	BYTE ans = 0xff;
#ifdef DEBUG
printf("Group A \n");
#endif
	if(BIT_5(reg_mem[IPR].content))
	{
		/*call IRQ3 first 
		call IRQ5 */
		if (IRQ3 & reg_mem[IRQ].content)
		//IRQ3 is set
			ans = 0x03;
		else if(IRQ5 & reg_mem[IRQ].content)
		//IRQ5 is set
			ans = 0x05;
#ifdef DEBUG 
printf(" IRQ3 > IRQ5 \n");
#endif
	}
	else{
		/* call IRQ5 first 
		call IRQ3
		*/
		if(IRQ5 & reg_mem[IRQ].content)
		//IRQ5 is set
			ans = 0x05;
		else if (IRQ3 & reg_mem[IRQ].content)
		//IRQ3 is set
			ans = 0x03;
#ifdef DEBUG
printf(" IRQ5 > IRQ3 \n");
#endif
	}
	return ans;
}

/* handling priorrity for interrupts of group B */
BYTE group_B()
{
	BYTE ans=0xff;
#ifdef DEBUG
printf("Group B \n");
#endif
	if (BIT_2(reg_mem[IPR].content))
	{
		/* call IRQ0 first 
		call IRQ2
		*/
		if (IRQ0 & reg_mem[IRQ].content)
		{// if IRQ0 bit is set
			ans = 0x00; // IRQ0
		}
		else if (IRQ2 & reg_mem[IRQ].content){
			ans = 0x02; //IRQ2
		}
#ifdef DEBUG 
printf(" IRQ0 > IRQ2 \n");
#endif
	}
	else {
		/* call IRQ2 
		call IRQ0
		*/
		if (IRQ2 & reg_mem[IRQ].content){
			ans = 0x02; //IRQ2
		}
		else if (IRQ0 & reg_mem[IRQ].content)
		{// if IRQ0 bit is set
			ans = 0x00; // IRQ0
		}
#ifdef DEBUG 
printf(" IRQ2 > IRQ0 \n");
#endif
	}
	return ans;
}

/* handling priority for interrupts of group C */
BYTE group_C()
{
	BYTE ans = 0xff; 
#ifdef DEBUG
printf("Group C \n");
#endif
	if(BIT_1(reg_mem[IPR].content))
	{
		/* call IRQ4 first 
		call IRQ1
		*/
		if (IRQ4 & reg_mem[IRQ].content){
			ans = 0x04; //IRQ4
		}
		else if (IRQ1 & reg_mem[IRQ].content)
		{// if IRQ0 bit is set
			ans = 0x01; // IRQ1
		}
	#ifdef DEBUG 
	printf("IRQ4 > IRQ1 \n");
	#endif
	}
	else {
		/* call IRQ1 first 
		then call IRQ4 
		*/
		if (IRQ1 & reg_mem[IRQ].content)
		{// if IRQ0 bit is set
			ans = 0x01; // IRQ1
		}
		else if (IRQ4 & reg_mem[IRQ].content){
			ans = 0x04; //IRQ4
		} 
	#ifdef DEBUG 
	printf(" IRQ1 > IRQ4 \n");
	#endif
	}
	return ans;
}

/* Polling function 
returns the byte of the most */
BYTE polling()
{
BYTE retval; // holds the return value
BYTE prt1, prt2,prt3; // holds byte from lowest to highest priority
BYTE temp;
temp = BIT_4(reg_mem[IPR].content)<<2|BIT_3(reg_mem[IPR].content)<<1|BIT_0(reg_mem[IPR].content);
switch(temp)
{
	case 0x00:	/* reserved */
	#ifdef DEBUG 
	printf(" RESERVED 1 \n");
	#endif	
	break;
	
	case 0x01: 	/* c>a>b */
	prt3=group_C();
	prt2=group_A();
	prt1=group_B();
	break;
	
	case 0x02: /* A>B>C */
	prt3=group_A();
	prt2=group_B();
	prt1=group_C();
	break;
	
	case 0x03: /* A>C>B */
	prt3=group_A();
	prt2=group_C();
	prt1=group_B();
	break;
	
	case 0x04: /* B>C>A */
	prt3=group_B();
	prt2=group_C();
	prt1=group_A();
	break;
	
	case 0x05: /* C>B>A */
	prt3=group_C();
	prt2=group_B();
	prt1=group_A();
	break;
	
	case 0x06: /* B>A>C */
	prt3=group_B();
	prt2=group_A();
	prt1=group_C();
	break;
	
	case 0x07: /* reserved */
	
	#ifdef DEBUG 
	printf("RESERVED 2 \n");
	#endif
		
	break;
}

if ((BIT_7(reg_mem[IPR].content) | BIT_6(reg_mem[IPR].content))==0){
	/* reserved */
	#ifdef DEBUG
	printf("RESERVED 3 \n");
	#endif
}
/* if prt3 does not hold ff, retval= prt3, else if prt2 does not hold ff , retval = prt2, else retval = prt3 */
if(prt3 != 0xff)
retval = prt3;
else if(prt2 != 0xff)
retval = prt2;
else
retval = prt1;

return retval;
}

 
/* VEIW MEMORY */

test_prog()
{
	#ifdef VEIW_PROG_MEM
	printf(" program counter holds : %x \n", pc);
	int laddr = pc;
	int i;
	for (i=0; i<0x27; i++)
	{
		printf(" PROG mem loc %x contains %x BYTE \n", laddr, memory[PROG][laddr]);
		laddr++;
	}
	#endif
	#ifdef VEIW_STACK
	printf("stack holds : ");
	WORD i;
	for (i=SP; (i!= 0 )&&(i<0xffff); i++)
	{
		printf("%2x ", read_dm(i));
	}
	printf("%2x ", read_dm(i));
	printf("\n");
	#endif
}

///////////////////////////////////////////////////////////////////////////////////////////

void run_machine()
{
/* Z8 machine emulator
   instruction fetch, decode, and execute 
*/
BYTE inst;       /* Current instruction */
BYTE src;        /* SRC operand - read if needed */
BYTE dst;        /* DST operand - read if needed */
BYTE regval;	 /* temp to emulate OR instruction */
BYTE high_nib;   /* MS nibble of instruction */
BYTE low_nib;    /* LS nibble of instruction */
int running;     /* TRUE until STOP instruction */
int sanity;      /* Limit on number of instruction cycles */
WORD dest;		 /*	for 16 bit addressing, in case on DA, IRR and @IRR */
WORD temp;
running = TRUE;
sanity = 0;

BYTE tcount; // number of true instruction 
BYTE fcount; // number of false instructions
BYTE cexec; // 1 execute true part 0 exec false part
BYTE tempflags; // temp variable to hold the flag until the emulator is done excuting conditional statements

#ifdef IE_TEST
write_rm(IMR, INT_ENA | IRQ0); /* PORT 0 interrupts allowed */
write_rm(IRQ, 0);              /* No pending interrupt requests */
#endif

while (running && sanity < 30)
{
#ifdef IE_TEST
printf("Time: %02d  IRQ: %02x\n", sys_clock, reg_mem[IRQ] . content);
#endif

     
/* Get next instruction and extract nibbles */
#ifdef WATCH
printf("Program counter holds : %x \n", pc);
#endif
     inst = prog_mem_fetch();
     high_nib = MSN(inst);
     low_nib = LSN(inst);
     
     /* Check for special cases: 
        - lnib: 0x08 through 0x0E - r or cc in hnib 
        - lnib: 0x0F - no-operand instructions */
     if (low_nib >= 0x08)
     {
          /* LD through INC and some no-operand instructions */
          switch(low_nib)
          {
          case 0x08: /* LD dst , src  r,R */
          dst = RPBLK | high_nib;
          src = read_rm(prog_mem_fetch()); // src value
          /*dst <-- src*/
          write_rm(dst, src);
          sys_clock +=6; // 6 cycles 
          break;
          
		  case 0x09: /* LD dst, src  R,r */
		  src = read_rm(RPBLK|high_nib);
		  dst = prog_mem_fetch();
		  /*dst <-- src*/
		  write_rm(dst, src);
		  sys_clock +=6; // 6 cycles
		  break;
		  	
		  case 0x0A: /* DJNZ r, dst */
               dst = prog_mem_fetch();
               src = RPBLK|high_nib;
               regval = read_rm(src);
               regval -= 1;
               if (regval != 0)
               {
               	/* Reg != 0 -- repeat loop */
               /* Signed extend dst to 16 bits if -ve */
               pc = pc + SIGN_EXT(dst);
               #ifdef JUMP
	  			printf("pc after jump is : %x \n", pc);
	  			printf("contents of prog mem(pc) is %x \n", read_pm(pc));
	  			#endif
	  			/***********IF ADDITION *************/
				tcount =0;
				fcount=0;
				cexec=0;
				#ifdef IF_TEST
                printf("flow of control tcount: %x fcount: %x cexec: %x \n", tcount, fcount, cexec);
                #endif
				/***********************************/ 
               sys_clock +=2;
               }
                write_rm(src, regval);
                sys_clock +=10; // total of 12 cycles if JUMP is taken 
               break;
               
		  case 0x0B: /* JR cc,RA */
		  		dst = prog_mem_fetch(); // RA
		  		if(cond_handler(high_nib)){
		  			#ifdef JUMP
		  			printf("pc before jump is : %x \n", pc);
		  			printf("contents of prog mem(pc) is %x \n", read_pm(pc));
		  			#endif
		  			pc = pc + SIGN_EXT(dst);
		  			#ifdef JUMP
		  			printf( "JUMP TAKEN: "
					  		"dst = %x , pc = %x \n", dst, pc);
					  		printf("contents of prog mem(pc) is %x \n", read_pm(pc));
					#endif
					/***********IF ADDITION *************/
					tcount =0;
					fcount=0;
					cexec=0;
					#ifdef IF_TEST
	                printf("flow of control tcount: %x fcount: %x cexec: %x \n", tcount, fcount, cexec);
	                #endif
					/***********************************/ 
		  			sys_clock += 2; // total of 12 cycles if jump is taken 
		  		}
		  	   break;
               
          case 0x0C: /* LD dst, IMM */
               dst = prog_mem_fetch();
               write_rm((RPBLK|high_nib), dst);
               sys_clock +=6; // 6 cycles 
               break;
               
          case 0x0D: /* JP cc, DA */
          		/* get DA */
          		dest = (prog_mem_fetch() <<8);
				dest|=(prog_mem_fetch());
        	   if(cond_handler(high_nib)){
        	   	// condition code is true
        	   	/* PC <-- dst */
        	   	pc = dest;
        	   	#ifdef JUMP
        	   	printf("JUMP TAKEN, NEW PC = %x \n", pc);
        	   	#endif
	    	   	/***********IF ADDITION *************/
				tcount =0;
				fcount=0;
				cexec=0;
				#ifdef IF_TEST
                printf("flow of control tcount: %x fcount: %x cexec: %x \n", tcount, fcount, cexec);
                #endif
				/***********************************/ 
        	   	sys_clock += 2; // total of 12 cycles if jump is taken
        	   }
               break;
              
          case 0x0E: /* INC dst */
               dst = RPBLK | high_nib;
               regval = read_rm(dst);
               sign = SIGN(regval);
               regval += 1;
               write_rm(dst, regval);
               /* Update flags */
               write_rm(FLAGS, FLAG_Z(regval == 0));
               write_rm(FLAGS, FLAG_S(SIGN(regval)));
               write_rm(FLAGS, FLAG_V(SIGN(regval) != sign)); 
               
               sys_clock +=6; // 6 cycles
               break;
               
          case 0x0F: /* STOP .. NOP */     
               switch (high_nib)
               {
               case 0x01: /* IF instuction */
               /* fetch next byte */
               dst = prog_mem_fetch();
               regval = MSN(dst); // condition code
               src = LSN(dst);
               dst = src>>2;
               tcount = dst&0x03; // retrieve tt
               fcount = src&0x03; // retrieve ff
               cexec = cond_handler(regval); // holds 1 if condition is true, and 0 if false
               /* store flags in temp variable */
               tempflags = read_rm(FLAGS);
               #ifdef IF_TEST
               printf(" dst : %x, src: %x \n", dst, src);
               printf("If encountered, tcount %x, fcount: %x : cond : %x \n", tcount, fcount, cexec);
			   #endif	
               break;
               case 0x06: /* STOP - added instruction - not on opcode map */
                    running = FALSE;
               break;
               
               case 0x07: /* HALT system and wait for interrupt */
               printf("Halt system and check for interrupts! \n");
               break;
               
               case 0x08: /* DI disable interrupts */
               printf(" Interrupts disabled \n");
               write_rm(IMR, IMR_7(0)); // IMR(7) <--0 
               sys_clock += 6;
               break;
               
               case 0x09: /* EI Enable interrupts */
               printf("Interrupts enabled \n ");
               /* IMR(7) <--1  */
               write_rm(IMR, IMR_7(0x01));
               sys_clock +=6; // 6 cycles
               break;
               
               case 0x0A: /*RET */
               sp = SP;
               if (SPLOC == 0){ // stack is in data memory
	               pc = (read_dm(sp++)<<8)|read_dm(sp++);
	               write_rm(SPL, LSBY(sp));
	               write_rm(SPH, MSBY(sp));
               }else{ //stack is in reg_mem
               	pc = read_rm(sp++)<<8|read_rm(sp++);
               	write_rm(SPL, LSBY(sp));
               }
               sys_clock +=14; // 14 cycles 
               break;
               
               case 0x0B: /* IRET */
               sp = SP;
               if (SPLOC ==0) { // stack is in data memory 
               	/* FLAGS <-- @SP */
               write_rm(FLAGS, read_dm(sp++));
               /* PC <-- @SP	*/
               pc = (read_dm(sp++)<<8)|read_dm(sp++);
               /* SP <-- SP +2 */
               write_rm(SPL, LSBY(sp));
               write_rm(SPH, MSBY(sp));
               }
               else{ // stack in reg mem
               /* FLAGS <-- @SP */
               write_rm(FLAGS, read_rm(sp++));
               /* PC <-- @SP	*/
               pc = (read_rm(sp++)<<8)|read_rm(sp++);
               /* SP <-- SP +2 */
               write_rm(SPL, LSBY(sp));
               }
               /* IMR(7) <--1  */
               write_rm(IMR, IMR_7(0x01)); // enable interupts 
               /* increment system clock */
               sys_clock +=16; // 16 cycles
               break;
               
               case 0x0C: /* RCF */
               carry =0;
               write_rm(FLAGS, FLAG_C(carry));
               sys_clock +=6; // 6 cycles 
               break;
               
               case 0x0D: /* SCF */
               carry = 0x01; //C<--1
               write_rm(FLAGS, FLAG_C(carry));
               printf("carry is set \n");
               sys_clock +=6; // cycles
               break;
               
               case 0x0E: /* CCF */
               carry = !carry;
               write_rm(FLAGS, FLAG_C(carry));
               sys_clock+=0x06; // 6 cycles
               break;
               
               case 0x0F: /* NOP */
               sys_clock +=6; // 6 cycles
               break;
               }
               break;
          }
     }
     else
     {
          if (low_nib >= 0x02 && (high_nib <= 0x07 || high_nib== 0x0A || high_nib == 0x0B || high_nib ==0x0E))
		  {
		  	/*	instructions have thesame addressing modes
				2 operands instruction destination and source
			  	call get_args to return source value and destination address
			  */ 
          	src = get_args(low_nib, &dst); // get src value and dst address
          	switch (high_nib)
          	{
          		case 0x00: // ADD
          		regval = read_rm(dst);
          		write_rm(dst, adder(regval, src, 0)); //  0 no carry 
          		//flags are implicitly set in the adder function
          		break;
          		
          		case 0x01: // ADC
          		regval = read_rm(dst);
          		write_rm(dst, adder (regval, src, carry)); // + carry 
          		break;
          		
          		case 0x02: // SUB
          		regval = read_rm(dst);
          		write_rm(dst, subber (regval, src, 0)); // 0: no carry 
          		break;
          		
          		case 0x03: //SUBC 
          		regval = read_rm(dst);
          		write_rm(dst, subber (regval, src, carry)); // - carry
				//flags are set implicitly in subber function 
          		break;
          		
          		case 0x04: //OR
          		regval = read_rm(dst);
          		regval = regval|src;
          		/* set flags */
          		write_rm(FLAGS, FLAG_Z(ZERO(regval))); // set zero flag
          		write_rm(FLAGS, FLAG_V(0)); // reset overflow flag to zero
          		write_rm(FLAGS, FLAG_S(SIGN(regval))); // set signed flag
          		break;
          		
          		case 0x05: //AND
          		regval = read_rm(dst);
          		regval = regval&src;
          		/* set flags */
          		write_rm(FLAGS, FLAG_Z(ZERO(regval))); // set zero flag
          		write_rm(FLAGS, FLAG_V(0)); // reset overflow flag to zero
          		write_rm(FLAGS, FLAG_S(SIGN(regval))); // set signed flag
          		break;
          		
          		case 0x06: // TCM
          		regval = read_rm(dst);
          		regval = ~regval&src;
          		/* set flags */
          		write_rm(FLAGS, FLAG_Z(ZERO(regval))); // set zero flag
          		write_rm(FLAGS, FLAG_V(0)); // reset overflow flag to zero
          		write_rm(FLAGS, FLAG_S(SIGN(regval))); // set signed flag
          		break;
          		
          		case 0x07: //TM
          		regval = read_rm(dst);
          		regval = regval&src;
          		/* set flags */
          		write_rm(FLAGS, FLAG_Z(ZERO(regval))); // set zero flag
          		write_rm(FLAGS, FLAG_V(0)); // reset overflow flag to zero
          		write_rm(FLAGS, FLAG_S(SIGN(regval))); // set signed flag
          		break;
          		
          		case 0x0A: //CP
          		regval = read_rm(dst);
          		subber(regval, src, 0);
          		//flags are set implicitly in the function
          		break;
          		
          		case 0x0B: //XOR 
          		regval = read_rm(dst);
          		write_rm(dst, regval^src);
          		/* set flags */
          		write_rm(FLAGS, FLAG_Z(ZERO(regval))); // set zero flag
          		write_rm(FLAGS, FLAG_V(0)); // reset overflow flag to zero
          		write_rm(FLAGS, FLAG_S(SIGN(regval))); // set signed flag
          		break;
          		
          		case 0x0E: //LD
          		/* dst <--src */
          		write_rm(dst, src);
          		break;
          	}
          }
          else if (low_nib < 0x02){
          	// handles special cases
          	if (high_nib ==0x03)
          	{
          		switch(high_nib)
          		{
          			case 0x03: // JP & SRP
          			dst = prog_mem_fetch(); // get next memory location
          			if(low_nib == 0x00){ // JP dst IRR
          				/* get 16 Bit destination */
          				regval = read_rm(dst++);
          				dest = regval<<8;
          				regval = read_rm(dst);
          				dest = dest|regval;
          				/*PC <--dst */
          				pc=dest;
          				/***********IF ADDITION *************/
						tcount =0;
						fcount=0;
						cexec=0;
						#ifdef IF_TEST
		                printf("flow of control tcount: %x fcount: %x cexec: %x \n", tcount, fcount, cexec);
		                #endif
						/***********************************/ 
          				/* increment system clock */
          				sys_clock +=8; // 8 cycles 
          			}else if (low_nib== 0x01){ // SRP IMM
			          	write_rm(RP, LSN(dst));
			          	printf(" RP Is set to : %x \n", read_rm(RP));
			          	sys_clock +=6; // 6 cycles
          			}
          			break;
          		}
          		
          	}else{
          		/* The same addressing mode 
				  call get_args_2() to retrieve dst
				  low nib? 	0 - R 
				  			1 - IR 
				  			Also used when 0 - RR ; i.e for INCW AND DECW 
				*/
				dst = get_args_2(low_nib);
		          switch (high_nib)
		          {
		          	case 0x00: // DEC
		          	regval = read_rm(dst);
		          	sign = SIGN(regval); // store value incase of overflow
		          	regval--;
		          	write_rm(dst, regval);
		          	//set flags
	          		write_rm(FLAGS, FLAG_Z(ZERO(regval))); // set zero flag
	          		write_rm(FLAGS, FLAG_S(SIGN(regval))); // set signed flag
	          		write_rm(FLAGS, FLAG_V(sign != SIGN(regval))); // set overflow flag		          	
		          	break;
		          	
		          	
		          	case 0x01: //RLC
		          	regval = read_rm(dst);
		          	sign = SIGN(regval);
		          	regval= (regval*2)+carry;
		          	write_rm(dst, regval);
		          	carry = sign>>7;
		          	/* Set flags */
					write_rm(FLAGS, FLAG_C(carry));
					write_rm(FLAGS, FLAG_S(SIGN(regval)));
					write_rm(FLAGS, FLAG_Z(ZERO(regval)));
					write_rm(FLAGS, (FLAG_V(SIGN(regval))==sign));
		          	
		          	break;
		          	
		          	case 0x02: //INC
		          	regval = read_rm(dst);
		          	sign = SIGN(regval);
		          	regval+=1;
		          	write_rm(dst, regval);
		          	//set flags
	          		write_rm(FLAGS, FLAG_Z(ZERO(regval))); // set zero flag
	          		write_rm(FLAGS, FLAG_S(SIGN(regval))); // set signed flag
	          		write_rm(FLAGS, FLAG_V(sign != SIGN(regval))); // set overflow flag
		          	break;
		          	
		          	case 0x04:	//DA
		          	regval = read_rm(dst);
		          	write_rm( dst, adjust_dec(regval));
		          	sys_clock+=2; // total of 8 cycles
		          	// flags are set implicitly in the adjust_dec function
					break;
					
					case 0x05:	//POP
					sp =SP; // stack pointer
					/* dst <--@SP */
					
					if(SPLOC == 0)
					{ // Stack is in data memory 
						regval= read_dm(sp++);
						write_rm(dst, regval);
						/* SP <-- SP+1	*/
					    write_rm(SPH, MSBY(sp));
						
					}
					else{ // stack is in Reg-mem
						regval = read_rm(sp++);
						write_rm(dst, regval);
					}
                    /* SP <--- Sp+1 */
					write_rm(SPL,LSBY(sp));
					sys_clock += 4; // total of 10 cycles
					break;
					
					case 0x06: //COM
					regval = read_rm(dst);
					regval = ~regval;
					write_rm(dst, regval);
					/* set flags */
	          		write_rm(FLAGS, FLAG_Z(ZERO(regval))); // set zero flag
	          		write_rm(FLAGS, FLAG_S(SIGN(regval))); // set signed flag
	          		write_rm(FLAGS, FLAG_V(0)); // reset overflow flag to zero
					
					break;
					
					case 0x07:	//PUSH
					/* SP <-- SP-1 */
					sp =SP-0x01;
					write_rm(SPL, LSBY(sp));
					/*@SP <-- src */				
					regval = read_rm(dst);
					if (SPLOC == 0){ //stack is in data memory
					
					write_dm(sp, regval);
					write_rm(SPH, MSBY(sp));
					sys_clock += 2; // 2 extra cycles for external stack
					}else{	// stack is in register mermory
					write_rm(SPL, regval);
					}
					
					/* increment system clock */
					if (low_nib == 0x01)
					{
						sys_clock +=2; // 2 extra cycles for IR address mode
					}
					
					sys_clock +=4; // total base value of 10 cycles
					break;
					
					case 0x08: // DECW
					// ADDRESSING MODE IS RR for low_nib =0, IR for low nib = 1
					/* get 16 bit destination value */
					dest = read_rm(dst++)<<8;
					regval = read_rm(dst);
					dest = dest|regval;
					sign = SIGN(MSBY(dest)); // store sign value for overflow check
					dest--; // dest = dest -1
					write_rm(dst--, LSBY(dest)); // writes lo byte to memory
					write_rm(dst, MSBY(dest)); //  writes hi byte to memory 
					
		          	//set flags
	          		write_rm(FLAGS, FLAG_Z(ZERO(regval))); // set zero flag
	          		write_rm(FLAGS, FLAG_S(SIGN(regval))); // set signed flag
	          		write_rm(FLAGS, FLAG_V(sign != SIGN(regval))); // set overflow flag
	          		sys_clock += 4; // total of 10 cycles
          			break;
					
					case 0x09:	//RL
					regval = read_rm(dst);
					sign = SIGN(regval);
					/* C <-- dst(7) */
					carry = sign>>7;
					write_rm(dst ,(regval*2)+ carry);
					/* set flags */
					write_rm(FLAGS, FLAG_C(carry));
					write_rm(FLAGS, FLAG_S(SIGN(regval)));
					write_rm(FLAGS,FLAG_Z(ZERO(regval)));
					write_rm(FLAGS,FLAG_V(SIGN(regval)==sign));
					break;
					
					
					case 0x0A: // INCW
					// ADDRESSING MODE IS RR for low_nib =0, IR for low nib = 1
					/* get 16 bit destination value */
					
					dest = read_rm(dst++)<<8;
					regval = read_rm(dst);
					dest = dest|regval;
					sign = SIGN(MSBY(dest)); // store sign value for overflow check
					dest++; // dest = dest +1
					write_rm(dst--, LSBY(dest)); // writes lo byte to memory
					write_rm(dst, MSBY(dest)); //  writes hi byte to memory 
					
		          	//set flags
	          		write_rm(FLAGS, FLAG_Z(ZERO(regval))); // set zero flag
	          		write_rm(FLAGS, FLAG_S(SIGN(regval))); // set signed flag
	          		write_rm(FLAGS, FLAG_V(sign != SIGN(regval))); // set overflow flag
	          		
	          		/* increment sys_clock */
	          		sys_clock +=4; // total of 10 cycles
          			break;
					
					case 0x0B:	//CLR
					write_rm(dst, 0);
					break;
					
					case 0x0C:	//RRC
					regval = read_rm(dst); // temp to hold content of dst reg
		          	sign = SIGN(regval);
		          	src = LSB(regval); // hold value of carry temporarily
		          	write_rm(dst, (regval/2)+(carry<<7));
		          	carry = src;
		          	/* Set flags */
		          	regval = read_rm(dst);
					write_rm(FLAGS, FLAG_C(carry));
					write_rm(FLAGS, FLAG_S(SIGN(regval)));
					write_rm(FLAGS, FLAG_Z(ZERO(regval)));
					write_rm(FLAGS, FLAG_V(SIGN(regval)==sign));					
					
					break;
					
					case 0x0D:	//SRA
					sign= SIGN(read_rm(dst)); 
					carry = LSB(read_rm(dst));
					write_rm(dst, (read_rm(dst)/2) + sign);
					/* set flags */
		          	src = read_rm(dst);
					write_rm(FLAGS, FLAG_C(carry));
					write_rm(FLAGS, FLAG_S(SIGN(src)));
					write_rm(FLAGS, FLAG_Z(ZERO(src)));
					write_rm(FLAGS, FLAG_V(SIGN(0)));	// always zero
					break;
					
					case 0x0E:	//RR
					src = read_rm(dst); // uses src as a temp
		          	sign = SIGN(src);
		          	carry = LSB(src);
		          	write_rm(dst,(src/2)+(carry<<7));
		          	/* Set flags */
		          	src = read_rm(dst);
					write_rm(FLAGS, FLAG_C(carry));
					write_rm(FLAGS, FLAG_S(SIGN(src)));
					write_rm(FLAGS, FLAG_Z(ZERO(src)));
					write_rm(FLAGS, FLAG_V(SIGN(src)==sign));
					break;
					
					case 0x0F:	//SWAP
					src = read_rm(dst);
					write_rm(dst, (LSN(src)<<4)|MSN(src));
					/* set flags */
					write_rm(FLAGS, FLAG_S(SIGN(read_rm(dst))));
					write_rm(FLAGS, FLAG_Z(ZERO(read_rm(dst))));
					sys_clock +=2; // total of 8 cycles 
					break;      
				  }
		          
          	}
          } 	
          else{
          	// peculiar cases 
          	switch(high_nib)
          	{
          		case 0x08: 
          		switch (low_nib)
          		{
          			case 0x02:	/* LDE (dst, src) r, Irr */
          			dst = prog_mem_fetch();
          			src = RPBLK|LSN(dst);
          			dst = RPBLK|MSN(dst);
          			/* obtain 16 bit address of source in data mem */
          			dest = (read_rm(src++)<<8)|read_rm(src);
          			/* dst <-- src */
          			write_rm(dst, read_dm(dest));
          			sys_clock +=12; //12 cycles 
          			break;
          			
          			case 0x03: /* LDEI (dst, src) Ir, Irr */
          			dst = prog_mem_fetch();
          			src = RPBLK|LSN(dst);
          			dst = read_rm(RPBLK|MSN(dst));
          			/* obtain 16 bit address of source in data mem */
          			dest = (read_rm(src++)<<8)|read_rm(src++);
          			/* dst <-- src */
          			write_rm(dst++, read_dm(dest)); 
          			sys_clock +=18; //18 cycles 
          			break;
          		}
          		break;
          		
          		case 0x09:
          		switch (low_nib)
          		{
          			case 0x02:	/* LDE (dst, src) Irr, r */
          			dst = prog_mem_fetch();
          			src = RPBLK|MSN(dst);
          			dst = RPBLK|LSN(dst);
          			/* obtain 16 bit address of source in data mem */
          			dest = (read_rm(src++)<<8)|read_rm(src);
          			/* dst <-- src */
          			write_rm(dst, read_dm(dest));
          			sys_clock += 12; //12 cycles 
          			break;
          			
          			case 0x03: /* LDEI (dst, src) Irr, Ir */
          			dst = prog_mem_fetch();
          			src = RPBLK|MSN(dst);
          			dst = read_rm(RPBLK|LSN(dst));
          			/* obtain 16 bit address of source in prog mem */
          			dest = (read_rm(src++)<<8)|read_rm(src++);
          			/* dst <-- src */
          			write_rm(dst++, read_dm(dest));
          			sys_clock +=18; //18 cycles
          			break;         			
          		}
          		break;
          		
          		case 0x0C:
          		switch (low_nib)
          		{
          			case 0x02:	/* LDC (dst, src) r, Irr */
          			dst = prog_mem_fetch();
          			src = RPBLK|LSN(dst);
          			dst = RPBLK|MSN(dst);
          			/* obtain 16 bit address of source in prog mem */
          			regval = read_rm(src);
          			dest = read_rm(RPBLK|(regval++))<<8;
          			dest |= read_rm(RPBLK|(regval));
          			/* destination now holds target addr in program mem */
          			/* get source value */
          			src = read_pm(dest);
          			/* dst <-- src */
          			write_rm(dst, src);
					sys_clock +=12; // 12 cycles  			
          			break;
          			
          			case 0x03: /* LDCI (dst, src) Ir, Irr */
          			/* loading from program memory into register memory */
          			dst = prog_mem_fetch();
          			src = RPBLK|LSN(dst);
          			dst = RPBLK|MSN(dst);
          			/* obtain 16 bit address of source in prog mem */
          			regval = read_rm(src);
          			dest = read_rm(RPBLK|(regval++))<<8;
          			dest |= read_rm(RPBLK|(regval++));
          			temp = dest +1; // rr<-- rr+1
          			regval = MSBY(temp); 
          			write_rm(src++, regval);
          			regval = LSBY(temp);
          			write_rm(src, regval);
          			/* destination now holds target addr in program mem
          			and Irr in reg_mem now points to the succeeding register pair */
          			/* get source value */
          			src = read_pm(dest);
          			/* get dst addr in reg_mem */
          			regval = read_rm(dst);
          			write_rm(dst, (regval+1)); // r<-- r+1
          			/* dst <-- src */
          			dst = RPBLK|regval;
          			write_rm(dst, src); 
          			sys_clock +=18; //18 cycles 
          			break;
					
					case 0x07:	/*LD (dst, src) : r, X	*/
					dst = RPBLK|MSN(prog_mem_fetch());
					src = prog_mem_fetch();
					write_rm(dst, read_rm(src+dst));
					sys_clock += 10; // 10 cycles 
					break;    			
          		}
				break;
				
				case 0x0D:
          		switch (low_nib)
          		{
          			case 0x02:	/* LDC (dst, src): Irr, r */
          			dst = prog_mem_fetch();
          			src = RPBLK|MSN(dst);
          			dst = RPBLK|LSN(dst);
          			/* obtain 16 bit address of destination in prog mem */
          			regval = read_rm(dst);
          			dest = read_rm(RPBLK|(regval++))<<8;
          			dest |= read_rm(RPBLK|(regval));
          			/* PROG_MEM[dest] <-- src */
          			write_pm(dest, src);
          			sys_clock +=12; //12 cycles 
          			break;
          			
          			case 0x03: /* LDCI (dst, src) Irr, Ir */
          			/* load from reg_mem to program memory 
					  and increment r and rr	*/
          			dst = prog_mem_fetch(); // get operands
					src = RPBLK|MSN(dst);
          			dst = RPBLK|LSN(dst);
          			/* obtain 16 bit address of destination in prog mem */
          			regval = read_rm(dst);
          			dest = read_rm(RPBLK|(regval++))<<8;
          			dest |= read_rm(RPBLK|(regval++));
          			temp = dest +1; // rr<-- rr+1
          			regval = MSBY(temp);
          			write_rm(dst++, regval);
          			regval = LSBY(temp);
          			write_rm(dst, regval);
          			/* destination now holds target addr in program mem
          			and Irr in reg_mem now points to the succeeding register pair */
          			/* get source value */
          			regval = read_rm(src);
          			dst = read_rm(RPBLK|(regval++)); // source value
          			write_rm(src, regval); // r<--r+1
          			src = dst; // src now holds source value
          			/* PROG_MEM[dest] <-- src */
          			write_pm(dest, src);
          			sys_clock +=18; // 18 cycles 
          			break;
					
					case 0x04: /* CALL (dst) IRR	*/
					/* get destination */
					dst = prog_mem_fetch();
					dest = (read_rm(dst++))<<8;
					dest = (dest|read_rm(dst));
					/* SP <-- SP-2 */
					sp = SP;
					sp =-0x02;
					/* updating the SP */
					write_rm(SPL, LSBY(sp));
					if(SPLOC == 0){ // stack is in data memory
					write_rm(SPH, MSBY(sp));
					/* @SP <-- PC */
					write_dm(sp++, MSBY(pc));
					write_dm(sp++, LSBY(pc));
					}else{	// stack is in register memory 
					/* @SP <-- PC */
					write_rm(sp++, MSBY(pc));
					write_rm(sp++, LSBY(pc));
					}
					/* PC <-- dst */
					pc = dest;
					/***********IF ADDITION *************/
					tcount =0;
					fcount=0;
					cexec=0;
					#ifdef IF_TEST
	                printf("flow of control tcount: %x fcount: %x cexec: %x \n", tcount, fcount, cexec);
	                #endif
					/***********************************/ 
					sys_clock +=20; // 20 cycles
					break;
					
					case 0x06: /* CALL (dst) DA 	*/
					/* get destination */
					dst = prog_mem_fetch();
					dest = dst<<8;
					dst = prog_mem_fetch();
					dest = (dest|dst);
					/* SP <-- SP-2 */
					sp = SP;
					sp =-0x02;
					write_rm(SPL, LSBY(sp));
					if(SPLOC==0){ // stack is in data mem
						write_rm(SPH, MSBY(sp));
						/* @SP <-- PC */
						write_dm( sp++, MSBY(pc));
						write_dm(sp, LSBY(pc));	
					}else{// stack is in reg mem
					/* @SP <-- PC */
						write_rm( sp++, MSBY(pc));
						write_rm(sp, LSBY(pc));	
						#ifdef VEIW_STACK
						printf(" stack holds : lo: %2x hi: %2x \n", read_rm(sp--), read_rm(sp));
						#endif							
					}
					/* PC <-- dst */
					pc = dest;
					/*increment system clock */
					sys_clock +=20; // 20 cycles
					/***********IF ADDITION *************/
					tcount =0;
					fcount=0;
					cexec=0;
					#ifdef IF_TEST
					printf("flow of control tcount: %x fcount: %x cexec: %x \n", tcount, fcount, cexec);
					#endif
					/***********************************/ 
					#ifdef VEIW_MEM
					printf("pc holds : %4x \n", pc);
					#endif
					break;
					
					case 0x07:	/* LD X,r	*/
					src = RPBLK|MSN(prog_mem_fetch()); // fetch register r
					dst = prog_mem_fetch();    // fetch offset X
					// dst <- reg_mem[R + offset]
					write_rm(dst, read_rm(src+dst));
					sys_clock +=10; //10 cycles 
					break;
          		}		
          		break;
          		
          		case 0x0F:
          		switch (low_nib)
          		{
          			case 0x03:	/* LD (dst, src) Ir, r */
          			dst = prog_mem_fetch();
          			src = read_rm(RPBLK|LSN(dst));
          			dst = read_rm(RPBLK|MSN(dst));
          			write_rm(dst, src);	// dst <-- src
          			sys_clock +=6; // 6 cycles 
          			break;
          			
          			case 0x05: /* LD (dst, src) IR, R */
          			src = prog_mem_fetch(); //get IR
          			regval = read_rm(src);  // regval <-- reg_mem{IR] i.e  
          			src = read_rm(regval);  // src = reg_mem[R]
          			dst = prog_mem_fetch();
          			write_rm(dst, src);
          			sys_clock +=10; // 10 cycles
          			break;         			
          		}
          		break;
          	}
          }
     }

#ifdef IE_TEST
     switch(sanity)
     {
     case 3:
            write_rm(PORT0, 0x83); /* Timer: continuous & 3x2 (6) cycles */
            break;
     case 11:
     case 16:
            /* Emulate ISR:
               - Acknowledge IRQ0 interrupt - emulate OR 
            */
            regval = read_rm(IRQ);
            regval &= ~IRQ0;      /* Mask off IRQ0 only */
            write_rm(IRQ, regval);
            break;
     case 12:
     case 17:
            /* Emulate ISR:
               - Emulate IRET - reenable interrupts */
            reg_mem[IMR] . content |= INT_ENA;
            break;
            
     }
#endif
     
     /* Instruction cycle completed -- check for interrupts 
        - call device check
        - TRAPs - software-generated interrupts (SWI, SVC, etc) are caused
          by turning IRQ bit on (see section 2.6.4 in assignment)
        - IRET enables interrupts:
           reg_mem[IRQ] . content |= INT_ENA;
        - Concurrent interrupts are possible if timer and uart interrupt at 
          same time
     */
     #ifdef IE_TEST
	 TIMER_check();
     //UART_check();
     #endif

     if ((reg_mem[IMR] . content & INT_ENA) && reg_mem[IRQ] . content != 0)
     {
          /* CPU interrupts enabled and one or more pending interrupts
	       - check interrupt mask (IMR) if device allowed 
          */
          if ((reg_mem[IMR] . content & reg_mem[IRQ] . content) & IRQ_MASK)
          {
#ifdef IE_TEST
printf("Interrupt on IMR: %02x\n", 
                  reg_mem[IMR] . content & reg_mem[IRQ] . content);
#endif
               /* At least one interrupt pending 
                - if single, proceed
                - if multiple, determine priority to select
                - push Flags, PC lo, and PC hi
                - clear interrupt status bit (IMR[7])
                - PC hi = device vector[n]
                - PC lo = device vector[n+1]
                - ISR entered on this clock tick 
                - Pending interrupts remain until after IRET
                - IRET enables interrupt
                - update sys_clock with interrupt overhead
               */
               
               switch (reg_mem[IRQ].content)
               {
               	case IRQ0:
               		/* IRQ0 interrupt */
               		regval = 0x00; // reg_val holds the value of the IRQ index
               		break;
               	case IRQ1:
               		regval = 0x01;
               		break;
               	case IRQ2:
               		regval = 0x02;
               		break;
               	case IRQ3:
               		regval = 0x03;
               		break;
               	case IRQ4:
               		regval = 0x04;
               		break;
               	case IRQ5:
               		regval = 0x05;
               		break;
               	default:
               		/* more than one pending interrupt
               		   call polling function to determine by priority 
					   returns IRQ with highest priority */
               		regval = polling();
               }
               /* get interrupt vector */
				dst = regval*2; // Interrupt vector holds two contiguous locations in memory
				dest = read_pm(dst); // hi byte is 00
				dest = dest <<8 | (read_pm(dst+1));
				/* PUSH PC lo and PC hi unto stack */
				/* SP <-- SP-2 */
				sp = SP;
				sp =-0x02;
				write_rm(SPL, LSBY(sp));
				if(SPLOC==0){ // stack is in data mem
					write_rm(SPH, MSBY(sp));
					/* @SP <-- PC */
					write_dm( sp++, MSBY(pc));
					write_dm(sp, LSBY(pc));	
				}else{// stack is in reg mem
				/* @SP <-- PC */
					write_rm( sp++, MSBY(pc));
					write_rm(sp, LSBY(pc));								
				}
				/* PC <-- interrupt vector */
				pc = dest;
				#ifdef IE_TEST
				printf(" pc holds interrupt vector : %x \n", pc);
				#endif
				
				/*increment system clock */
				sys_clock +=20; // 20 cycles
				
				/* PUSH flags unto stack */
				/* SP <-- SP-1 */
				sp =SP-0x01;
				write_rm(SPL, LSBY(sp));
				/*@SP <-- src */				
				src = read_rm(FLAGS);
				if (SPLOC == 0){ //stack is in data memory
				write_dm(sp, src);
				write_rm(SPH, MSBY(sp));
				sys_clock += 2; // 2 extra cycles for external stack
				}else{	// stack is in register mermory
				write_rm(SPL, src);
				}
				sys_clock +=10; // 10 cycles for pushing
				/* clear interrupt status bit */
				reg_mem[IMR] . content &= ~INT_ENA; 
				
				/* clear IRQ bit to signal interrupt is being handled*/
				switch(regval){
					case 0x00:
						dst = IRQ0;
						break;
					case 0x01:
						dst = IRQ1;
						break;
					case 0x02:
						dst = IRQ2;
						break;
					case 0x03:
						dst = IRQ3;
						break;
					case 0x04:
						dst = IRQ4;
						break;
					case 0x05:
						dst = IRQ5;
						break;
				}
				reg_mem[IRQ].content &= ~dst; 
				/* 6 cycles for clearing */
				sys_clock +=6; // total of atleast 36 cycles overhead
          }
     }
     /**********************************IF ADDITION ****************************/
     int i;
     if(cexec) // cexec is 1
     {
     	if(tcount>0x00 && tcount<0x04) //sanity check
     	{
     		#ifdef IF_TEST
     		printf("executing true part, tcount: %x \n", tcount);
     		#endif
     		/* execute true part of the statement */
     		tcount--; // decrement the count of true instructions
     		/* restore flags */
     		write_rm(FLAGS, tempflags);
     	}
     	else if(fcount>0x00 && fcount<0x04)
     	{
     		/* do not execute false part skip over false part
			  increment program counter by number of bytes required per instruction */
			  for(i=fcount; i>0; i--)
			  {
			  	#ifdef IF_TEST
			  	printf("skipping false part, fcount: %x \n", fcount);
			  	#endif
			  	fcount--;
			  	inst = prog_mem_fetch(); // fetch opcode 
			  	pc +=opcode_size[MSN(inst)][LSN(inst)]; // increment pc by size of operands for opcode
			  }
			  /* restore flags */
     		  write_rm(FLAGS, tempflags);
     	}
     }
     else { // cexec == 0,  execute false part only 
     	if(tcount>0x00 && tcount<0x04)
     	{
     		for(i=tcount; i>0; i--)
     		{
     			#ifdef IF_TEST
	     		printf("skipping true part, tcount: %x \n", tcount);
	     		#endif
	     		tcount--;
     			inst = prog_mem_fetch(); // fetch opcode
			  	pc +=opcode_size[MSN(inst)][LSN(inst)]; // increment pc by size of operands for opcode
     		}
     	}
     	else if(fcount>0x00 && fcount<0x04)
     	{
     		/* execute false part */
     		#ifdef IF_TEST
     		printf("executing false part, fcount: %x \n", fcount);
     		#endif
     		fcount--; //decrement false count
     		/* restore Flags */
     		write_rm(FLAGS, tempflags);
     	}
     }
     
     #ifdef IE_TEST
     printf("System clock : %x \n", sys_clock);
     #endif
     disp_reg_mem();
     #ifdef VEIW_CACHE
     veiw_cache();
     #endif
     test_prog();
     sys_clock++;
     sanity++;
}
}

//////*******************Z8_MACHINE CODE *************************************/

/*
 Z8 Interrupt Emulation - Mainline
 
 ECED 3403
 17 June 2014
*/



int main(int argc, char *argv[])
{
/* Initialize emulator */

reg_mem_init();
loader (argc, argv);
/*     
reg_mem_device_init(PORT3, UART_device, TXDONE);*/
#ifdef IE_TEST
reg_mem_device_init(PORT0, TIMER_device, 0x00);
#endif
opc_size_init();
cache_mem_init();
getchar();
run_machine();
#ifdef VEIW_CACHE
veiw_cache();
#endif
getchar();
return 0;
}

/*
  Sample S19 srecord-extraction program
  - extract bytes from an s-record in a file using sscanf()
  - sscanf() appears to require ints and unsigned ints only
  - can be used to verify assignment 1 records or act as part 
    of the loader in assignment 2
  - turn on diagnostics with define DEBUG
  
  ECED 3403
  28 June 14
*/
#include <stdio.h>
#include <stdlib.h>

#define DEBUG
#define LINE_LEN   256
#define PM_SZ 63556
#define BYTE unsigned char
#define WORD unsigned short


enum SREC_ERRORS   {MISSING_S, BAD_TYPE, CHKSUM_ERR};

char *srec_diag[] = {
"Invalid srec - missing 'S'",
"Invalid srec - bad rec type",
"\nInvalid srec - chksum error"};

char srec[LINE_LEN];
FILE *fp;
WORD pc;
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
unsigned char mem[3][PM_SZ];	/* 1- regmem, 2- prog mem and 3 for data mem */

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
				 write values of temp to memory */
			switch(srtype)
			     {
			     	case 1: /* s1 program_mem */
			     	case 2: /* s2 data memory */
			     		for(i=0; i<length; i++)
			     		{
			     			/* address still holds starting loc for loading */
			     			mem[srtype][address++]= temp[i];
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
			     			/* reg_mem initiliazer called first */
			     			/* reg_mem[address].contents = temp[i]; */
			     			mem[srtype][address++]= temp[i];
			     			address++;
			     		}
			    	#ifdef DEBUG 
			    	printf("Register memory \n");
			    	#endif
			     	break;
			     }
			#ifdef DEBUG
			address-=length;
			printf("start printing from address is %x \n", address);
			for(i=0; i<length; i++)
			{
				
				printf("contents of memory loc %4x is: %2x \n", address, mem[srtype][address]);
				address++;
			}
			#endif
			}
	}
}
	 
#ifdef DEBUG
printf("pc is %4x \n", pc);
printf("\n File read - no errors detected\n");
#endif
fclose(fp);
getchar();
}

main(int argc, char *argv[])
{
loader (argc, argv);
}
 

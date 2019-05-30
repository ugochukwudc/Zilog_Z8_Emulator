tcount =0;
fcount=0;
cexec=0;

/************editting the call, DJNZ, JP and JR ******************/
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
/******IF ADDITION ******/
//can force tcount, fcount, cexec to zero
tcount =0;
fcount=0;
cexec=0;
/***********************/
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
/******IF ADDITION ******/
//can force tcount, fcount, cexec to zero
tcount =0;
fcount=0;
cexec=0;
/***********************/
#ifdef VEIW_MEM
printf("pc holds : %4x \n", pc);
#endif
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

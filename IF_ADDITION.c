///////////////Instruction Decode ////////////////////////////////////////
lownib>=8
switch lownib 
case 0x0F
switch highnib
case 0x01: /* IF instuction */
/* fetch next byte */
dst = prog_mem_fetch();
regval = MSN(dst); // condition code
src = LSN(dst);
dst = src>>2;
tcount = dst&0x11; // retrieve tt
fcount = src&0x11; // retrieve ff
cexec = cond_handler(regval); // holds 1 if condition is true, and 0 if false
/* store flags in temp variable */
tempflags = read_rm(FLAGS);
#ifdef TEST_IF
printf("If encountered, tcount %1x, fcount: %1x : cond : %x \n", tcount, fcount, cexec);
#endif	
break;

/* Inserted into JUMP sections of the JP, JR, DJNZ and CALL instructions */
/***********IF ADDITION *************/
tcount =0;
fcount=0;
cexec=0;
#ifdef IF_TEST
printf("flow of control tcount: %x fcount: %x cexec: %x \n", tcount, fcount, cexec);
#endif
/***********************************/ 

/************END of instruction CYCLE ***********************************/
/**********************************IF ADDITION ****************************/
int i;
if(cexec) // cexec is 1
{
if(tcount>0x00 && tcount<0x04) //sanity check
{
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
	  	inst = prog_mem_fetch(); // fetch opcode 
	  	pc +=opcode_size[MSN(inst)][LSN(inst)]; // increment pc by size of operands for opcode
	  }
	  fcount =0;
}
/* restore flags */
write_rm(FLAGS, tempflags);
}
else { // cexec == 0,  execute false part only 
if(tcount>0x00 && tcount<0x04)
{
	for(i=tcount; i>0; i--)
	{
		inst = prog_mem_fetch(); // fetch opcode
	  	pc +=opcode_size[MSN(inst)][LSN(inst)]; // increment pc by size of operands for opcode
	}
	tcount =0;
}
else if(fcount>0x00 && fcount<0x04)
{
	/* execute false part */
	fcount--; //decrement false count
	/* restore Flags */
	write_rm(FLAGS, tempflags);
}
/* restore flags */
write_rm(FLAGS, tempflags);
}
/****************initializing opcode_size[][]*************************/
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
		default:
			ans =1;
		break;
	}
	
	return ans;
}
BYTE h,l;
for (h=0, h<=0x0f; h++)
{
	for(l=0; l<=0x0f; l++)
	{
		opcode_size[h][l]=op_size(h,l);
	}
}

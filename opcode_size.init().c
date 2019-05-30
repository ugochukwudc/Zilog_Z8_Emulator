#include "Z8_IE.h"
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


main()
{
opc_size_init();
}

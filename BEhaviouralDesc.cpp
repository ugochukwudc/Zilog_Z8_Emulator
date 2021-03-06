IF is decoded 
FETCH NEXT BYTE
EXTRACT HNIB AND LNIB
HNIB IS CONDITION CODE
EXTRACT MOST SIGNIFICANT BITS AND LEAST SIGNIFICANT BITS FROM LOWER NIBBLE
MOST SIGNIFICANT IS TCOUNT, LEAST SIGNIFICANT IS FCOUNT
SAVE STATE, STORE FLAGS IN TEMPFLAGS
CEXEC = CHECK CONDITION CODE
/* IF TRUE CEXEC IS 1, ELSE CEXEC IS 0 */

/* AT THE END OF EACH INSTRUCTION CYCLE */
IF CEXEC IS 1 /* EXECUTE TRUE PART OF COND STATEMENT */
	IF TCOUNT IS GREATER THAN 0
		DECREMENT TCOUNT
		RESTORE STATE, FLAGS <-- TEMPFLAGS
		/* ALLOW EXECUTION TO CONTINUE */
	ENDIF
	/* TRUE PART IS DONE EXECUTING */
	ELSE IF FCOUNT IS GREATER THAN 0
		/* SKIP FALSE PART */
		WHILE FCOUNT IS GREATER THAN 0
			FETCH NEXT INSTRUCTION FROM PROG_MEM
			INCREMENT PC BY OPCODE_SIZE BYTES
			DECREMENT FCOUNT BY 1
		ENDWHILE 
	ENDELSE
	RETORE STATE
ENDIF
ELSE /* CEXEC IS 0 
		EXECUTE FALSE PART OF COND STATEMENT */
	IF TCOUNT IS GREATER THAN 0
		WHILE TCOUNT IS GREATER THAN 0
			FETCH NEXT INSTRUCTION
			INCREMENT PC BY OPCODE_SIZE BYTES
			DECREMENT TCOUNT BY 1
		ENDWHILE
	ENDIF
	ELSE IF FCOUNT IS GREATER THAN 0
		DECREMENT FCOUNT
		RESTORE STATE, FLAGS<--TEMPFLAGS
	ENDELSE
	RESTORE STATE
ENDELSE 



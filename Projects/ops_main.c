/*
 *  Copyright (c) 1995 by the Regents of the University of Minnesota,
 *  Peter Bergner, David Engebretsen, and Aaron Sawdey
 *
 *  All rights reserved.
 *
 *  Permission is given to use, copy, and modify this software for any
 *  non-commercial purpose as long as this copyright notice is not
 *  removed.  All other uses, including redistribution in whole or in
 *  part, are forbidden without prior written permission.
 *
 *  This software is provided with absolutely no warranty and no support.
 */

#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include "ops_opcodes.h"
#include "isa_config.h"
#include "machine_globals.h"
#include "ops_headers.h"
#include "asm-sym.h"
#include "../fasttimes/common.h"
#include "trace.h"

void simulate(void)
{
    byte  *pc = PC_PTR(get_start_addr("_main"));
    word  *ireg;
    float *freg;

    final_pc = 0;

    /* Get local pointers to the register files, this greatly improves
       the simulation time on machines that pass parameters in registers
    */
    ireg = r;
    freg = f;

    if (interactive)
        set_break_at(PC_ADDR(pc),SOFT_BREAK,0);

    if (verbose) {
	fprintf(stderr,"Starting simulation at address: 0x%08x\n",PC_ADDR(pc));
	fprintf(stderr,"\tmemSize = %d = 0x%08x\n",mem_size,mem_size);
    }

    /* Loop until the "exit trap" instruction clears the PC.
       The final PC can be found in "final_pc".
    */
    for(;pc;)
    {   uword insn   = READ_PTR_INSN(pc);
        uword opcode = insn & OPCODE_MASK;
	if (opcode)
	{   opcode = SHIFT_OPCODE(opcode);
     	    pc = insns[opcode](pc,insn,ireg,freg);
	} else
     	    pc = funcs[READ_FUNC(insn)](pc,insn,ireg,freg);
   }
   if (verbose)
        fprintf(stderr,"Exit trap received at address 0x%08x\n",
	    PC_ADDR(final_pc));

}



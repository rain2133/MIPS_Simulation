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

extern FILE *yyin;
void print_usage(int quit);
int get_fast_args(char *str, char **new_args[]);
extern char *optarg;
extern int optind;
int getopt (int argc, char *const *argv, const char *optstring);

void main(int argc, char *argv[])
{
    Symbol *s;
    int c, i, fast_argc;
    int errflg = 0;
    char **fast_argv = NULL;
    uword stack_pointer, pv, ps;

#ifdef DEBUG_TIME
    double tbegin, tend;
    tbegin = gettime();
#endif

    cmd = ((cmd=strrchr(argv[0],'/')) == NULL) ? argv[0] : cmd + 1;
    if (argc == 1) print_usage(-1);

#if 1
    version      = FAST_VERSION;
#else
    version      = "0.96";
#endif
    debug_output = FALSE;
    interactive  = FALSE;
    stats_info   = FALSE;
    trace_info   = FALSE;
    verbose      = FALSE;
    debug_asm    = FALSE;
    debug_data   = FALSE;
#ifdef DEBUG_INSNS
    debug_insns  = FALSE;
#endif

    mp_mode = tNULL;
    pid = parent_pid = (int)getpid();
    mem_size = DEFAULT_MEM_SIZE;

    set_wait_insns(-1);
    current_block = -1;
    num_blocks = 0;

    /* Initialize the symbol table for the assembly file lexer/parser */
    initsymbol();

    set_addr_state(tDATA);
    set_addr(0);
    set_addr_state(tTEXT);
    set_addr(0);
    set_addr_state(tSHARED);
    set_addr(0);

    while ((c = getopt(argc, argv, "fistva:d:m:")) != -1)
      switch (c) {
        case 'a': fast_argc = get_fast_args(optarg,&fast_argv); break;
	case 'd':
	    if (!strcmp(optarg,"ASM"))
		debug_asm = TRUE;
	    else if (!strcmp(optarg,"DATA"))
		debug_data = TRUE;
	    else if (!strcmp(optarg,"INSNS"))
#ifdef DEBUG_INSNS
	        debug_insns = TRUE;
#else
	    {   fprintf(stderr,"error:\toption -dINSNS is currently invalid\n");
		fprintf(stderr,"\trecompile \"%s\" with -DDEBUG_INSNS\n",cmd);
	        errflg++;
	    }
#endif
	    else
	        errflg++;
    	    break;
        case 'f': debug_output = TRUE; break;
        case 'i': interactive  = TRUE; break;
        case 'm': mem_size = (int)STR_2_INT(optarg,NULL,0); break;
        case 's': stats_info   = TRUE; break;
        case 't': trace_info   = TRUE; break;
        case 'v': verbose      = TRUE; break;
        case '?': default: errflg++; break;
      }
    if (errflg || ((nfiles=argc-optind) == 0))
        print_usage(-1);

    if ((filenames=(char **)malloc(sizeof(char *)*(nfiles))) == NULL)
    {   fprintf(stderr,"error: filenames malloc()\n");
	exit(-1);
    }
    for (nfiles=0; optind < argc; optind++)
    {   char *file = argv[optind];
	if (access(file, R_OK))
	{   fprintf(stderr,"error: cannot open file %s for reading\n",file);
	    errflg++;
	} else
	    filenames[nfiles++]=file;
    }
    if (errflg) exit(-1);

    if (debug_output)
    {   set_base_name(file_basename,filenames[0]);
        init_output();
    } else
        file_basename[0] = '\0';

    /* We need to do these *BEFORE* we parse the files! */
    init_regs(DEFAULT_IREG_SIZE, DEFAULT_FREG_SIZE);
    init_mem(mem_size);
    init_insns();
    init_break_table();

    /* "memSize" can be used in the asm files, so we need to add it to the
	symbol table before we parse the files.
    */
    s=newsymbol("memSize",0);
    s->flags |= (tGLOBAL|tLABEL); 
    s->addr = mem_size;

    /* Initialize the stack pointer (r29) and place all the incoming
       arguments on the stack (ie, argc and argv)
    */
    if (fast_argv == NULL) fast_argc = get_fast_args("",&fast_argv);

    /* Initialize the stackpointer to the largest double aligned address */
    stack_pointer = mem_size - BYTES_PER_BYTE;
    MAKE_ALIGNED_DOWN(stack_pointer,DOUBLE_ALIGNED);

    /* Reserve space for "argc" and "argv" (1 int + 1 pointer) */
    stack_pointer -= 2*BYTES_PER_WORD;

    /* Reserve space for the arguments:
          string length + NULL terminator + char pointer
    */
    for(i=0; i < fast_argc ;i++)
      stack_pointer -= (strlen(fast_argv[i]) + BYTES_PER_CHAR + BYTES_PER_WORD);
    MAKE_ALIGNED_DOWN(stack_pointer,DOUBLE_ALIGNED);
    WRITE_MEM_WORD(stack_pointer,fast_argc);
    pv = stack_pointer + BYTES_PER_WORD;
    WRITE_MEM_WORD(pv,pv+BYTES_PER_WORD);
    pv += BYTES_PER_WORD;
    ps = pv + fast_argc*BYTES_PER_WORD;
    /* Now copy the args into memory */
    for(i=0; i < fast_argc ;i++)
    {   WRITE_MEM_UWORD(pv,ps);
        strcpy((char *)MEM_PTR(ps),fast_argv[i]);
        pv += BYTES_PER_WORD;
        ps += (strlen(fast_argv[i]) + BYTES_PER_CHAR);
    }
    r[29] = stack_pointer;

    errors = 0;

    if (verbose) fprintf(stderr,"parsing: ");
    /* 
     *  pass 1: collect symbol info and assign addresses. 
     */

    set_addr_state(tDATA);
    pass=1;
    set_wait_insns(-1);
    
    for(i=0; i < nfiles ;i++) {
      curfile=i;
      yylineno = 1;
      if((yyin = fopen(filenames[i],"r")) == NULL)
      {   char err_str[MAX_TEXT_LEN];
	  sprintf(err_str,"\nerror: can't open %s for reading\n",filenames[i]);
	  perror(err_str) ;
	  errors++;
      } else
      {   if (verbose) fprintf(stderr,".");
	  yyparse();
          fclose(yyin);
      }
    }

    if(errors > 0) exit(1);

    set_addr_state(tDATA);
    shared_mem_start = pagealign(get_addr());
    set_addr_state(tSHARED);
    ssize = get_addr();

    symbol_analysis();

    if(errors > 0) exit(1);

    if (stats_info) init_stats();
    ssize = pagealign(ssize+MAX_BARS*sizeof(word));

    /*
     *  pass 2: produce output. 
     */

    set_addr_state(tSHARED);
    set_addr(shared_mem_start + MAX_BARS*sizeof(word));
    set_addr_state(tDATA);
    pass=2;
    set_wait_insns(-1);

    for(i=0; i < nfiles ;i++) {
      curfile=i;
      yylineno = 1;
      yyin = fopen(filenames[i],"r");
      if (verbose) fprintf(stderr,".");
      yyparse();
      fclose(yyin);
    }
    if (verbose) fprintf(stderr,"\n");

    if(errors > 0) exit(1);
    pass=3;

    /* Quit if they want this debug level */
    if (debug_asm || debug_data) exit(0);

    if (mp_mode & tFORK)
        reset_child_pids();

    if (mp_mode & (tSHARED|tBARRIER))
        share_mem((void *)MEM_PTR(shared_mem_start),ssize);

    if (mp_mode & tBARRIER)
    {   bar_counts = (int *)MEM_PTR(shared_mem_start);
	if (init_bars(MAX_BARS))
	{   fprintf(stderr,"error: init_bars(%d)\n",MAX_BARS);
	    exit(-1);
	}
    }

    if (trace_info)
    {   int  new_pid;
        char trace_str[64];
        char mem_size_str[32];
        char *timer = "./fasttimes";
        
        sprintf(trace_str,"%d",pid);
        sprintf(mem_size_str, "%d", mem_size);
        trace_init(pid);

        if (verbose)
            fprintf(stderr,"execl(fasttimes, fasttimes, %s, %s, %s)\n",
                trace_str,mem_size_str,file_basename);
   
        if ((new_pid=(int)fork()) == 0)
        {   if (access(timer,X_OK) != 0)
	    {   perror("error: main BE timing simulator is not executable!\n");
		exit(-1);
	    }
	    execl(timer, "fasttimes",
            trace_str, mem_size_str, file_basename, (char *)0);
   
            /* If get here, exec call failed */
            perror("error: main BE exec() failed\n");
            exit(-1);
        } else if (new_pid == -1)
        {   perror("error: main BE fork() failed\n");
            exit(-1);
        }
	be_pid = new_pid;
    }

    /* Start the simulation */
    simulate();

    if (trace_info) trace_wrapup();
    if (stats_info) stats_wrapup();

    exit_sim(0);

#ifdef DEBUG_TIME
    tend = gettime();
    fprintf(stderr,"Total execution time (sec): %g\n",tend-tbegin);
#endif
}

/*
    Assemble the instruction into 32 bits per H&P
*/
int
assemble_insn(int insn_type,
                  int opcode, int rd, int rs1, int rs2, int immed)
{
    int insn = 0;
#ifdef DEBUG_ASSEMBLE
    fprintf(stderr,"\tassemble_insn(%d,0x%x,%d,%d,%d,%d) = ",
	insn_type,opcode,rd,rs1,rs2,immed);
#endif
    switch (insn_type)
    {
	/* R-TYPE special intructions */
	case R_TYPE_RRRXX: case R_TYPE_RRXXX: case R_TYPE_XRRXX:
	case R_TYPE_XRXXX: case R_TYPE_RXXXX:
	    insn |= WRITE_OPCODE(OP_SPECIAL);
	    insn |= WRITE_R1(rs1);
	    insn |= WRITE_R2(rs2);
	    insn |= WRITE_R3(rd);
	    insn |= WRITE_FUNC(opcode);
	    break;

	/* I-TYPE instructions */
	case I_TYPE_RRXIX: case I_TYPE_RXXIX: case I_TYPE_XRXXL:
	case I_TYPE_XXXXL: case I_TYPE_XRXXX: case I_TYPE_XXXXX:
	    insn |= WRITE_OPCODE(opcode);
	    insn |= WRITE_R1(rs1);
	    insn |= WRITE_R2(rd);
	    insn |= WRITE_IMMED(immed);
	    break;

	case I_TYPE_XRRIX:
	    insn |= WRITE_OPCODE(opcode);
	    insn |= WRITE_R1(rs1);
	    insn |= WRITE_R2(rs2);
	    insn |= WRITE_IMMED(immed);
	    break;

	/* J-TYPE instructions */
	case J_TYPE_XXXXL: case J_TYPE_XXXIX: case J_TYPE_XXXXX:
	    insn |= WRITE_OPCODE(opcode);
	    insn |= WRITE_OFFSET(immed);
	    break;

    }
#ifdef DEBUG_ASSEMBLE
    fprintf(stderr," 0x%08x\n",insn);
#endif
    return insn;
}


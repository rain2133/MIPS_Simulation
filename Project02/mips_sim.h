enum opcodes {
/* All R-Type Insns are SPECIAL insns: Opcode = 0  
   All R-Type Insns are SPECIAL insns: Opcode = 17
*/
  OP_RTYPE = 0, OP_FTYPE = 17,

/* I-Type Insns. Max range: 1 - 63 */
  /* I-Type ALU Insns. */
  
  OP_ADDI = 8,	OP_SLTI = 10,
  

  /* I-Type Branch and Jump Insns. */
  OP_BEQ = 4,   OP_BNE = 5,

  /* I-Type Load and Store Insns. */
  OP_LW = 35,    OP_SW = 43,
  OP_LWC1 = 49,  OP_SWC1 = 57,
  OP_MOVE = 47,

/* J-Type Insns. */
  OP_J = 2,

/* !!! Do *NOT* remove this entry !!!
   This is used to delimit the end of the opcode array, so we can check
   to see if you try to define more that 64 opcodes!
*/
  // NUM_OPCODES = 14
};

/* MIPS only has 6 bits for the opcode, which isn't enough to specify
   all of the implemented instructions.  To work around this, several
   instructions share the same "OP_SPECIAL" opcode.  To differentiate
   these instructions, we use the 11 bit function field.  The func values
   for these instructions are defined here.
   Max function range: 0 - 2047
*/

enum functions_R {
  OP_SLL = 0,	OP_ADD = 32, OP_JR = 8,

/* !!! Do *NOT* remove this entry !!!
   This is used to delimit the end of the function array, so we can check
   to see if you try to define more that 2048 functions!
*/
  // NUM_FUNCS = 3
};

enum functions_F {
  OP_ADDS = 0,	
  OP_MULS = 2,

/* !!! Do *NOT* remove this entry !!!
   This is used to delimit the end of the function array, so we can check
   to see if you try to define more that 2048 functions!
*/
  // NUM_FUNCS = 2
};
/*                                                              -*- c -*-
 * Operand definitions for the JavaScript byte-code.
 *
 * This file is automatically create from the operands.def file.
 * Editing is strongly discouraged.  You should edit the file
 * `extract-op-names.js' instead.
 */

DASM$op_names = new Array ();
DASM$op_data = new Array ();
DASM$op_flags = new Array ();
DASM$op_names[0]	= "halt";
DASM$op_data[0] 	= 0;
DASM$op_flags[0] 	= 0x0;
DASM$op_names[1]	= "done";
DASM$op_data[1] 	= 0;
DASM$op_flags[1] 	= 0x0;
DASM$op_names[2]	= "nop";
DASM$op_data[2] 	= 0;
DASM$op_flags[2] 	= 0x0;
DASM$op_names[3]	= "dup";
DASM$op_data[3] 	= 0;
DASM$op_flags[3] 	= 0x0;
DASM$op_names[4]	= "pop";
DASM$op_data[4] 	= 0;
DASM$op_flags[4] 	= 0x0;
DASM$op_names[5]	= "pop_n";
DASM$op_data[5] 	= 1;
DASM$op_flags[5] 	= 0x0;
DASM$op_names[6]	= "apop";
DASM$op_data[6] 	= 1;
DASM$op_flags[6] 	= 0x0;
DASM$op_names[7]	= "swap";
DASM$op_data[7] 	= 0;
DASM$op_flags[7] 	= 0x0;
DASM$op_names[8]	= "roll";
DASM$op_data[8] 	= 1;
DASM$op_flags[8] 	= 0x0;
DASM$op_names[9]	= "const";
DASM$op_data[9] 	= 4;
DASM$op_flags[9] 	= 0x0;
DASM$op_names[10]	= "const_null";
DASM$op_data[10] 	= 0;
DASM$op_flags[10] 	= 0x0;
DASM$op_names[11]	= "const_true";
DASM$op_data[11] 	= 0;
DASM$op_flags[11] 	= 0x0;
DASM$op_names[12]	= "const_false";
DASM$op_data[12] 	= 0;
DASM$op_flags[12] 	= 0x0;
DASM$op_names[13]	= "const_undefined";
DASM$op_data[13] 	= 0;
DASM$op_flags[13] 	= 0x0;
DASM$op_names[14]	= "const_i0";
DASM$op_data[14] 	= 0;
DASM$op_flags[14] 	= 0x0;
DASM$op_names[15]	= "const_i1";
DASM$op_data[15] 	= 0;
DASM$op_flags[15] 	= 0x0;
DASM$op_names[16]	= "const_i2";
DASM$op_data[16] 	= 0;
DASM$op_flags[16] 	= 0x0;
DASM$op_names[17]	= "const_i3";
DASM$op_data[17] 	= 0;
DASM$op_flags[17] 	= 0x0;
DASM$op_names[18]	= "const_i";
DASM$op_data[18] 	= 4;
DASM$op_flags[18] 	= 0x0;
DASM$op_names[19]	= "load_global";
DASM$op_data[19] 	= 4;
DASM$op_flags[19] 	= 0x1;
DASM$op_names[20]	= "store_global";
DASM$op_data[20] 	= 4;
DASM$op_flags[20] 	= 0x1;
DASM$op_names[21]	= "load_arg";
DASM$op_data[21] 	= 1;
DASM$op_flags[21] 	= 0x0;
DASM$op_names[22]	= "store_arg";
DASM$op_data[22] 	= 1;
DASM$op_flags[22] 	= 0x0;
DASM$op_names[23]	= "load_local";
DASM$op_data[23] 	= 2;
DASM$op_flags[23] 	= 0x0;
DASM$op_names[24]	= "store_local";
DASM$op_data[24] 	= 2;
DASM$op_flags[24] 	= 0x0;
DASM$op_names[25]	= "load_property";
DASM$op_data[25] 	= 4;
DASM$op_flags[25] 	= 0x1;
DASM$op_names[26]	= "store_property";
DASM$op_data[26] 	= 4;
DASM$op_flags[26] 	= 0x1;
DASM$op_names[27]	= "load_array";
DASM$op_data[27] 	= 0;
DASM$op_flags[27] 	= 0x0;
DASM$op_names[28]	= "store_array";
DASM$op_data[28] 	= 0;
DASM$op_flags[28] 	= 0x0;
DASM$op_names[29]	= "nth";
DASM$op_data[29] 	= 0;
DASM$op_flags[29] 	= 0x0;
DASM$op_names[30]	= "cmp_eq";
DASM$op_data[30] 	= 0;
DASM$op_flags[30] 	= 0x0;
DASM$op_names[31]	= "cmp_ne";
DASM$op_data[31] 	= 0;
DASM$op_flags[31] 	= 0x0;
DASM$op_names[32]	= "cmp_lt";
DASM$op_data[32] 	= 0;
DASM$op_flags[32] 	= 0x0;
DASM$op_names[33]	= "cmp_gt";
DASM$op_data[33] 	= 0;
DASM$op_flags[33] 	= 0x0;
DASM$op_names[34]	= "cmp_le";
DASM$op_data[34] 	= 0;
DASM$op_flags[34] 	= 0x0;
DASM$op_names[35]	= "cmp_ge";
DASM$op_data[35] 	= 0;
DASM$op_flags[35] 	= 0x0;
DASM$op_names[36]	= "cmp_seq";
DASM$op_data[36] 	= 0;
DASM$op_flags[36] 	= 0x0;
DASM$op_names[37]	= "cmp_sne";
DASM$op_data[37] 	= 0;
DASM$op_flags[37] 	= 0x0;
DASM$op_names[38]	= "sub";
DASM$op_data[38] 	= 0;
DASM$op_flags[38] 	= 0x0;
DASM$op_names[39]	= "add";
DASM$op_data[39] 	= 0;
DASM$op_flags[39] 	= 0x0;
DASM$op_names[40]	= "mul";
DASM$op_data[40] 	= 0;
DASM$op_flags[40] 	= 0x0;
DASM$op_names[41]	= "div";
DASM$op_data[41] 	= 0;
DASM$op_flags[41] 	= 0x0;
DASM$op_names[42]	= "mod";
DASM$op_data[42] 	= 0;
DASM$op_flags[42] 	= 0x0;
DASM$op_names[43]	= "neg";
DASM$op_data[43] 	= 0;
DASM$op_flags[43] 	= 0x0;
DASM$op_names[44]	= "and";
DASM$op_data[44] 	= 0;
DASM$op_flags[44] 	= 0x0;
DASM$op_names[45]	= "not";
DASM$op_data[45] 	= 0;
DASM$op_flags[45] 	= 0x0;
DASM$op_names[46]	= "or";
DASM$op_data[46] 	= 0;
DASM$op_flags[46] 	= 0x0;
DASM$op_names[47]	= "xor";
DASM$op_data[47] 	= 0;
DASM$op_flags[47] 	= 0x0;
DASM$op_names[48]	= "shift_left";
DASM$op_data[48] 	= 0;
DASM$op_flags[48] 	= 0x0;
DASM$op_names[49]	= "shift_right";
DASM$op_data[49] 	= 0;
DASM$op_flags[49] 	= 0x0;
DASM$op_names[50]	= "shift_rright";
DASM$op_data[50] 	= 0;
DASM$op_flags[50] 	= 0x0;
DASM$op_names[51]	= "iffalse";
DASM$op_data[51] 	= 4;
DASM$op_flags[51] 	= 0x2;
DASM$op_names[52]	= "iftrue";
DASM$op_data[52] 	= 4;
DASM$op_flags[52] 	= 0x2;
DASM$op_names[53]	= "call_method";
DASM$op_data[53] 	= 4;
DASM$op_flags[53] 	= 0x1;
DASM$op_names[54]	= "jmp";
DASM$op_data[54] 	= 4;
DASM$op_flags[54] 	= 0x2;
DASM$op_names[55]	= "jsr";
DASM$op_data[55] 	= 0;
DASM$op_flags[55] 	= 0x0;
DASM$op_names[56]	= "return";
DASM$op_data[56] 	= 0;
DASM$op_flags[56] 	= 0x0;
DASM$op_names[57]	= "typeof";
DASM$op_data[57] 	= 0;
DASM$op_flags[57] 	= 0x0;
DASM$op_names[58]	= "new";
DASM$op_data[58] 	= 0;
DASM$op_flags[58] 	= 0x0;
DASM$op_names[59]	= "delete_property";
DASM$op_data[59] 	= 4;
DASM$op_flags[59] 	= 0x1;
DASM$op_names[60]	= "delete_array";
DASM$op_data[60] 	= 0;
DASM$op_flags[60] 	= 0x0;
DASM$op_names[61]	= "locals";
DASM$op_data[61] 	= 2;
DASM$op_flags[61] 	= 0x0;
DASM$op_names[62]	= "min_args";
DASM$op_data[62] 	= 1;
DASM$op_flags[62] 	= 0x0;
DASM$op_names[63]	= "load_nth_arg";
DASM$op_data[63] 	= 0;
DASM$op_flags[63] 	= 0x0;
DASM$op_names[64]	= "with_push";
DASM$op_data[64] 	= 0;
DASM$op_flags[64] 	= 0x0;
DASM$op_names[65]	= "with_pop";
DASM$op_data[65] 	= 1;
DASM$op_flags[65] 	= 0x0;
DASM$op_names[66]	= "try_push";
DASM$op_data[66] 	= 4;
DASM$op_flags[66] 	= 0x2;
DASM$op_names[67]	= "try_pop";
DASM$op_data[67] 	= 1;
DASM$op_flags[67] 	= 0x0;
DASM$op_names[68]	= "throw";
DASM$op_data[68] 	= 0;
DASM$op_flags[68] 	= 0x0;
DASM$op_names[69]	= "iffalse_b";
DASM$op_data[69] 	= 4;
DASM$op_flags[69] 	= 0x2;
DASM$op_names[70]	= "iftrue_b";
DASM$op_data[70] 	= 4;
DASM$op_flags[70] 	= 0x2;
DASM$op_names[71]	= "add_1_i";
DASM$op_data[71] 	= 0;
DASM$op_flags[71] 	= 0x0;
DASM$op_names[72]	= "add_2_i";
DASM$op_data[72] 	= 0;
DASM$op_flags[72] 	= 0x0;
DASM$op_names[73]	= "load_global_w";
DASM$op_data[73] 	= 4;
DASM$op_flags[73] 	= 0x1;
DASM$op_names[74]	= "jsr_w";
DASM$op_data[74] 	= 4;
DASM$op_flags[74] 	= 0x1;

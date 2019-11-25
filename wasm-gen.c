/*
 *  WebAssembly code generator for TCC
 *
 *  Copyright (c) 2019 David A Roberts
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifdef TARGET_DEFS_ONLY

/* number of available registers */
#define NB_REGS 5

/* a register can belong to several classes. The classes must be
   sorted from more general to more precise (see gv2() code which does
   assumptions on it). */
#define RC_INT     0x0001 /* generic integer register */
#define RC_FLOAT   0x0002 /* generic float register */
#define RC_CMP0    0x0004
#define RC_CMP1    0x0008
#define RC_IRET    RC_INT /* function return: integer register */
#define RC_LRET    RC_INT /* function return: second integer register */
#define RC_FRET    RC_FLOAT /* function return: float register */

/* pretty names for the registers */
enum {
    REG0 = 0,
    REG1,
    REG2,
    REG_CMP0,
    REG_CMP1,
};

ST_DATA const int reg_classes[NB_REGS] = {
    /* REG0 */ RC_INT,
    /* REG1 */ RC_INT,
    /* REG2 */ RC_INT,
    /* REG_CMP0 */ RC_CMP0,
    /* REG_CMP1 */ RC_CMP1,
};

/* return registers for function */
#define REG_IRET REG0 /* single word int return register */
#define REG_LRET REG1 /* second word return register (for long long) */
#define REG_FRET REG2 /* float return register */

/* pointer size, in bytes */
#define PTR_SIZE 4

/* long double size and alignment, in bytes */
#define LDOUBLE_SIZE  8
#define LDOUBLE_ALIGN 8

/* maximum alignment (for aligned attribute support) */
#define MAX_ALIGN     8

/******************************************************/
#else /* ! TARGET_DEFS_ONLY */
/******************************************************/

#include "tcc.h"

#define DATA_OFFSET 16

#define log(...) { \
    int _log_i; \
    for (_log_i = 0; _log_i < vtop - pvtop; _log_i++) fprintf(stderr, " "); \
    fprintf(stderr, "\033[0;31m"); \
    fprintf(stderr, __VA_ARGS__); \
    fprintf(stderr, "\033[0m"); \
    fprintf(stderr, "\n"); \
}

/******************************************************/

const char *lookup_op(int op) {
    if (op == TOK_PDIV) op = '/';
    switch(op) {
        case '+': return "add";
        case '-': return "sub";
        case '&': return "and";
        case '^': return "xor";
        case '|': return "or";
        case '*': return "mul";
        case TOK_SHL: return "shl";
        case TOK_SHR: return "shr_u";
        case TOK_SAR: return "shr_s";
        case '/':     return "div_s";
        case TOK_UDIV: return "div_u";
        case '%':      return "rem_s";
        case TOK_UMOD: return "rem_u";
        case TOK_EQ: return "eq";
        case TOK_NE: return "ne";
        case TOK_LT: return "lt_s";
        case TOK_LE: return "le_s";
        case TOK_GT: return "gt_s";
        case TOK_GE: return "ge_s";
        case TOK_ULT: return "lt_u";
        case TOK_ULE: return "le_u";
        case TOK_UGT: return "gt_u";
        case TOK_UGE: return "ge_u";
        default: tcc_error("unknown op %#x", op);
    }
}

const char *lookup_type(int t) {
    switch(t & VT_BTYPE) {
        case VT_PTR:
        case VT_INT: return "i32";
        case VT_FLOAT: return "f32";
        default: tcc_error("unknown type %#x", t);
    }
}

void wasm_init(void) {
    printf("(module\n");
    printf("(import \"wasi_unstable\" \"fd_write\"");
    printf(" (func $__wasi_fd_write (param i32 i32 i32 i32) (result i32)))\n");
    printf("(memory 1)\n");
    printf("(export \"memory\" (memory 0))\n");
    printf("(global $SP (mut i32) (i32.const %d))\n", 4096);
}

int loc_offset = 0;

void gfunc_prolog(CType *func_type) {
    int addr = 0, t;
    Sym *sym = func_type->ref;
    func_vt = sym->type;
    func_var = (sym->c == FUNC_ELLIPSIS);
    log("gfunc_prolog %s", funcname);

    printf("(func $%s (export \"%s\") ", funcname, funcname);
    if (sym->f.func_type != FUNC_NEW)
        tcc_error("unsupported function prototype");

    while ((sym = sym->next) != NULL) {
        t = sym->type.t;
        sym_push(sym->v & ~SYM_FIELD, &sym->type,
                 VT_LOCAL | lvalue_type(sym->type.t), addr);
        printf("(param $p%d %s) ", addr, lookup_type(t));
        addr++;
    }

    t = func_vt.t;
    if (t) printf("(result %s)", lookup_type(t));
    printf("\n");
    for (int i = 0; i < NB_REGS; i++)
        printf("(local $r%d i32)\n", i);
    loc_offset = loc;
}

void load(int r, SValue *sv) {
    int v = sv->r & VT_VALMASK, fc = sv->c.i, ft = sv->type.t;
    log("load r=%d v=%#x fc=%d ft=%d", r, v, fc, ft);

    if (v == VT_JMP || v == VT_JMPI) {
        if ((fc & 0xff) - BLOCK_VT_JMP != v - VT_JMP)
            tcc_error("block doesn't support VT_JMP/I");
        gsym(fc);
        printf("set_local $r%d\n", r);
        return;
    }

    printf("(set_local $r%d (", r);

    if (sv->r & VT_LVAL) {
        if (v == VT_LOCAL) {
            if (fc >= 0) {
                printf("get_local $p%d", fc);
            } else {
                printf("i32.load (i32.add (get_global $SP) (i32.const %d))",
                    fc - loc_offset);
            }
        } else if (v == VT_CONST) {
            tcc_error("handle globals");
        } else {
            if ((ft & VT_BTYPE) == VT_FLOAT) {
                printf("f32.load");
            } else if ((ft & VT_BTYPE) == VT_DOUBLE) {
                printf("f64.load");
            } else if ((ft & VT_BTYPE) == VT_LDOUBLE) {
                printf("f64.load");
            } else if ((ft & VT_TYPE) == VT_BYTE) {
                printf("i32.load8_s");
            } else if ((ft & VT_TYPE) == (VT_BYTE | VT_UNSIGNED)) {
                printf("i32.load8_u");
            } else if ((ft & VT_TYPE) == VT_SHORT) {
                printf("i32.load16_s");
            } else if ((ft & VT_TYPE) == (VT_SHORT | VT_UNSIGNED)) {
                printf("i32.load16_u");
            } else {
                printf("i32.load");
            }
            printf(" (get_local $r%d)", v);
        }
    } else {
        if (v == VT_CONST) {
            if (sv->r & VT_SYM) {
                ElfSym *esym = elfsym(sv->sym);
                printf("i32.const %d (; %s ;)", DATA_OFFSET + esym->st_value, get_tok_str(sv->sym->v, NULL));
            } else {
                printf("i32.const %d", fc);
            }
        } else if (v == VT_LOCAL) {
            if (fc >= 0) {
                tcc_error("can't load address of function parameter");
            } else {
                printf("i32.add (get_global $SP) (i32.const %d)", fc - loc_offset);
            }
        } else if (v == VT_CMP) {
            printf("i32.%s (get_local $r%d) (get_local $r%d)",
                lookup_op(fc), REG_CMP0, REG_CMP1);
        } else {
            printf("get_local $r%d", v);
        }
    }

    printf("))\n");
}

void store(int r, SValue *sv) {
    int v = sv->r & VT_VALMASK, fc = sv->c.i, ft = sv->type.t;
    log("store r=%d v=%#x fc=%d ft=%d", r, v, fc, ft);

    if (v == VT_LOCAL) {
        printf("(i32.store (i32.add (get_global $SP) (i32.const %d)) (get_local $r%d))\n",
            fc - loc_offset, r);
    } else if (v == VT_CONST) {
        tcc_error("handle globals");
    } else {
        tcc_error("indirect store");
    }
}

void gfunc_call(int nb_args) {
    Sym *return_sym;
    log("gfunc_call nb_args=%d", nb_args);

    for (int i = 0; i < nb_args; i++) {
        vrotb(nb_args - i);
        if ((vtop->type.t & VT_BTYPE) == VT_STRUCT) {
            tcc_error("structures passed as value not handled yet");
        } else {
            printf("get_local $r%d\n", gv(RC_INT));
        }
        vtop--;
    }

    return_sym = vtop->type.ref;
    for (Sym *cur = return_sym->next; cur; cur = cur->next)
        log("fixed parameter type %d", cur->type.t);
    if (return_sym->f.func_type == FUNC_ELLIPSIS)
        log("ellipsis parameter");

    if ((vtop->r & (VT_VALMASK | VT_LVAL)) == VT_CONST) {
        const char* name = get_tok_str(vtop->sym->v, NULL);
        int t = return_sym->type.t;
        int local = (loc - loc_offset) & -8; // keep aligned
        if (return_sym->f.func_type != FUNC_NEW)
            tcc_error("unsupported function prototype");
        printf("(set_global $SP (i32.add (get_global $SP) (i32.const %d)))\n", local);
        printf("call $%s\n", name);
        if (t) printf("set_local $r%d\n", REG_IRET);
        printf("(set_global $SP (i32.sub (get_global $SP) (i32.const %d)))\n", local);
    } else {
        gv(RC_INT);
        tcc_error("indirect call");
    }
    vtop--;
}

ST_FUNC int gfunc_sret(CType *vt, int variadic, CType *ret, int *ret_align, int *regsize) {
    *ret_align = 1;
    return 0;
}

int num_blocks = 0;

ST_FUNC int gblock(int t) {
    int k = t & 0xff, i = t >> 8;
    if (!i) i = ++num_blocks;
    if (k == BLOCK_IF) {
        printf("if $B%d\n", i);
    } else if (k == BLOCK_SWITCH) {
        printf("block $S%d\n", i);
        printf("i32.const 0\n");
    } else if (k == BLOCK_SWITCH_CASE) {
        int r = gv(RC_INT);
        printf("get_local $r%d\n", r);
        printf("i32.or\n");
        printf("tee_local $r%d\n", r);
        printf("get_local $r%d\n", r);
        printf("if $C%d\n", i);
        vtop--;
    } else {
        printf("block $B%d", i);
        if (k == BLOCK_VT_JMP || k == BLOCK_VT_JMPI) {
            printf(" (result i32)\n");
            printf("i32.const %d", k == BLOCK_VT_JMP);
        }
        printf("\n");
        if (k == BLOCK_LOOP) printf("loop $L%d\n", i);
    }
    return (i << 8) | (k & 0xff);
}

void gsym_addr(int t, int a) {
    tcc_error("gsym_addr");
}

void gsym(int t) {
    int k = t & 0xff, i = t >> 8;
    if (i == 0) {
        log("gsym(0)");
    } else if (k == BLOCK_IF_ELSE) {
        printf("else $B%d\n", i);
    } else if (k == BLOCK_SWITCH) {
        printf("end $S%d\n", i);
    } else if (k == BLOCK_SWITCH_CASE) {
        printf("end $C%d\n", i);
    } else {
        if (k == BLOCK_VT_JMP || k == BLOCK_VT_JMPI)
            printf("i32.eqz\n");
        if (k == BLOCK_LOOP) {
            printf("br $L%d\n", i);
            printf("end $L%d\n", i);
        }
        printf("end $B%d\n", i);
    }
}

int gjmp(int t) {
    int k = t & 0xff, i = t >> 8;
    if (i == 0) {
        tcc_error("gjmp(0)");
    } else if (nocode_wanted) {
        log("no code wanted");
    } else if (k == BLOCK_SWITCH) {
        printf("br $S%d\n", i);
    } else if (k == BLOCK_LOOP_CONTINUE) {
        printf("br $L%d\n", i);
    } else {
        printf("br $B%d\n", i);
    }
    return t;
}

void gjmp_addr(int a) {
    log("gjmp_addr");
}

int gtst(int inv, int t) {
    int k = t & 0xff, i = t >> 8, v = vtop->r & VT_VALMASK;
    log("gtst inv=%d k=%d i=%d", inv, k, i);
    if (nocode_wanted) {
        log("no code wanted");
    } else if (v == VT_CMP || v == VT_JMP || v == VT_JMPI) {
        vtop->c.i ^= inv;
        if (k == BLOCK_IF) {
            printf("get_local $r%d\n", gv(RC_INT));
            t = gblock(t);
        } else {
            int r = gv(RC_INT);
            if (i == 0) {
                t = gblock(t);
                i = t >> 8;
            }
            printf("(br_if $B%d (get_local $r%d))\n", i, r);
        }
    } else {
        tcc_error("gtst");
    }
    vtop--;
    return t;
}

void gen_opi(int op) {
    log("gen_opi op=%#x", op);
    if (TOK_ULT <= op && op <= TOK_GT) {
        gv2(RC_CMP0, RC_CMP1);
        vtop--;
        vtop->r = VT_CMP;
        vtop->c.i = op;
    } else {
        int r, fr;
        gv2(RC_INT, RC_INT);
        r = vtop[-1].r;
        fr = vtop[0].r;
        vtop--;
        printf("(set_local $r%d (i32.%s (get_local $r%d) (get_local $r%d)))\n",
            r, lookup_op(op), r, fr);
    }
}

/* generate a floating point operation 'v = t1 op t2' instruction. The
   two operands are guaranted to have the same floating point type */
void gen_opf(int op) {
    log("gen_opf op=%d", op);
}

/* convert integers to fp 't' type. Must handle 'int', 'unsigned int'
   and 'long long' cases. */
void gen_cvt_itof(int t) {
    log("gen_cvt_itof t=%d", t);
}

/* convert fp to int 't' type */
void gen_cvt_ftoi(int t) {
    log("gen_cvt_ftoi t=%d", t);
}

/* convert from one floating point type to another */
void gen_cvt_ftof(int t) {
    log("gen_cvt_ftof t=%d", t);
}

void ggoto(void) {
    tcc_error("computed goto");
}

/* Save the stack pointer onto the stack and return the location of its address */
ST_FUNC void gen_vla_sp_save(int addr) {
    tcc_error("variable length arrays unsupported for this target");
}

/* Restore the SP from a location on the stack */
ST_FUNC void gen_vla_sp_restore(int addr) {
    tcc_error("variable length arrays unsupported for this target");
}

/* Subtract from the stack pointer, and push the resulting value onto the stack */
ST_FUNC void gen_vla_alloc(CType *type, int align) {
    tcc_error("variable length arrays unsupported for this target");
}

void gfunc_epilog(void) {
    int t = func_vt.t;
    if (t) printf("get_local $r%d\n", REG_IRET);
    printf(")\n");
}

void wasm_end(void) {
    printf("(data (i32.const %d) \"", DATA_OFFSET);
    for (int i = 0; i < data_section->data_offset; i++)
        printf("\\%02x", data_section->data[i]);
    printf("\")\n");
    printf(")\n");
    exit(0);
}

/*************************************************************/
#endif
/*************************************************************/

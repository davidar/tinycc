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
#define NB_REGS 12

/* a register can belong to several classes. The classes must be
   sorted from more general to more precise (see gv2() code which does
   assumptions on it). */
#define RC_INT     0x0001
#define RC_LLONG   0x0002
#define RC_FLOAT   0x0004
#define RC_DOUBLE  0x0008
#define RC_IRET    0x0010
#define RC_LRET    0x0020
#define RC_FRET    0x0040
#define RC_DRET    0x0080

ST_DATA const int reg_classes[NB_REGS] = {
    RC_INT | RC_IRET,
    RC_INT,
    RC_INT,
    RC_LLONG | RC_LRET,
    RC_LLONG,
    RC_LLONG,
    RC_FLOAT | RC_FRET,
    RC_FLOAT,
    RC_FLOAT,
    RC_DOUBLE | RC_DRET,
    RC_DOUBLE,
    RC_DOUBLE,
};

/* return registers for function */
#define REG_IRET  0
#define REG_LRET  3
#define REG_FRET  6
#define REG_DRET  9

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

const int reg_types[NB_REGS] = {
    VT_INT,
    VT_INT,
    VT_INT,
    VT_LLONG,
    VT_LLONG,
    VT_LLONG,
    VT_FLOAT,
    VT_FLOAT,
    VT_FLOAT,
    VT_DOUBLE,
    VT_DOUBLE,
    VT_DOUBLE,
};

#define ALIGNMENT_MASK (-8)

#define log(...) { \
    int _log_i; \
    for (_log_i = 0; _log_i < vtop - pvtop; _log_i++) fprintf(stderr, " "); \
    fprintf(stderr, "\033[0;31m"); \
    fprintf(stderr, __VA_ARGS__); \
    fprintf(stderr, "\033[0m"); \
    fprintf(stderr, "\n"); \
}

int aCMP, bCMP, tCMP;

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
        case VT_FUNC:
        case VT_BYTE:
        case VT_SHORT:
        case VT_INT:    return "i32";
        case VT_LLONG:  return "i64";
        case VT_FLOAT:  return "f32";
        case VT_LDOUBLE:
        case VT_DOUBLE: return "f64";
        default: tcc_error("unknown type %#x", t);
    }
}

void wasm_init(void) {
    printf("(module\n");
    printf("(import \"wasi_unstable\" \"fd_write\"");
    printf(" (func $__wasi_fd_write (param i32 i32 i32 i32) (result i32)))\n");
    printf("(memory (export \"memory\") 2)\n");
}

void gfunc_prolog(CType *func_type) {
    int addr = 0, t;
    Sym *sym = func_type->ref;
    func_vt = sym->type;
    func_var = (sym->c == FUNC_ELLIPSIS);
    loc = 0;
    log("gfunc_prolog %s func_ind=%d", funcname, func_ind);
    printf("(elem (i32.const %d) $%s)\n", func_ind, funcname);

    printf("(func $%s (export \"%s\") ", funcname, funcname);
    if (sym->f.func_type != FUNC_NEW)
        tcc_error("unsupported function prototype");

    if ((func_vt.t & VT_BTYPE) == VT_STRUCT) {
        func_vc = addr++;
        printf("(param $p%d %s) ", func_vc, lookup_type(VT_PTR));
    }

    while ((sym = sym->next) != NULL) {
        t = sym->type.t;
        sym_push(sym->v & ~SYM_FIELD, &sym->type,
                 VT_LOCAL | lvalue_type(sym->type.t), addr);
        printf("(param $p%d %s) ", addr, lookup_type(t));
        addr++;
    }

    t = func_vt.t;
    if (t && (func_vt.t & VT_BTYPE) != VT_STRUCT)
        printf("(result %s)", lookup_type(t));
    printf("\n");
    for (int i = 0; i < NB_REGS; i++)
        printf("(local $r%d %s)\n", i, lookup_type(reg_types[i]));
}

void gsv(SValue *sv) {
    int v = sv->r & VT_VALMASK, fc = sv->c.i, ft = sv->type.t;
    if (v == VT_CONST) {
        if (sv->r & VT_SYM) {
            ElfSym *esym = elfsym(sv->sym);
            const char *section;
            switch(esym->st_shndx) {
                case 1: section = "FUNC"; break;
                case 2: section = "DATA"; break;
                default: tcc_error("unknown section index %d", esym->st_shndx);
            }
            printf("(i32.add (get_global $%s) (i32.const %d) (; %s ;))",
                section, esym->st_value, get_tok_str(sv->sym->v, NULL));
        } else {
            printf("%s.const %d", lookup_type(ft), fc);
        }
    } else if (v == VT_LOCAL) {
        if (fc >= 0) {
            tcc_error("can't load address of function parameter");
        } else {
            printf("(i32.add (get_global $SP) (i32.const %d))", fc);
        }
    } else {
        printf("get_local $r%d", v);
    }
    printf("\n");
}

void loads(SValue *sv) {
    int v = sv->r & VT_VALMASK, fc = sv->c.i, ft = sv->type.t;
    log("loads r=%#x v=%#x fc=%d ft=%d", sv->r, v, fc, ft);

    if (v == VT_JMP || v == VT_JMPI) {
        if ((fc & 0xff) - BLOCK_VT_JMP != v - VT_JMP)
            tcc_error("block doesn't support VT_JMP/I");
        gsym(fc);
        return;
    }

    if (v == VT_CMP) {
        printf("(%s.%s (get_local $r%d) (get_local $r%d))\n",
            lookup_type(tCMP), lookup_op(fc), aCMP, bCMP);
        return;
    }

    if (sv->r == (VT_LVAL | VT_LOCAL) && fc >= 0) {
        printf("get_local $p%d\n", fc);
        return;
    }

    gsv(sv);

    if (sv->r & VT_LVAL) {
        char s = (ft & VT_UNSIGNED) ? 'u' : 's';
        switch (ft & VT_BTYPE) {
            case VT_BYTE: printf("i32.load8_%c", s); break;
            case VT_SHORT: printf("i32.load16_%c", s); break;
            default: printf("%s.load", lookup_type(ft));
        }
        printf("\n");
    }
}

void load(int r, SValue *sv) {
    log("load %d", r);
    loads(sv);
    printf("set_local $r%d\n", r);
}

void store(int r, SValue *sv) {
    int v = sv->r & VT_VALMASK, fc = sv->c.i, ft = sv->type.t;
    log("store r=%d v=%#x fc=%d ft=%d", r, v, fc, ft);
    gsv(sv);
    printf("get_local $r%d\n", r);
    switch (ft & VT_BTYPE) {
        case VT_BYTE: printf("i32.store8"); break;
        case VT_SHORT: printf("i32.store16"); break;
        default: printf("%s.store", lookup_type(ft));
    }
    printf("\n");
}

int gvt(int t) {
    switch (t & VT_BTYPE) {
        case VT_PTR:
        case VT_BYTE:
        case VT_SHORT:
        case VT_INT:    return gv(RC_INT);
        case VT_LLONG:  return gv(RC_LLONG);
        case VT_FLOAT:  return gv(RC_FLOAT);
        case VT_LDOUBLE:
        case VT_DOUBLE: return gv(RC_DOUBLE);
        default: tcc_error("gvt(%s)", lookup_type(t));
    }
}

int greturn(int t) {
    switch (t & VT_BTYPE) {
        case VT_PTR:
        case VT_BYTE:
        case VT_SHORT:
        case VT_INT:    return REG_IRET;
        case VT_LLONG:  return REG_LRET;
        case VT_FLOAT:  return REG_FRET;
        case VT_LDOUBLE:
        case VT_DOUBLE: return REG_DRET;
        case VT_STRUCT:
        case VT_VOID:   return -1;
        default: tcc_error("return %s", lookup_type(t));
    }
}

void gfunc_call(int nb_args) {
    Sym *return_sym = vtop[-nb_args].type.ref;
    int t = return_sym->type.t;
    int local = loc & ALIGNMENT_MASK;
    int indirect = ((vtop[-nb_args].r & (VT_VALMASK | VT_LVAL)) != VT_CONST);
    int r = -1;
    log("gfunc_call nb_args=%d", nb_args);

    for (int i = 0; i < nb_args; i++) {
        vrotb(nb_args - i);
        if ((vtop->type.t & VT_BTYPE) == VT_STRUCT) {
            tcc_error("structures passed as value not handled yet");
        } else {
            printf("get_local $r%d\n", gvt(vtop->type.t));
        }
        vtop--;
    }

    if (indirect) r = gv(RC_INT);
    printf("(set_global $SP (i32.add (get_global $SP) (i32.const %d)))\n", local);
    if (indirect) {
        if (return_sym->f.func_type != FUNC_NEW)
            tcc_error("unsupported function prototype for indirect call");
        printf("(call_indirect ");
        for (Sym *cur = return_sym->next; cur; cur = cur->next)
            printf("(param %s) ", lookup_type(cur->type.t));
        if (t) printf("(result %s) ", lookup_type(t));
        printf("(get_local $r%d))\n", r);
    } else {
        const char* name = get_tok_str(vtop->sym->v, NULL);
        printf("call $%s\n", name);
    }
    r = greturn(t);
    if (r >= 0) printf("set_local $r%d\n", r);
    printf("(set_global $SP (i32.sub (get_global $SP) (i32.const %d)))\n", local);
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
        if (k == BLOCK_LOOP)
            printf("end $L%d\n", i);
        printf("end $B%d\n", i);
    }
}

int gjmp(int t) {
    int k = t & 0xff, i = t >> 8;
    if (k == BLOCK_RETURN) {
        int r = greturn(func_vt.t);
        if (r >= 0) printf("get_local $r%d\n", r);
        printf("return\n");
    } else if (i == 0) {
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
    tcc_error("gjmp_addr");
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

void gen_opx(int op) {
    int r = vtop[-1].r, fr = vtop[0].r, t = vtop->type.t;
    log("gen_op op=%#x", op);
    vtop--;
    if (TOK_ULT <= op && op <= TOK_GT) {
        aCMP = r;
        bCMP = fr;
        tCMP = t;
        vtop->r = VT_CMP;
        vtop->c.i = op;
    } else {
        printf("(set_local $r%d (%s.%s (get_local $r%d) (get_local $r%d)))\n",
            r, lookup_type(t), lookup_op(op), r, fr);
    }
}

void gen_opi(int op) {
    gv2(RC_INT, RC_INT);
    gen_opx(op);
}

void gen_opl(int op) {
    gv2(RC_LLONG, RC_LLONG);
    gen_opx(op);
}

void gen_opf(int op) {
    if (vtop->type.t == VT_FLOAT) {
        gv2(RC_FLOAT, RC_FLOAT);
    } else {
        gv2(RC_DOUBLE, RC_DOUBLE);
    }
    gen_opx(op);
}

void gen_cvt(int t) {
    int s = vtop->type.t, r = gvt(s);
    char us = (s & VT_UNSIGNED) ? 'u' : 's';
    char ut = (t & VT_UNSIGNED) ? 'u' : 's';
    printf("get_local $r%d\n", r);
    if (s == VT_INT && t == VT_LLONG) {
        printf("i64.extend_i32_%c", us);
    } else if (s == VT_LLONG && t == VT_INT) {
        printf("i32.wrap_i64");
    } else if (s == VT_FLOAT && t == VT_DOUBLE) {
        printf("f64.promote_f32");
    } else if (s == VT_DOUBLE && t == VT_FLOAT) {
        printf("f32.demote_f64");
    } else if (is_float(s) && !is_float(t)) {
        printf("%s.trunc_%s_%c", lookup_type(t), lookup_type(s), ut);
    } else if (!is_float(s) && is_float(t)) {
        printf("%s.convert_%s_%c", lookup_type(t), lookup_type(s), us);
    } else {
        tcc_error("can't cast %s to %s", lookup_type(s), lookup_type(t));
    }
    printf("\n");
    r = greturn(t);
    save_reg(r);
    printf("set_local $r%d\n", r);
    vtop->r = r;
    vtop->type.t = t;
}

void gen_cvt_itoi(int t) { gen_cvt(t); }
void gen_cvt_itof(int t) { gen_cvt(t); }
void gen_cvt_ftoi(int t) { gen_cvt(t); }
void gen_cvt_ftof(int t) { gen_cvt(t); }

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
    int r = greturn(func_vt.t);
    if (r >= 0) printf("get_local $r%d\n", r);
    printf(")\n");
    ind++;
}

void wasm_end(void) {
    int offset = 16, len = data_section->data_offset,
        sp = (offset + len + (1 << 16)) & ALIGNMENT_MASK;
    printf("(global $DATA (export \"__memory_base\") i32 (i32.const %d))\n", offset);
    printf("(global (export \"__data_end\") i32 (i32.const %d))\n", offset + len);
    printf("(global (export \"__heap_base\") i32 (i32.const %d))\n", sp);
    printf("(global $SP (export \"__stack_pointer\") (mut i32) (i32.const %d))\n", sp);
    printf("(data (i32.const %d) \"", offset);
    for (int i = 0; i < len; i++)
        printf("\\%02x", data_section->data[i]);
    printf("\")\n");

    printf("(global $FUNC (export \"__table_base\") i32 (i32.const 0))\n");
    printf("(table (export \"__indirect_function_table\") %lu anyfunc)\n",
        text_section->data_offset);
    printf(")\n");
    exit(0);
}

/*************************************************************/
#endif
/*************************************************************/

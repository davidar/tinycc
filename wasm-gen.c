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
#define NB_REGS 13

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
#define RC_INLINE  0x0100

ST_DATA const int reg_classes[NB_REGS] = {
    RC_INLINE,
    RC_INT,
    RC_INT,
    RC_INT | RC_IRET,
    RC_LLONG,
    RC_LLONG,
    RC_LLONG | RC_LRET,
    RC_FLOAT,
    RC_FLOAT,
    RC_FLOAT | RC_FRET,
    RC_DOUBLE,
    RC_DOUBLE,
    RC_DOUBLE | RC_DRET,
};

/* return registers for function */
#define REG_IRET  3
#define REG_LRET  6
#define REG_FRET  9
#define REG_DRET 12

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
    0,
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

#define ALIGNMENT_MASK (-MAX_ALIGN)

#define log(...) { \
    int _log_i; \
    for (_log_i = 0; _log_i < vtop - pvtop; _log_i++) fprintf(stderr, " "); \
    fprintf(stderr, "\033[0;31m"); \
    fprintf(stderr, __VA_ARGS__); \
    fprintf(stderr, "\033[0m"); \
    fprintf(stderr, "\n"); \
}

int aCMP, bCMP, tCMP;
int nblocks = 0, nlabels = 0, nfuncs = 0;

/******************************************************/

int native_type(int t) {
    switch(t & VT_BTYPE) {
        case VT_PTR:
        case VT_FUNC:
        case VT_BOOL:
        case VT_BYTE:
        case VT_SHORT:
        case VT_INT:    return VT_INT;
        case VT_LLONG:  return VT_LLONG;
        case VT_FLOAT:  return VT_FLOAT;
        case VT_LDOUBLE:
        case VT_DOUBLE: return VT_DOUBLE;
        case VT_VOID:   return VT_VOID;
        default: tcc_error("unknown type %#x", t);
    }
}

const char *lookup_type(int t) {
    switch(native_type(t)) {
        case VT_INT:    return "i32";
        case VT_LLONG:  return "i64";
        case VT_FLOAT:  return "f32";
        case VT_DOUBLE: return "f64";
    }
    return 0;
}

int lookup_return(int t) {
    if ((t & VT_BTYPE) == VT_STRUCT) t = VT_VOID;
    switch (native_type(t)) {
        case VT_INT:    return REG_IRET;
        case VT_LLONG:  return REG_LRET;
        case VT_FLOAT:  return REG_FRET;
        case VT_DOUBLE: return REG_DRET;
        case VT_VOID:   return -1;
    }
    return 0;
}

const char *lookup_op(int op, int t) {
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
        case TOK_LT: return is_float(t) ? "lt" : "lt_s";
        case TOK_LE: return is_float(t) ? "le" : "le_s";
        case TOK_GT: return is_float(t) ? "gt" : "gt_s";
        case TOK_GE: return is_float(t) ? "ge" : "ge_s";
        case TOK_ULT: return "lt_u";
        case TOK_ULE: return "le_u";
        case TOK_UGT: return "gt_u";
        case TOK_UGE: return "ge_u";
        default: tcc_error("unknown op %#x", op);
    }
}

void wasm_init(void) {
    FILE *f = fopen("preamble.wat", "r");
    printf("(module\n");
    for (int c = getc(f); c != EOF; c = getc(f)) putchar(c);
    fclose(f);
    printf("(func $__wasm_call_ctors)\n");
    printf("(memory (export \"memory\") 2)\n");
    section_reserve(data_section, MAX_ALIGN);
}

void gfunc_sig(Sym *return_sym, int decl) {
    int addr = 0, t = return_sym->type.t;
    if (decl) func_vt = return_sym->type;

    if ((t & VT_BTYPE) == VT_STRUCT) {
        printf("(param ");
        if (decl) printf("$p%d ", func_vc = addr++);
        printf("%s) ", lookup_type(VT_PTR));
    }

    for (Sym *cur = return_sym->next; cur; cur = cur->next) {
        int s = cur->type.t;
        printf("(param ");
        if (decl) {
            sym_push(cur->v & ~SYM_FIELD, &cur->type, VT_LOCAL | lvalue_type(s), addr);
            printf("$p%d ", addr++);
        }
        printf("%s) ", lookup_type(s));
    }

    if (return_sym->f.func_type == FUNC_ELLIPSIS) {
        printf("(param ");
        if (decl) printf("$p%d ", func_var = addr++);
        printf("%s) ", lookup_type(VT_PTR));
    }

    if (t != VT_VOID && (t & VT_BTYPE) != VT_STRUCT) {
        printf("(result %s) ", lookup_type(t));
    }
}

void gglobal(Sym *sym) {
    const char *name = get_tok_str(sym->v, NULL);
    log("gglobal %s = %d", name, sym->st_value);
}

void gdata(Sym *sym, int offset) {
    int p = sym->st_value;
    if (p < 0) p = ~p;
    log("gdata [%d] = %d", offset, p);
    write32le(data_section->data + offset, p);
}

void gfunc_prolog(CType *func_type) {
    loc = nblocks = nlabels = 0;
    log("gfunc_prolog %s func_type=%#x", funcname, func_type->t);
    if (func_type->t & VT_STATIC) {
        BufferedFile *f;
        for (f = file; f->prev; f = f->prev);
        printf("(func $%s/%s ", f->filename, funcname);
    } else {
        printf("(func $%s (export \"%s\") ", funcname, funcname);
    }
    gfunc_sig(func_type->ref, 1);
    printf("\n");
    for (int i = 1; i < NB_REGS; i++)
        printf("(local $r%d %s) ", i, lookup_type(reg_types[i]));
    printf("(local $goto i32) (local $exe_label i32)\n");
    printf("(set_local $exe_label (i32.const 0))\n");
}

void gsv(SValue *sv) {
    int v = sv->r & VT_VALMASK, fc = sv->c.i, ft = sv->type.t;
    if (v == VT_CONST) {
        if (sv->r & VT_SYM) {
            const char *name = get_tok_str(sv->sym->v, NULL);
            int p = sv->sym->st_value;
            if (p < 0) p = ~p;
            printf("(i32.const %d) (; %s ;)", p, name);
        } else {
            printf("(%s.const %d)", lookup_type(ft), fc);
        }
    } else if (v == VT_LOCAL || v == VT_LLOCAL) {
        if (v == VT_LLOCAL) printf("(i32.load ");
        if (fc >= 0) {
            tcc_error("can't load address of function parameter");
        } else {
            printf("(i32.add (get_global $SP) (i32.const %d))", fc);
        }
        if (v == VT_LLOCAL) printf(")");
    } else {
        printf("(get_local $r%d)", v);
    }
}

void load(int r, SValue *sv) {
    int v = sv->r & VT_VALMASK, fc = sv->c.i, ft = sv->type.t;
    log("load %d r=%#x v=%#x fc=%d ft=%#x", r, sv->r, v, fc, ft);

    if (v == VT_JMP || v == VT_JMPI) {
        int k = fc & 0xff;
        if (k != BLOCK_VT_JMP && k != BLOCK_VT_JMPI)
            tcc_error("block (%d) doesn't support VT_JMP%s", k, (v == VT_JMP) ? "" : "I");
        gsym(fc);
        if (k - BLOCK_VT_JMP != v - VT_JMP)
            printf("i32.eqz\n");
        if (r > 0) printf("set_local $r%d\n", r);
        return;
    }

    if (r > 0) printf("(set_local $r%d ", r);
    if (v == VT_CMP) {
        printf("(%s.%s (get_local $r%d) (get_local $r%d))",
            lookup_type(tCMP), lookup_op(fc, tCMP), aCMP, bCMP);
    } else if ((sv->r & VT_LVAL) && (v == VT_LOCAL) && fc >= 0) {
        printf("(get_local $p%d)", fc);
    } else if (sv->r & VT_LVAL) {
        char s = (ft & VT_UNSIGNED) ? 'u' : 's';
        printf("(");
        switch (ft & VT_BTYPE) {
            case VT_BYTE: printf("i32.load8_%c", s); break;
            case VT_SHORT: printf("i32.load16_%c", s); break;
            default: printf("%s.load", lookup_type(ft));
        }
        printf(" ");
        gsv(sv);
        printf(")");
    } else {
        gsv(sv);
    }
    if (r > 0) printf(")\n");
}

void store(int r, SValue *sv) {
    int v = sv->r & VT_VALMASK, fc = sv->c.i, ft = sv->type.t;
    log("store %d r=%#x v=%#x fc=%d ft=%#x", r, sv->r, v, fc, ft);
    printf("(");
    if ((sv->r & VT_LVAL) && (v == VT_LOCAL) && fc >= 0) {
        printf("set_local $p%d", fc);
    } else {
        switch (ft & VT_BTYPE) {
            case VT_BYTE: printf("i32.store8"); break;
            case VT_SHORT: printf("i32.store16"); break;
            default: printf("%s.store", lookup_type(ft));
        }
        printf(" ");
        gsv(sv);
    }
    printf(" (get_local $r%d))\n", r);
}

void gfunc_call(int nb_args) {
    Sym *return_sym = vtop[-nb_args].type.ref;
    int t = return_sym->type.t;
    int indirect = ((vtop[-nb_args].r & (VT_VALMASK | VT_LVAL)) != VT_CONST);
    int r = -1;
    int nb_fixed = 0;
    if (return_sym->f.func_type == FUNC_ELLIPSIS) {
        for (Sym *cur = return_sym->next; cur; cur = cur->next) nb_fixed++;
    } else {
        nb_fixed = nb_args;
    }
    log("gfunc_call nb_args=%d nb_fixed=%d", nb_args, nb_fixed);

    for (int i = 0; i < nb_fixed; i++) {
        vrotb(nb_args - i);
        if ((vtop->type.t & VT_BTYPE) == VT_STRUCT) {
            tcc_error("structures passed as value not handled yet");
        } else {
            gv(RC_INLINE);
            printf(" ;; push arg\n");
        }
        vtop--;
    }

    for (int i = nb_fixed; i < nb_args; i++) {
        int size, align;
        SValue sv;
        convert_parameter_type(&vtop->type);
        size = type_size(&vtop->type, &align);
        if (size < 4) size = align = 4;
        loc = (loc - size) & -align;
        sv.type.t = vtop->type.t;
        sv.r = VT_LOCAL | VT_LVAL;
        sv.c.i = loc;

        printf("(%s.store ", lookup_type(sv.type.t));
        gsv(&sv);
        printf(" ");
        gv(RC_INLINE);
        printf(")\n");
        vtop--;

        if (i == nb_args - 1) {
            gsv(&sv);
            printf(" ;; varargs\n");
        }
    }

    if (indirect) r = gv(RC_INT);
    printf("(set_global $SP (i32.add (get_global $SP) (i32.const %d)))\n",
        loc & ALIGNMENT_MASK);
    if (lookup_return(t) > 0) printf("(set_local $r%d ", lookup_return(t));
    if (indirect) {
        if (return_sym->f.func_type != FUNC_NEW)
            tcc_error("unsupported function prototype for indirect call");
        printf("(call_indirect ");
        gfunc_sig(return_sym, 0);
        printf("(get_local $r%d))", r);
    } else {
        const char* name = get_tok_str(vtop->sym->v, NULL);
        if (vtop->type.t & VT_STATIC) {
            BufferedFile *f;
            for (f = file; f->prev; f = f->prev);
            printf("(call $%s/%s)", f->filename, name);
            if (!vtop->sym->st_value) vtop->sym->st_value = ~(nfuncs++);
        } else {
            printf("(call $%s)", name);
        }
    }
    if (lookup_return(t) > 0) printf(")");
    printf("\n");
    printf("(set_global $SP (i32.sub (get_global $SP) (i32.const %d)))\n",
        loc & ALIGNMENT_MASK);
    vtop--;
}

ST_FUNC int gfunc_sret(CType *vt, int variadic, CType *ret, int *ret_align, int *regsize) {
    *ret_align = 1;
    return 0;
}

ST_FUNC int gblock(int t) {
    int k = t & 0xff, i = t >> 8;
    if (!i) i = ++nblocks;
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
    } else if (k == BLOCK_GOTO) {
        printf("(tee_local $exe_label (i32.or (get_local $exe_label) "
            "(i32.eq (get_local $goto) (i32.const %d))))\n", i);
        printf("if $G%d\n", i);
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

ST_FUNC int glabel(void) {
    return (++nlabels << 8) | BLOCK_GOTO;
}

void gsym(int t) {
    int k = t & 0xff, i = t >> 8;
    if (i == 0) {
        log("gsym(0)");
    } else if (k == BLOCK_IF_ELSE) {
        printf("else $B%d\n", i);
    } else if (k == BLOCK_SWITCH) {
        printf("drop\n");
        printf("end $S%d\n", i);
    } else if (k == BLOCK_SWITCH_CASE) {
        printf("end $C%d\n", i);
    } else if (k == BLOCK_GOTO) {
        printf("end $G%d\n", i);
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
        int r = lookup_return(func_vt.t);
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
    } else if (k == BLOCK_GOTO) {
        printf("(set_local $goto (i32.const %d))\n", i);
        gjmp(rsym);
    } else {
        printf("br $B%d\n", i);
    }
    return t;
}

int gtst(int inv, int t) {
    int k = t & 0xff, i = t >> 8, v = vtop->r & VT_VALMASK;
    log("gtst inv=%d k=%d i=%d", inv, k, i);
    if (nocode_wanted) {
        log("no code wanted");
    } else if (v == VT_CMP || v == VT_JMP || v == VT_JMPI) {
        vtop->c.i ^= inv;
        if (k == BLOCK_IF) {
            gv(RC_INLINE);
            printf("\n");
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
            r, lookup_type(t), lookup_op(op, t), r, fr);
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
    int s = vtop->type.t, r = lookup_return(t);
    char us = (s & VT_UNSIGNED) ? 'u' : 's';
    char ut = (t & VT_UNSIGNED) ? 'u' : 's';
    s = native_type(s);
    t = native_type(t);
    save_reg(r);
    printf("(set_local $r%d ", r);
    if (s != t) {
        printf("(");
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
            tcc_error("can't cast %s (%#x) to %s (%#x)", lookup_type(s), s, lookup_type(t), t);
        }
        printf(" ");
    }
    gv(RC_INLINE);
    if (s != t) printf(")");
    printf(")\n");
    vtop->r = r;
    vtop->type.t = t;
}

void gen_cvt_itoi(int t) { gen_cvt(t); }
void gen_cvt_itof(int t) { gen_cvt(t); }
void gen_cvt_ftoi(int t) { gen_cvt(t); }
void gen_cvt_ftof(int t) { gen_cvt(t); }

ST_FUNC void gen_vla_sp_save(int addr) {
    tcc_error("variable length arrays unsupported for this target");
}

ST_FUNC void gen_vla_sp_restore(int addr) {
    tcc_error("variable length arrays unsupported for this target");
}

ST_FUNC void gen_vla_alloc(CType *type, int align) {
    tcc_error("variable length arrays unsupported for this target");
}

void gfunc_epilog(void) {
    int r = lookup_return(func_vt.t);
    if (r >= 0) printf("get_local $r%d\n", r);
    printf(")\n\n");
}

void wasm_end(void) {
    int len = data_section->data_offset, sp = 1 << 16;
    printf("(global (export \"__data_end\") i32 (i32.const %d))\n", len);
    printf("(global (export \"__heap_base\") i32 (i32.const %d))\n", sp);
    printf("(global $SP (export \"__stack_pointer\") (mut i32) (i32.const %d))\n", sp);
    printf("(data (i32.const 0) \"");
    for (int i = 0; i < len; i++)
        printf("\\%02x", data_section->data[i]);
    printf("\")\n");
    printf("(table (export \"__indirect_function_table\") %d anyfunc)\n", nfuncs);
    printf(")\n");
    exit(0);
}

/*************************************************************/
#endif
/*************************************************************/

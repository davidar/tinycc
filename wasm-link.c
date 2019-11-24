#ifdef TARGET_DEFS_ONLY

#define EM_TCC_TARGET EM_C60

/* relocation type for 32 bit data relocation */
#define R_DATA_32   R_C60_32
#define R_DATA_PTR  R_C60_32
#define R_JMP_SLOT  R_C60_JMP_SLOT
#define R_GLOB_DAT  R_C60_GLOB_DAT
#define R_COPY      R_C60_COPY
#define R_RELATIVE  R_C60_RELATIVE

#define R_NUM       R_C60_NUM

#define ELF_START_ADDR 0x00000400
#define ELF_PAGE_SIZE  0x1000

#define PCRELATIVE_DLLPLT 0
#define RELOCATE_DLLPLT 0

#else /* !TARGET_DEFS_ONLY */

#include "tcc.h"

int code_reloc (int reloc_type) {
    tcc_error("code_reloc");
}

int gotplt_entry_type (int reloc_type) {
    tcc_error("gotplt_entry_type");
}

ST_FUNC unsigned create_plt_entry(TCCState *s1, unsigned got_offset, struct sym_attr *attr) {
    tcc_error("create_plt_entry");
}

ST_FUNC void relocate_plt(TCCState *s1) {
    tcc_error("relocate_plt");
}

void relocate_init(Section *sr) {}
void relocate_fini(Section *sr) {}

void relocate(TCCState *s1, ElfW_Rel *rel, int type, unsigned char *ptr, addr_t addr, addr_t val) {
    tcc_error("relocate");
}

#endif /* !TARGET_DEFS_ONLY */

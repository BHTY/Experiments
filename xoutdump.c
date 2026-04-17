// XENIX emulator for Linux
// wants to run on Linux because XENIX apps need UNIX system calls like fork

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define PTR_SUB(a,b)    ((size_t)(a) - (size_t)(b))

struct xexec {
    /*  x.out header */
    uint16_t x_magic;   /*  Magic number    */
    uint16_t x_ext;     /*  Size of header extension    */
    uint32_t x_text;    /*  Size of text segment    */
    uint32_t x_data;    /*  Size of initialized data    */
    uint32_t x_bss;     /*  Size of uninitialized data  */
    uint32_t x_syms;    /*  Size of symbol table    */
    uint32_t x_reloc;   /*  Relocation table length */
    uint32_t x_entry;   /*  Entry offset, relative to xe_eseg   */
    uint8_t  x_cpu;     /*  CPU type & byte/word order  */
    uint8_t  x_relsym;  /*  Relocation & symbol format  */
    uint16_t x_renv;    /*  Run-time environment    */
};

struct xext {
    /*  x.out header extension  */
    uint32_t xe_trsize;     /*  Size of text relocation */
    uint32_t xe_drsize;     /*  Size of data relocation */
    uint32_t xe_brsize;     /*  ??? */
    uint32_t xe_dbase;      /*  Data relocation base    */
    uint32_t xe_stksize;    /*  Stack size  */
    uint32_t xe_segpos;     /*  Segment table position  */
    uint32_t xe_segsize;    /*  Segment table size  */
    uint32_t xe_mdtpos;     /*  Machine-dependent table position    */
    uint32_t xe_mdtsize;    /*  Machine-dependent table size    */
    uint8_t  xe_mdttype;    /*  Machine-dependent table type    */
    uint8_t  xe_pagesize;   /*  File pagesize, in multiples of 512  */
    uint8_t  xe_ostype;     /*  Operating system type   */
    uint8_t  xe_osvers;     /*  Operating system version    */
    uint16_t xe_eseg;       /*  Entry segment   */
    uint16_t xe_sres;       /*  Reserved    */
};

/*
 *  Definitions for xs_type:
 *      Values from 64 to 127 are reserved
 */
#define XS_TNULL    0   /*  unused segment  */
#define XS_TTEXT    1   /*  text segment    */
#define XS_TDATA    2   /*  data segment    */
#define XS_TSYMS    3   /*  symbol table segment    */
#define XS_TREL4    4   /*  relocation segment  */

struct xseg {
    /*  x.out segment table entry   */
    uint16_t xs_type;   /*  Segment type    */
    uint16_t xs_attr;   /*  Segment attributes  */
    uint16_t xs_seg;    /*  Segment number  */
    uint16_t xs_sres;   /*  Unused  */
    uint32_t xs_filpos; /*  File position   */
    uint32_t xs_psize;  /*  Physical size (in file) */
    uint32_t xs_vsize;  /*  Virtual size (in core)  */
    uint32_t xs_rbase;  /*  Relocation base address */
    uint32_t xs_lres;   /*  Unused  */
    uint32_t xs_lres2;  /*  Unused  */
};

struct sym {
    /*  x.out symbol table entry    */
    uint16_t s_type;
    uint16_t s_seg; 
    uint32_t s_value;
};

void dump_xexec (struct xexec* exec)
{
    printf("[xexec]\n");
    printf("x_magic       %04XH\n", exec->x_magic);
    printf("x_ext         %04XH\n", exec->x_ext);
    printf("x_text    %08XH\n", exec->x_text);
    printf("x_data    %08XH\n", exec->x_data);
    printf("x_bss     %08XH\n", exec->x_bss);
    printf("x_syms    %08XH\n", exec->x_syms);
    printf("x_reloc   %08XH\n", exec->x_reloc);
    printf("x_entry   %08XH\n", exec->x_entry);
    printf("x_cpu           %02XH\n", exec->x_cpu);
    printf("x_relsym        %02XH\n", exec->x_relsym);
    printf("x_renv        %04XH\n", exec->x_renv);
}

void dump_xext (struct xext* ext)
{
    printf("[xext]\n");
    printf("xe_trsize    %08XH\n", ext->xe_trsize);
    printf("xe_drsize    %08XH\n", ext->xe_drsize);
    printf("xe_brsize    %08XH\n", ext->xe_brsize);
    printf("xe_dbase     %08XH\n", ext->xe_dbase);
    printf("xe_stksize   %08XH\n", ext->xe_stksize);
    printf("xe_segpos    %08XH\n", ext->xe_segpos);
    printf("xe_segsize   %08XH\n", ext->xe_segsize);
    printf("xe_mdtpos    %08XH\n", ext->xe_mdtpos);
    printf("xe_mdtsize   %08XH\n", ext->xe_mdtsize);
    printf("xe_mdttype         %02XH\n", ext->xe_mdttype);
    printf("xe_pagesize        %02XH\n", ext->xe_pagesize);
    printf("xe_ostype          %02XH\n", ext->xe_ostype);
    printf("xe_osvers          %02XH\n", ext->xe_osvers);
    printf("xe_eseg          %04XH\n", ext->xe_eseg);
    printf("xe_sres          %04XH\n", ext->xe_sres);
}

void dump_xseg (struct xseg* seg)
{
    printf("[xseg]\n");
    printf("xs_type        %04XH\n", seg->xs_type);
    printf("xs_attr        %04XH\n", seg->xs_attr);
    printf("xs_seg         %04XH\n", seg->xs_seg);
    printf("xs_sres        %04XH\n", seg->xs_sres);
    printf("xs_filpos  %08XH\n", seg->xs_filpos);
    printf("xs_psize   %08XH\n", seg->xs_psize);
    printf("xs_vsize   %08XH\n", seg->xs_vsize);
    printf("xs_rbase   %08XH\n", seg->xs_rbase);
    printf("xs_lres    %08XH\n", seg->xs_lres);
    printf("xs_lres2   %08XH\n", seg->xs_lres2);
}

void dump_segtab (uint8_t* buffer, struct xext* ext)
{
    struct xseg* seg = buffer + ext->xe_segpos;
    struct xseg* firstseg = seg;
    
    while (PTR_SUB(seg, firstseg) < ext->xe_segsize)
    {
        dump_xseg(seg);
        seg++;
    }
}

void dump_xout (uint8_t* buffer)
{
    struct xexec* exec = buffer;
    struct xext* ext = exec + 1;

    if (exec->x_magic != 0x0206) {
        printf("Not valid x.out!\n");
        return;
    }

    dump_xexec(exec);
    dump_xext(ext);
    dump_segtab(buffer, ext);
}


int main (int argc, char** argv)
{
    char* filename = argv[1];

    uint32_t size;
    uint8_t* buffer;
    FILE* fp = fopen(filename, "rb");
    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    buffer = malloc(size);
    fread(buffer, 1, size, fp);
    fclose(fp);

    dump_xout(buffer);
}

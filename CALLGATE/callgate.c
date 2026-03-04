/*      Works in Win32 or DOS Flat Mode         */

#include <stdio.h>

//#include <windows.h>

typedef unsigned long DWORD;
typedef unsigned short WORD;
typedef unsigned char  BYTE;

DWORD dwGDTBase = 0;
WORD  wGDTLimit = 0;
DWORD dwLDTBase = 0;
DWORD wLDTLimit = 0;
DWORD dwIDTBase = 0;
WORD  wIDTLimit = 0;

typedef struct _SEGMENT_DESCRIPTOR {
    WORD	limit_0_15;
    WORD	base_0_15;
    BYTE	base_16_23;
    BYTE	access;
    BYTE	limit_16_19_attrs;
    BYTE	base_24_31;
} SEGMENT_DESCRIPTOR;

#define DPL(a)		(((a) >> 5) & 3)
#define PRESENT(a)	((a) & 0x80)
#define NONSYSTEM(a)	((a) & 0x10)
#define GRAN(a)		((a) & 0x80)
#define BIG(a)		((a) & 0x40)
#define CODE(a)		((a) & 8)
#define READWRITE(a)	((a) & 2)
#define CONF_ED(a)      ((a) & 4)
#define SYSTYPE(a)	((a) & 0xf)
#define SEGTYPE_TSS_AVAIL	0x1
#define SEGTYPE_LDT		0x2
#define SEGTYPE_TSS_BUSY	0x3
#define SEGTYPE_CALL_GATE	0x4
#define SEGTYPE_TASK_GATE	0x5
#define SEGTYPE_INTR_GATE	0x6
#define SEGTYPE_TRAP_GATE	0x7

char *SegTypes[] = {"???     ",
    		    "TSS Avl ",
		    "LDT     ",
		    "TSS Busy",
		    "CallGate",
		    "TaskGate",
		    "IntrGate",
		    "TrapGate"
};

void Write_Call_Gate (SEGMENT_DESCRIPTOR* pDesc, WORD wSel, DWORD dwOffset, DWORD dwCount, BYTE dpl)
{
    pDesc->limit_0_15 = dwOffset;
    pDesc->limit_16_19_attrs = dwOffset >> 16;
    pDesc->base_24_31 = dwOffset >> 24;
    pDesc->base_0_15 = wSel;
    pDesc->base_16_23 = dwCount;
    pDesc->access = 0x80 | (dpl << 5) | 12;

}

void Print_Segment_Descriptor (WORD wSel, SEGMENT_DESCRIPTOR* pDesc)
{
    DWORD dwBaseAddr = pDesc->base_0_15 | (pDesc->base_16_23 << 16) | (pDesc->base_24_31 << 24);
    DWORD dwLimit = pDesc->limit_0_15 | ((pDesc->limit_16_19_attrs & 0xF) << 16);
    printf("%04X: ", wSel);

    if (!PRESENT(pDesc->access)) {
	printf("NOT PRESENT\n");
	return;
    }

    printf("DPL%d ", DPL(pDesc->access));

    if (NONSYSTEM(pDesc->access)) {
	printf("%08X %s%d ", dwBaseAddr, CODE(pDesc->access) ? "Code" : "Data", BIG(pDesc->limit_16_19_attrs) ? 32 : 16);

	printf("%c ", READWRITE(pDesc->access) ? (CODE(pDesc->access) ? 'R' : 'W') : ' ');	

        printf(" Lim %05X %s  ", dwLimit, GRAN(pDesc->limit_16_19_attrs) ? "Page" : "Byte");

        printf("%s\n", CONF_ED(pDesc->access) ? (CODE(pDesc->access) ? "Conforming" : "ExpandDown") : "");

    } else {
	DWORD dwOffset = pDesc->limit_0_15 | (pDesc->limit_16_19_attrs << 16) | (pDesc->base_24_31 << 24);
	printf("%s ", SegTypes[SYSTYPE(pDesc->access) & 7]);
	switch (SYSTYPE(pDesc->access) & 7) {
	    case SEGTYPE_LDT:
	    	dwLDTBase = dwBaseAddr;
		wLDTLimit = dwLimit;
	    case SEGTYPE_TSS_AVAIL:
	    case SEGTYPE_TSS_BUSY:
                printf("%08X  Lim %05X %s\n", dwBaseAddr, dwLimit,
                    GRAN(pDesc->limit_16_19_attrs) ? "Page" : "Byte"
                );
	    	break;
	    case SEGTYPE_TASK_GATE:
	    	printf("%04X\n", pDesc->base_0_15);
		break;
	    case SEGTYPE_INTR_GATE:
	    case SEGTYPE_TRAP_GATE:
	    	printf("%04X:%08X\n", pDesc->base_0_15, dwOffset);
		break;	
	    case SEGTYPE_CALL_GATE:
	    	printf("%04X:%08X Copy %02X %sWORDs\n",
			pDesc->base_0_15,
			dwOffset,
			pDesc->base_16_23 & 0x1F,
			(SYSTYPE(pDesc->access) & 8) ? "D" : ""
		);
	    	break;
	}
    }
}

char hello_ring0[] = {0xb8, 0x78, 0x56, 0x34, 0x12, 0xcb};

int main() {
    WORD gdtr[3];
    WORD idtr[3];
    WORD ldtr;
    WORD mycs, myds;
    int i;

    __asm {
	sgdt gdtr
	sldt ldtr
	sidt idtr
	mov ax, cs
	mov mycs, ax
	mov ax, ds
	mov myds, ax
    };

    printf("CS: %04X  DS: %04X\n", mycs, myds);
    printf("GDT base: %04X%04X Limit: %04X\n", gdtr[2], gdtr[1], gdtr[0]);
    printf("IDT base: %04X%04X Limit: %04X\n", idtr[2], idtr[1], idtr[0]);
    printf("LDT: %04X\n", ldtr);

    dwGDTBase = (gdtr[2] << 16) | gdtr[1];
    wGDTLimit = gdtr[0];

    dwIDTBase = (idtr[2] << 16) | idtr[1];
    wIDTLimit = idtr[0];

    //*(char*)(dwLDTBase) = 0;
    //printf("Written to LDT\n");
    //*(char*)(dwGDTBase) = 0;
    //printf("Written to GDT\n");
    //memset(dwGDTBase, 0, wGDTLimit);

    printf("\nGlobal Descriptor Table\n");
    for (i = 0; i < wGDTLimit; i += sizeof(SEGMENT_DESCRIPTOR)) {
	Print_Segment_Descriptor(i, dwGDTBase + i);
    }

    if (dwLDTBase) {
	printf("\n\nLocal Descriptor Table\n");
	for (i = 0; i < wLDTLimit; i += sizeof(SEGMENT_DESCRIPTOR)) {
            Print_Segment_Descriptor(i | 4, dwLDTBase + i);
	}
    }

    printf("\n\nInterrupt Descriptor Table\n");
    for (i = 0; i < wIDTLimit; i += sizeof(SEGMENT_DESCRIPTOR)) {
        Print_Segment_Descriptor(i / sizeof(SEGMENT_DESCRIPTOR), dwIDTBase + i);
    }

    {
	WORD call_gate[3] = {0};
	DWORD res = 0;
	SEGMENT_DESCRIPTOR desc;
	printf("\n");
	Write_Call_Gate (&desc, 0x28, &hello_ring0, 0, 3);
	//Print_Segment_Descriptor(0x5678, &desc);

	// Write to LDT+4F0
	*(SEGMENT_DESCRIPTOR*)(dwLDTBase + 0x4F0) = desc;
	call_gate[2] = 0x4F7;

	Print_Segment_Descriptor(0x4F4, dwLDTBase + 0x4F0);

        __asm {
                call fword ptr call_gate
                mov res, eax
        };

	printf("Result %08X\n", res);
    }
}
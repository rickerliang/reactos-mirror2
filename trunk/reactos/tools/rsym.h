/* rsym.h */

#ifndef RSYM_H
#define RSYM_H

#define IMAGE_DOS_MAGIC 0x5a4d
#define IMAGE_PE_MAGIC 0x00004550

#define IMAGE_SIZEOF_SHORT_NAME 8

#define IMAGE_NUMBEROF_DIRECTORY_ENTRIES 16

typedef unsigned char BYTE;
typedef unsigned char UCHAR;
typedef unsigned short WORD;
typedef unsigned short USHORT;
#if defined(__x86_64__) && defined(linux)
typedef signed int LONG;
typedef unsigned int ULONG;
typedef unsigned int DWORD;
#else
typedef signed long LONG;
typedef unsigned long ULONG;
typedef unsigned long DWORD;
#endif
#if defined(_WIN64)
typedef unsigned __int64 ULONG_PTR;
#else
#if defined(__x86_64__) && defined(linux)
typedef  unsigned int  ULONG_PTR;
#else
typedef  unsigned long ULONG_PTR;
#endif
#endif

#pragma pack(2)
typedef struct _IMAGE_DOS_HEADER {
  WORD e_magic;
  WORD e_cblp;
  WORD e_cp;
  WORD e_crlc;
  WORD e_cparhdr;
  WORD e_minalloc;
  WORD e_maxalloc;
  WORD e_ss;
  WORD e_sp;
  WORD e_csum;
  WORD e_ip;
  WORD e_cs;
  WORD e_lfarlc;
  WORD e_ovno;
  WORD e_res[4];
  WORD e_oemid;
  WORD e_oeminfo;
  WORD e_res2[10];
  LONG e_lfanew;
} IMAGE_DOS_HEADER,*PIMAGE_DOS_HEADER;
#pragma pack(4)

#define IMAGE_FILE_LINE_NUMS_STRIPPED	4
#define IMAGE_FILE_LOCAL_SYMS_STRIPPED	8
#define IMAGE_FILE_DEBUG_STRIPPED	512

#pragma pack(4)
typedef struct _IMAGE_FILE_HEADER {
  WORD Machine;
  WORD NumberOfSections;
  DWORD TimeDateStamp;
  DWORD PointerToSymbolTable;
  DWORD NumberOfSymbols;
  WORD SizeOfOptionalHeader;
  WORD Characteristics;
} IMAGE_FILE_HEADER, *PIMAGE_FILE_HEADER;

typedef struct _IMAGE_DATA_DIRECTORY {
  DWORD VirtualAddress;
  DWORD Size;
} IMAGE_DATA_DIRECTORY,*PIMAGE_DATA_DIRECTORY;

#define IMAGE_DIRECTORY_ENTRY_BASERELOC	5

typedef struct _IMAGE_OPTIONAL_HEADER {
  WORD Magic;
  BYTE MajorLinkerVersion;
  BYTE MinorLinkerVersion;
  DWORD SizeOfCode;
  DWORD SizeOfInitializedData;
  DWORD SizeOfUninitializedData;
  DWORD AddressOfEntryPoint;
  DWORD BaseOfCode;
  DWORD BaseOfData;
  DWORD ImageBase;
  DWORD SectionAlignment;
  DWORD FileAlignment;
  WORD MajorOperatingSystemVersion;
  WORD MinorOperatingSystemVersion;
  WORD MajorImageVersion;
  WORD MinorImageVersion;
  WORD MajorSubsystemVersion;
  WORD MinorSubsystemVersion;
  DWORD Reserved1;
  DWORD SizeOfImage;
  DWORD SizeOfHeaders;
  DWORD CheckSum;
  WORD Subsystem;
  WORD DllCharacteristics;
  DWORD SizeOfStackReserve;
  DWORD SizeOfStackCommit;
  DWORD SizeOfHeapReserve;
  DWORD SizeOfHeapCommit;
  DWORD LoaderFlags;
  DWORD NumberOfRvaAndSizes;
  IMAGE_DATA_DIRECTORY DataDirectory[IMAGE_NUMBEROF_DIRECTORY_ENTRIES];
} IMAGE_OPTIONAL_HEADER,*PIMAGE_OPTIONAL_HEADER;

#define IMAGE_SCN_TYPE_NOLOAD     0x00000002
#define IMAGE_SCN_LNK_REMOVE      0x00000800
#define IMAGE_SCN_MEM_READ        0x40000000
#define IMAGE_SCN_MEM_DISCARDABLE 0x02000000

typedef struct _IMAGE_SECTION_HEADER {
  BYTE Name[IMAGE_SIZEOF_SHORT_NAME];
  union {
    DWORD PhysicalAddress;
    DWORD VirtualSize;
  } Misc;
  DWORD VirtualAddress;
  DWORD SizeOfRawData;
  DWORD PointerToRawData;
  DWORD PointerToRelocations;
  DWORD PointerToLinenumbers;
  WORD NumberOfRelocations;
  WORD NumberOfLinenumbers;
  DWORD Characteristics;
} IMAGE_SECTION_HEADER,*PIMAGE_SECTION_HEADER;

typedef struct _IMAGE_BASE_RELOCATION {
	DWORD VirtualAddress;
	DWORD SizeOfBlock;
} IMAGE_BASE_RELOCATION,*PIMAGE_BASE_RELOCATION;


typedef struct {
  USHORT f_magic;         /* magic number             */
  USHORT f_nscns;         /* number of sections       */
  ULONG  f_timdat;        /* time & date stamp        */
  ULONG  f_symptr;        /* file pointer to symtab   */
  ULONG  f_nsyms;         /* number of symtab entries */
  USHORT f_opthdr;        /* sizeof(optional hdr)     */
  USHORT f_flags;         /* flags                    */
} FILHDR;

typedef struct {
  char           s_name[8];  /* section name                     */
  ULONG  s_paddr;    /* physical address, aliased s_nlib */
  ULONG  s_vaddr;    /* virtual address                  */
  ULONG  s_size;     /* section size                     */
  ULONG  s_scnptr;   /* file ptr to raw data for section */
  ULONG  s_relptr;   /* file ptr to relocation           */
  ULONG  s_lnnoptr;  /* file ptr to line numbers         */
  USHORT s_nreloc;   /* number of relocation entries     */
  USHORT s_nlnno;    /* number of line number entries    */
  ULONG  s_flags;    /* flags                            */
} SCNHDR;
#pragma pack(4)

typedef struct _SYMBOLFILE_HEADER {
  ULONG SymbolsOffset;
  ULONG SymbolsLength;
  ULONG StringsOffset;
  ULONG StringsLength;
} SYMBOLFILE_HEADER, *PSYMBOLFILE_HEADER;

typedef struct _STAB_ENTRY {
  ULONG n_strx;         /* index into string table of name */
  UCHAR n_type;         /* type of symbol */
  UCHAR n_other;        /* misc info (usually empty) */
  USHORT n_desc;        /* description field */
  ULONG n_value;        /* value of symbol */
} STAB_ENTRY, *PSTAB_ENTRY;

#define N_FUN 0x24
#define N_SLINE 0x44
#define N_SO 0x64

/* COFF symbol table */

#define E_SYMNMLEN	8	/* # characters in a symbol name	*/
#define E_FILNMLEN	14	/* # characters in a file name		*/
#define E_DIMNUM	4	/* # array dimensions in auxiliary entry */

#define N_BTMASK	(0xf)
#define N_TMASK		(0x30)
#define N_BTSHFT	(4)
#define N_TSHIFT	(2)

/* derived types, in e_type */
#define DT_NON		(0)	/* no derived type */
#define DT_PTR		(1)	/* pointer */
#define DT_FCN		(2)	/* function */
#define DT_ARY		(3)	/* array */

#define BTYPE(x)	((x) & N_BTMASK)

#define ISPTR(x)	(((x) & N_TMASK) == (DT_PTR << N_BTSHFT))
#define ISFCN(x)	(((x) & N_TMASK) == (DT_FCN << N_BTSHFT))
#define ISARY(x)	(((x) & N_TMASK) == (DT_ARY << N_BTSHFT))
#define ISTAG(x)	((x)==C_STRTAG||(x)==C_UNTAG||(x)==C_ENTAG)
#define DECREF(x) ((((x)>>N_TSHIFT)&~N_BTMASK)|((x)&N_BTMASK))

#define C_EFCN		0xff	/* physical end of function	*/
#define C_NULL		0
#define C_AUTO		1	/* automatic variable		*/
#define C_EXT		2	/* external symbol		*/
#define C_STAT		3	/* static			*/
#define C_REG		4	/* register variable		*/
#define C_EXTDEF	5	/* external definition		*/
#define C_LABEL		6	/* label			*/
#define C_ULABEL	7	/* undefined label		*/
#define C_MOS		8	/* member of structure		*/
#define C_ARG		9	/* function argument		*/
#define C_STRTAG	10	/* structure tag		*/
#define C_MOU		11	/* member of union		*/
#define C_UNTAG		12	/* union tag			*/
#define C_TPDEF		13	/* type definition		*/
#define C_USTATIC	14	/* undefined static		*/
#define C_ENTAG		15	/* enumeration tag		*/
#define C_MOE		16	/* member of enumeration	*/
#define C_REGPARM	17	/* register parameter		*/
#define C_FIELD		18	/* bit field			*/
#define C_AUTOARG	19	/* auto argument		*/
#define C_LASTENT	20	/* dummy entry (end of block)	*/
#define C_BLOCK		100	/* ".bb" or ".eb"		*/
#define C_FCN		101	/* ".bf" or ".ef"		*/
#define C_EOS		102	/* end of structure		*/
#define C_FILE		103	/* file name			*/
#define C_LINE		104	/* line # reformatted as symbol table entry */
#define C_ALIAS	 	105	/* duplicate tag		*/
#define C_HIDDEN	106	/* ext symbol in dmert public lib */

#pragma pack(1)
typedef struct _COFF_SYMENT
{
  union
    {
      char e_name[E_SYMNMLEN];
      struct
        {
          ULONG e_zeroes;
          ULONG e_offset;
        }
      e;
    }
  e;
  ULONG e_value;
  short e_scnum;
  USHORT e_type;
  UCHAR e_sclass;
  UCHAR e_numaux;
} COFF_SYMENT, *PCOFF_SYMENT;
#pragma pack(4)

typedef struct _ROSSYM_ENTRY {
  ULONG_PTR Address;
  ULONG FunctionOffset;
  ULONG FileOffset;
  ULONG SourceLine;
} ROSSYM_ENTRY, *PROSSYM_ENTRY;

#define ROUND_UP(N, S) (((N) + (S) - 1) & ~((S) - 1))

extern char*
convert_path(const char* origpath);

extern void*
load_file ( const char* file_name, size_t* file_size );

#endif/*RSYM_H*/

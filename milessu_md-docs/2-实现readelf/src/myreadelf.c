#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <elf.h>
#include <gelf.h>
#include <libelf.h>
#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>

#define VERSION "1.0"

/**
 * @brief 打印帮助信息
 * 输出命令行的使用方法和各选项的描述。
 * @return void
 */
 void print_help()
 {
    printf("Usage: readelf [options] FILES\n");
    printf("Display information about the contents of ELF format files.\n");
    printf("  -H, --help                show help information\n");
    printf("  -v, --version             show version\n");
    printf("  -h, --file-header         Display the ELF file header\n");
    printf("  -S, --section-headers     Display the sections' header\n");
    printf("  -s, --syms                Display the symbol table\n");
    printf("  -r, --relocs              Display the relocations (if present)\n");
    printf("  -l, --program-header      Display the program headers\n");
    printf("  -T, --silent-truncation   If a symbol name is truncated, do not add [...] suffix\n");
    printf("  -W                        Don't break output lines to fit into 80 columns\n");
}

/**
 * @brief 打印版本信息
 * 输出程序的版本信息。
 * @return void
 */
 void print_version()
{
    printf("readelf version %s\n", VERSION);
}

const char* get_osabi_name(unsigned char osabi) {
    switch (osabi) {
        case ELFOSABI_SYSV:       return "UNIX - System V";
        case ELFOSABI_HPUX:       return "HP-UX";
        case ELFOSABI_NETBSD:     return "NetBSD";
        case ELFOSABI_LINUX:      return "Linux";
        case ELFOSABI_SOLARIS:    return "Solaris";
        case ELFOSABI_AIX:        return "AIX";
        case ELFOSABI_IRIX:       return "IRIX";
        case ELFOSABI_FREEBSD:    return "FreeBSD";
        case ELFOSABI_TRU64:      return "TRU64";
        case ELFOSABI_ARM:        return "ARM";
        case ELFOSABI_STANDALONE: return "Standalone (embedded)";
        default:                  return "Unknown";
    }
}

const char* get_type_name(uint16_t type) {
    switch (type) {
        case ET_NONE: return "NONE (No file type)";
        case ET_REL:  return "REL (Relocatable file)";
        case ET_EXEC: return "EXEC (Executable file)";
        case ET_DYN:  return "DYN (Shared object file)";
        case ET_CORE: return "CORE (Core file)";
        default:      return "Unknown";
    }
}

const char* get_machine_name(uint16_t machine) {
    switch (machine) {
        case EM_X86_64:   return "Advanced Micro Devices X86-64";
        case EM_386:      return "Intel 80386";
        case EM_ARM:      return "ARM";
        case EM_MIPS:     return "MIPS R3000";
        case EM_AARCH64:  return "AArch64";
        case EM_PPC:      return "PowerPC";
        case EM_SPARC:    return "SPARC";
        default:          return "Unknown";
    }
}

const char* get_data_encoding(unsigned char data) {
    switch (data) {
        case ELFDATA2LSB: return "2's complement, little endian";
        case ELFDATA2MSB: return "2's complement, big endian";
        default:          return "Unknown";
    }
}


/**
 * @brief 打印 ELF 文件头部信息
 * 输出 ELF 文件头部的详细信息，包括文件魔数、类、数据编码方式、机器架构等。
 * @param ehdr ELF 文件头部结构体
 * @return void
 */
void print_file_header(Elf64_Ehdr *ehdr)
{
    printf("ELF Header:\n");
    
    // 打印所有 16 字节的魔数
    printf("  Magic:   ");
    int i;
    for (i = 0; i < EI_NIDENT; ++i) 
    {
        printf("%02x ", ehdr->e_ident[i]);
    }
    printf("\n");

    // 打印 ELF 类别
    printf("  Class:                             %s\n", (ehdr->e_ident[EI_CLASS] == ELFCLASS64) ? "ELF64" : "Unknown");

    // 打印数据编码方式
    printf("  Data:                              %s\n", get_data_encoding(ehdr->e_ident[EI_DATA]));

    // 打印 ELF 版本
    printf("  Version:                           %d (current)\n", ehdr->e_version);

    // 打印 OS/ABI
    printf("  OS/ABI:                            %s\n", get_osabi_name(ehdr->e_ident[EI_OSABI]));

    // 打印 ABI 版本
    printf("  ABI Version:                       %d\n", ehdr->e_ident[EI_ABIVERSION]);

    // 打印文件类型
    printf("  Type:                              %s\n", get_type_name(ehdr->e_type));

    // 打印机器架构
    printf("  Machine:                           %s\n", get_machine_name(ehdr->e_machine));

    // 打印版本
    printf("  Version:                           0x%x\n", ehdr->e_version);

    // 打印入口地址
    printf("  Entry point address:               0x%lx\n", ehdr->e_entry);

    // 打印程序头表偏移
    printf("  Start of program headers:          %d (bytes into file)\n", ehdr->e_phoff);

    // 打印节头表偏移
    printf("  Start of section headers:          %d (bytes into file)\n", ehdr->e_shoff);

    // 打印标志
    printf("  Flags:                             0x%x\n", ehdr->e_flags);

    // 打印 ELF 文件头大小
    printf("  Size of this header:               %d (bytes)\n", ehdr->e_ehsize);

    // 打印程序头大小和数量
    printf("  Size of program headers:           %d (bytes)\n", ehdr->e_phentsize);
    printf("  Number of program headers:         %d\n", ehdr->e_phnum);

    // 打印节头大小和数量
    printf("  Size of section headers:           %d (bytes)\n", ehdr->e_shentsize);
    printf("  Number of section headers:         %d\n", ehdr->e_shnum);

    // 打印节头字符串表索引
    printf("  Section header string table index: %d\n", ehdr->e_shstrndx);
}

const char *get_section_type(uint32_t type)
{
    switch (type)
    {
        case SHT_NULL: return "NULL";
        case SHT_PROGBITS: return "PROGBITS";
        case SHT_SYMTAB: return "SYMTAB";
        case SHT_STRTAB: return "STRTAB";
        case SHT_RELA: return "RELA";
        case SHT_HASH: return "HASH";
        case SHT_DYNAMIC: return "DYNAMIC";
        case SHT_NOTE: return "NOTE";
        case SHT_NOBITS: return "NOBITS";
        case SHT_REL: return "REL";
        case SHT_SHLIB: return "SHLIB";
        case SHT_DYNSYM: return "DYNSYM";
        default: return "UNKNOWN";
    }
}

// 解析并返回 Flags 字符串
const char* flags_to_string(Elf64_Xword flags)
{
    static char flag_str[16];
    flag_str[0] = '\0';
    
    if (flags & SHF_WRITE) strcat(flag_str, "W");
    if (flags & SHF_ALLOC) strcat(flag_str, "A");
    if (flags & SHF_EXECINSTR) strcat(flag_str, "X");
    if (flags & SHF_MERGE) strcat(flag_str, "M");
    if (flags & SHF_STRINGS) strcat(flag_str, "S");
    if (flags & SHF_INFO_LINK) strcat(flag_str, "I");
    if (flags & SHF_LINK_ORDER) strcat(flag_str, "L");
    if (flags & SHF_OS_NONCONFORMING) strcat(flag_str, "O");
    if (flags & SHF_GROUP) strcat(flag_str, "G");
    if (flags & SHF_TLS) strcat(flag_str, "T");
    if (flags & SHF_COMPRESSED) strcat(flag_str, "C");
    if (flags & SHF_EXCLUDE) strcat(flag_str, "E");
    
    if (flag_str[0] == '\0')
    {
        strcat(flag_str, " ");
    }
    
    return flag_str;
}

/**
 * @brief 打印 ELF 文件的节头信息
 * 输出 ELF 文件中所有节的头部信息，包括节名、类型、地址等。
 * @param elf ELF 文件的指针
 * @return void
 */
void print_section_headers(Elf *elf)
 {
    Elf_Scn *scn = NULL;
    Elf64_Shdr *shdr = NULL;
    Elf64_Ehdr *ehdr;
    char *shstrtab = NULL;
    size_t section_count = 0;
    size_t i;
    size_t shstrndx;
    off_t shoff;
    
    // 获取 ELF 文件头
    ehdr = elf64_getehdr(elf);
    if (!ehdr)
    {
        fprintf(stderr, "Failed to get ELF header\n");
        return;
    }
    
    // 获取节区头的数量和偏移量
    section_count = ehdr->e_shnum;
    shoff = ehdr->e_shoff;
    
    // 找到字符串表节区（用于节区名）
    if (elf_getshdrstrndx(elf, &shstrndx) != 0)
    {
        fprintf(stderr, "Failed to get section header string table index\n");
        return;
    }
    
    // 获取字符串表节区内容  
    shstrtab = elf_strptr(elf, shstrndx, 0);
    if (!shstrtab)
    {
        fprintf(stderr, "Failed to get section name string table\n");
        return;
    }  
    
    // 打印节区头信息  
    printf(
        "There are %zu section headers, starting at offset 0x%lx:\n\n",
        section_count,
        (unsigned long) shoff
    );
    
    printf("Section Headers:\n");
    printf("  [Nr] Name              Type             Address           Offset\n");
    printf("       Size              EntSize          Flags  Link  Info  Align\n");

    for (i = 0; i < section_count; ++i)
    {
        scn = elf_getscn(elf, i);
        if (!scn) continue;
        
        shdr = elf64_getshdr(scn);
        if (!shdr) continue;
        
        // 获取节区名称，如果 sh_name 为 0，使用 "" 作为默认值
        char *section_name = shstrtab + shdr->sh_name;
        if (shdr->sh_name == 0)
        {
            section_name = "";
        }
        
        // 打印节区信息
        printf(
            "  [%2zu] %-17s %-15s  %016lx  %08lx\n",
            i,
            section_name,
            get_section_type(shdr->sh_type), // 根据类型代码输出类型字符串
            shdr->sh_addr,
            shdr->sh_offset
        );  
        
        printf(
            "       %016lx  %016lx %3s %7u %5u %5lu\n",  
            shdr->sh_size,
            shdr->sh_entsize,
            flags_to_string(shdr->sh_flags), // 将标志转换为字符串表示
            shdr->sh_link,
            shdr->sh_info,
            shdr->sh_addralign
        );
    }
    
    // 输出 Flags 的键值说明  
    printf("Key to Flags:\n");
    printf("  W (write), A (alloc), X (execute), M (merge), S (strings), I (info),\n");
    printf("  L (link order), O (extra OS processing required), G (group), T (TLS),\n");
    printf("  C (compressed), x (unknown), o (OS specific), E (exclude),\n");
    printf("  l (large), p (processor specific)\n");
}

/**
 * @brief 打印 ELF 文件的符号表
 * 输出 ELF 文件中的符号表，列出符号名、值、大小等信息。
 * @param elf ELF 文件的指针
 * @return void
 */
 void print_symbol_table(Elf *elf)
 {
    Elf_Scn *scn = NULL;
    GElf_Shdr shdr;
    Elf_Data *data = NULL;
    int sym_count, i;
    
    // 查找符号表节区
    while ((scn = elf_nextscn(elf, scn)) != NULL)
    {
        gelf_getshdr(scn, &shdr);
        if (shdr.sh_type == SHT_SYMTAB) 
        {
            data = elf_getdata(scn, NULL);
            break;
        }
    }
    
    if (!data)
    {
        fprintf(stderr, "No symbol table found\n");
        return;
    }
    
    // 获取符号表条目数量
    sym_count = shdr.sh_size / shdr.sh_entsize;
    
    // 打印标题行
    printf("\nSymbol table '.symtab' contains %d entries:\n", sym_count);
    printf("   Num:    Value          Size Type    Bind   Vis      Ndx Name\n");
    
    // 遍历符号表并打印每个符号
    for (i = 0; i < sym_count; i++)
    {
        GElf_Sym sym;
        gelf_getsym(data, i, &sym);
        
        // 符号类型
        const char *type;
        switch (GELF_ST_TYPE(sym.st_info))
        {
            case STT_NOTYPE: type = "NOTYPE"; break;
            case STT_OBJECT: type = "OBJECT"; break;
            case STT_FUNC:   type = "FUNC";   break;
            case STT_SECTION:type = "SECTION";break;
            case STT_FILE:   type = "FILE";   break;
            default:         type = "UNKNOWN";break;
        }
        
        // 符号绑定
        const char *bind;
        switch (GELF_ST_BIND(sym.st_info))
        {
            case STB_LOCAL:  bind = "LOCAL";  break; 
            case STB_GLOBAL: bind = "GLOBAL"; break;
            case STB_WEAK:   bind = "WEAK";   break;
            default:         bind = "UNKNOWN";break;
        }
        
        // 符号可见性
        const char *vis;
        switch (GELF_ST_VISIBILITY(sym.st_other))
        {
            case STV_DEFAULT:   vis = "DEFAULT";   break;
            case STV_INTERNAL:  vis = "INTERNAL";  break;
            case STV_HIDDEN:    vis = "HIDDEN";    break;
            case STV_PROTECTED: vis = "PROTECTED"; break;
            default:            vis = "UNKNOWN";   break;
        }
        
        // 符号索引（节区索引）
        char ndx[10];
        if (sym.st_shndx == SHN_UNDEF)
        {
            strcpy(ndx, "UND");
        }
        else if (sym.st_shndx == SHN_ABS)
        {
            strcpy(ndx, "ABS");
        }
        else if (sym.st_shndx == SHN_COMMON)
        {
            strcpy(ndx, "COM");
        }
        else
        {
            snprintf(ndx, sizeof(ndx), "%d", sym.st_shndx);
        }
        
        // 符号名称
        const char *name = elf_strptr(
            elf, shdr.sh_link, sym.st_name);
        if (!name) name = "";
        
        // 打印符号信息
        printf(
            "%6d: %016lx %5lu %-7s %-6s %-8s %3s %s\n",
            i,
            (unsigned long)sym.st_value,
            (unsigned long)sym.st_size,
            type,
            bind,
            vis,
            ndx,
            name
        );
    }
}

/**
 * @brief 打印 ELF 文件的程序头信息
 * 输出 ELF 文件中的程序头信息，包括类型、偏移量、虚拟地址、文件大小等。
 * @param ehdr ELF 文件头部结构体
 * @param elf ELF 文件的指针
 * @return void
 */
void print_program_headers(Elf64_Ehdr *ehdr, Elf *elf) 
{
    Elf64_Phdr *phdr;
    size_t phdr_count;
    phdr_count = ehdr->e_phnum;

    if (phdr_count == 0) 
    {
        printf("\nThere are no program headers in this file.\n");
    } 
    else 
    {
        printf("Program Headers:\n");
        size_t i;
        for (i = 0; i < phdr_count; ++i) 
        {
            phdr = elf64_getphdr(elf) + i;
            printf(
                "  [%2zu] Type: 0x%x, Offset: 0x%lx, Virtual Address: 0x%lx, File Size: %ld, Memory Size: %ld\n",
                i,
                phdr->p_type,
                phdr->p_offset,
                phdr->p_vaddr,
                phdr->p_filesz,
                phdr->p_memsz
            );
        }
    }
}

/**  
 * @brief 获取 x86-64 架构的重定位类型名称  
 * 将重定位类型编号转换为对应的字符串，以便输出更加可读。  
 * @param rel_type 重定位类型编号  
 * @return const char* 对应的重定位类型名称字符串  
 */
 const char* elf_getreloc_type_name(uint32_t rel_type) 
 {  
    switch (rel_type) 
    {  
        case 0: return "R_X86_64_NONE";
        case 1: return "R_X86_64_64";
        case 2: return "R_X86_64_PC32";
        case 3: return "R_X86_64_GOT32";
        case 4: return "R_X86_64_PLT32";
        case 5: return "R_X86_64_COPY";
        case 6: return "R_X86_64_GLOB_DAT";
        case 7: return "R_X86_64_JUMP_SLOT";
        case 8: return "R_X86_64_RELATIVE";
        case 9: return "R_X86_64_GOTPCREL"; 
        case 10: return "R_X86_64_32";
        case 11: return "R_X86_64_32S";
        case 12: return "R_X86_64_16";
        case 13: return "R_X86_64_PC16";
        case 14: return "R_X86_64_8";
        case 15: return "R_X86_64_PC8";
        case 16: return "R_X86_64_DTPMOD64";
        case 17: return "R_X86_64_DTPOFF64";
        case 18: return "R_X86_64_TPOFF64";
        case 19: return "R_X86_64_TLSGD";
        case 20: return "R_X86_64_TLSLD";
        case 21: return "R_X86_64_DTPOFF32";
        case 22: return "R_X86_64_GOTTPOFF";
        case 23: return "R_X86_64_TPOFF32";
        case 24: return "R_X86_64_PC64";
        case 25: return "R_X86_64_GOTOFF64";
        case 26: return "R_X86_64_GOTPC32";
        case 27: return "R_X86_64_GOT64";
        case 28: return "R_X86_64_GOTPCREL64";
        case 29: return "R_X86_64_GOTPC64";
        case 30: return "R_X86_64_GOTPLT64";
        case 31: return "R_X86_64_PLTOFF64";
        case 32: return "R_X86_64_SIZE32";
        case 33: return "R_X86_64_SIZE64";
        case 34: return "R_X86_64_GOTPC32_TLSDESC";
        case 35: return "R_X86_64_TLSDESC_CALL";
        case 36: return "R_X86_64_TLSDESC";
        case 37: return "R_X86_64_IRELATIVE";
        case 38: return "R_X86_64_RELATIVE64";
        case 39: return "R_X86_64_GOTPCRELX";
        case 40: return "R_X86_64_REX_GOTPCRELX";
        default: return "UNKNOWN";
    }
}

/**
 * @brief 打印 ELF 文件的重定位信息
 * 输出 ELF 文件中的重定位信息，包括偏移量和重定位信息。
 * @param elf ELF 文件的指针
 * @return void
 */
void print_relocations(Elf *elf) 
{
    Elf_Scn *scn = NULL;
    Elf64_Shdr *shdr;
    Elf_Data *data;
    size_t shstrndx;
    elf_getshdrstrndx(elf, &shstrndx);

    // 遍历每个节
    while ((scn = elf_nextscn(elf, scn))!= NULL)
    {
        shdr = elf64_getshdr(scn);

        // 检查节类型是否为 SHT_REL 或 SHT_RELA
        if (shdr->sh_type == SHT_REL || shdr->sh_type == SHT_RELA)
        {
            data = elf_getdata(scn, NULL);
            size_t rel_count = shdr->sh_size / shdr->sh_entsize;

            // 获取符号表
            Elf_Scn *symtab_scn = elf_getscn(elf, shdr->sh_link);
            Elf64_Shdr *symtab_shdr = elf64_getshdr(symtab_scn);
            Elf_Data *sym_data = elf_getdata(symtab_scn, NULL);

            // 打印标题行
            printf("\nRelocation section '%.*s' at offset 0x%lx contains %lu entries:\n",
                   (int)shdr->sh_name, elf_strptr(elf, shstrndx, shdr->sh_name),
                   shdr->sh_offset, rel_count);
            printf("  Offset          Info           Type           Sym. Value    Sym. Name + Addend\n");

            // 打印重定位条目信息
            size_t i;
            for (i = 0; i < rel_count; i++) 
            {
                Elf64_Rela *rela = &((Elf64_Rela *)data->d_buf)[i];
                // 提取重定位信息
                size_t sym_index = ELF64_R_SYM(rela->r_info);
                size_t rel_type = ELF64_R_TYPE(rela->r_info);

                // 获取符号名
                GElf_Sym sym;
                gelf_getsym(sym_data, sym_index, &sym);

                const char *sym_name = elf_strptr(elf, symtab_shdr->sh_link, sym.st_name);
                if (sym.st_name == 0) 
                {
                    sym_name = elf_strptr(elf, shstrndx, elf64_getshdr(elf_getscn(elf, sym.st_shndx))->sh_name);
                    if (sym_name == NULL)
                    {
                        sym_name = "UNKNOWN";
                    }
                }

                // 格式化附加值输出并取绝对值
                long long abs_addend = llabs(rela->r_addend);
                char addend_str[32];
                if (rela->r_addend >= 0) 
                {
                    snprintf(addend_str, sizeof(addend_str), "+ %lx", abs_addend);
                } 
                else 
                {
                    snprintf(addend_str, sizeof(addend_str), "- %lx", abs_addend);
                }

                // 打印条目信息
                printf(
                    "%012lx  %012lx %-16s  %016lx %s %s\n",
                    rela->r_offset, // 偏移量
                    rela->r_info, // info 字段
                    elf_getreloc_type_name(rel_type), // 重定位类型
                    sym.st_value, // 符号值
                    sym_name, // 符号名称
                    addend_str
                );
            }
        }
    }
}
  
/**
 * @brief 主程序入口
 * 解析命令行选项，读取 ELF 文件并打印相关信息。根据用户选择的选项显示文件头、节头、符号表、程序头和重定位等内容。
 * @param argc 命令行参数个数
 * @param argv 命令行参数
 * @return int 返回状态码，成功返回 0，失败返回 1
 */
 int main(int argc, char *argv[]) 
 {  
    int opt;  
    int show_help = 0;
    int show_version = 0;
    int show_file_header = 0;
    int show_section_headers = 0;
    int show_syms = 0;
    int show_relocs = 0;
    int show_program_header = 0;
    
    // 解析命令行选项  
    struct option long_options[] = 
    {  
        { "help", no_argument, NULL, 'H' }, 
        { "version", no_argument, NULL, 'v' },
        { "file-header", no_argument, NULL, 'h' },
        { "section-headers", no_argument, NULL, 'S' },
        { "syms", no_argument, NULL, 's' },
        { "relocs", no_argument, NULL, 'r' },
        { "program-header", no_argument, NULL, 'l' },
        { "silent-truncation", no_argument, NULL, 'T' },
        { "wide", no_argument, NULL, 'W' },
        { 0, 0, 0, 0 }
    };  
    
    while ((opt = getopt_long(argc, argv, "HvhSlrTWs", long_options, NULL)) != -1)
    {  
        switch (opt)
        {  
            case 'H': show_help = 1;            break;  
            case 'v': show_version = 1;         break;  
            case 'h': show_file_header = 1;     break;  
            case 'S': show_section_headers = 1; break;  
            case 's': show_syms = 1;            break;  
            case 'r': show_relocs = 1;          break;  
            case 'l': show_program_header = 1;  break;  
            case 'T':  
            case 'W':                           break;  
            default: show_help = 1;             break;  
        }  
    }  
  
    if (show_help)
    {  
        print_help();
        return 0;
    }  
    
    if (show_version) 
    {  
        print_version();  
        return 0;  
    }  
    
    if (optind >= argc) 
    {  
        fprintf(stderr, "Error: No ELF file specified\n");  
        return 1;
    }  
    
    char *filename = argv[optind];  
    int fd = open(filename, O_RDONLY);  
    if (fd == -1) 
    {  
        perror("Error opening file");  
        return 1;  
    }
    
    Elf *elf;  
    if (elf_version(EV_CURRENT) == EV_NONE)
    {  
        fprintf(stderr, "Error initializing libelf\n");  
        close(fd);  
        return 1;  
    }  
  
    elf = elf_begin(fd, ELF_C_READ, NULL);  
    if (elf == NULL)
    {  
        fprintf(stderr, "Error reading ELF file: %s\n", elf_errmsg(-1));
        close(fd);
        return 1;
    }
    
    Elf64_Ehdr *ehdr = elf64_getehdr(elf);  
    if (ehdr == NULL)
    {
        fprintf(stderr, "Error getting ELF header\n");
        elf_end(elf);
        close(fd);
        return 1;
    }
    
    if (show_file_header) 
    {  
        print_file_header(ehdr);  
    }
    
    if (show_section_headers) 
    {
        print_section_headers(elf);
    }
    
    if (show_syms) 
    {
        print_symbol_table(elf);
    }
    
    if (show_relocs) 
    {  
        print_relocations(elf);  
    }  
    
    if (show_program_header) 
    {  
        print_program_headers(ehdr, elf);  
    }  
    
    elf_end(elf);
    close(fd);
    
    return 0;
}

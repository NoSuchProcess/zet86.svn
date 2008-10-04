// ROM BIOS compatability entry points:
// ===================================
// $e05b ; POST Entry Point
// $e6f2 ; INT 19h Boot Load Service Entry Point
// $f045 ; INT 10 Functions 0-Fh Entry Point
// $f065 ; INT 10h Video Support Service Entry Point
// $f0a4 ; MDA/CGA Video Parameter Table (INT 1Dh)
// $fff0 ; Power-up Entry Point
// $fff5 ; ASCII Date ROM was built - 8 characters in MM/DD/YY
// $fffe ; System Model ID

#include "rombios.h"

   /* model byte 0xFC = AT */
#define SYS_MODEL_ID     0xFC

#ifndef BIOS_BUILD_DATE
#  define BIOS_BUILD_DATE "06/23/99"
#endif

  // 1K of base memory used for Extended Bios Data Area (EBDA)
  // EBDA is used for PS/2 mouse support, and IDE BIOS, etc.
#define EBDA_SEG           0x9FC0
#define EBDA_SIZE          1              // In KiB
#define BASE_MEM_IN_K   (640 - EBDA_SIZE)

/* 256 bytes at 0x9ff00 -- 0x9ffff is used for the IPL boot table. */
#define IPL_SEG              0x9ff0
#define IPL_TABLE_OFFSET     0x0000
#define IPL_TABLE_ENTRIES    8
#define IPL_COUNT_OFFSET     0x0080  /* u16: number of valid table entries */
#define IPL_SEQUENCE_OFFSET  0x0082  /* u16: next boot device */
#define IPL_BOOTFIRST_OFFSET 0x0084  /* u16: user selected device */
#define IPL_SIZE             0xff
#define IPL_TYPE_FLOPPY      0x01
#define IPL_TYPE_HARDDISK    0x02
#define IPL_TYPE_CDROM       0x03
#define IPL_TYPE_BEV         0x80

// This is for compiling with gcc2 and gcc3
#define ASM_START #asm
#define ASM_END #endasm

ASM_START
.rom

.org 0x0000

use16 8086

MACRO SET_INT_VECTOR
  mov ax, ?3
  mov ?1*4, ax
  mov ax, ?2
  mov ?1*4+2, ax
MEND

ASM_END

typedef unsigned char  Bit8u;
typedef unsigned short Bit16u;
typedef unsigned short bx_bool;
typedef unsigned long  Bit32u;


  void memsetb(seg,offset,value,count);
  void memcpyb(dseg,doffset,sseg,soffset,count);
  void memcpyd(dseg,doffset,sseg,soffset,count);

  // memset of count bytes
    void
  memsetb(seg,offset,value,count)
    Bit16u seg;
    Bit16u offset;
    Bit16u value;
    Bit16u count;
  {
  ASM_START
    push bp
    mov  bp, sp

      push ax
      push cx
      push es
      push di

      mov  cx, 10[bp] ; count
      test cx, cx
      je   memsetb_end
      mov  ax, 4[bp] ; segment
      mov  es, ax
      mov  ax, 6[bp] ; offset
      mov  di, ax
      mov  al, 8[bp] ; value
      cld
      rep
       stosb

  memsetb_end:
      pop di
      pop es
      pop cx
      pop ax

    pop bp
  ASM_END
  }

  // memcpy of count bytes
    void
  memcpyb(dseg,doffset,sseg,soffset,count)
    Bit16u dseg;
    Bit16u doffset;
    Bit16u sseg;
    Bit16u soffset;
    Bit16u count;
  {
  ASM_START
    push bp
    mov  bp, sp

      push ax
      push cx
      push es
      push di
      push ds
      push si

      mov  cx, 12[bp] ; count
      test cx, cx
      je   memcpyb_end
      mov  ax, 4[bp] ; dsegment
      mov  es, ax
      mov  ax, 6[bp] ; doffset
      mov  di, ax
      mov  ax, 8[bp] ; ssegment
      mov  ds, ax
      mov  ax, 10[bp] ; soffset
      mov  si, ax
      cld
      rep
       movsb

  memcpyb_end:
      pop si
      pop ds
      pop di
      pop es
      pop cx
      pop ax

    pop bp
  ASM_END
  }

  // Bit32u (unsigned long) and long helper functions
  ASM_START

  idiv_u:
    xor dx,dx
    div bx
    ret

  ldivul:
    mov     cx,[di]
    mov     di,2[di]
    call    ludivmod
    xchg    ax,cx
    xchg    bx,di
    ret

.align 2
ldivmod:
    mov     dx,di           ; sign byte of b in dh
    mov     dl,bh           ; sign byte of a in dl
    test    di,di
    jns     set_asign
    neg     di
    neg     cx
    sbb     di,*0
set_asign:
    test    bx,bx
    jns     got_signs       ; leave r = a positive
    neg     bx
    neg     ax
    sbb     bx,*0
    j       got_signs

.align 2
ludivmod:
    xor     dx,dx           ; both sign bytes 0
got_signs:
    push    bp
    push    si
    mov     bp,sp
    push    di              ; remember b
    push    cx
b0  =       -4
b16 =       -2

    test    di,di
    jne     divlarge
    test    cx,cx
    je      divzero
    cmp     bx,cx
    jae     divlarge        ; would overflow
    xchg    dx,bx           ; a in dx:ax, signs in bx
    div     cx
    xchg    cx,ax           ; q in di:cx, junk in ax
    xchg    ax,bx           ; signs in ax, junk in bx
    xchg    ax,dx           ; r in ax, signs back in dx
    mov     bx,di           ; r in bx:ax
    j       zdivu1

divzero:                        ; return q = 0 and r = a
    test    dl,dl
    jns     return
    j       negr            ; a initially minus, restore it

divlarge:
    push    dx              ; remember sign bytes
    mov     si,di           ; w in si:dx, initially b from di:cx
    mov     dx,cx
    xor     cx,cx           ; q in di:cx, initially 0
    mov     di,cx
                            ; r in bx:ax, initially a
                            ; use di:cx rather than dx:cx in order
                            ; to have dx free for a byte pair later
    cmp     si,bx
    jb      loop1
    ja      zdivu           ; finished if b > r
    cmp     dx,ax
    ja      zdivu

; rotate w (= b) to greatest dyadic multiple of b <= r

loop1:
    shl     dx,*1           ; w = 2*w
    rcl     si,*1
    jc      loop1_exit      ; w was > r counting overflow (unsigned)
    cmp     si,bx           ; while w <= r (unsigned)
    jb      loop1
    ja      loop1_exit
    cmp     dx,ax
    jbe     loop1           ; else exit with carry clear for rcr
loop1_exit:
    rcr     si,*1
    rcr     dx,*1
loop2:
    shl     cx,*1           ; q = 2*q
    rcl     di,*1
    cmp     si,bx           ; if w <= r
    jb      loop2_over
    ja      loop2_test
    cmp     dx,ax
    ja      loop2_test
loop2_over:
    add     cx,*1           ; q++
    adc     di,*0
    sub     ax,dx           ; r = r-w
    sbb     bx,si
loop2_test:
    shr     si,*1           ; w = w/2
    rcr     dx,*1
    cmp     si,b16[bp]      ; while w >= b
    ja      loop2
    jb      zdivu
    cmp     dx,b0[bp]
    jae     loop2

zdivu:
    pop     dx              ; sign bytes
zdivu1:
    test    dh,dh
    js      zbminus
    test    dl,dl
    jns     return          ; else a initially minus, b plus
    mov     dx,ax           ; -a = b * q + r ==> a = b * (-q) + (-r)
    or      dx,bx
    je      negq            ; use if r = 0
    sub     ax,b0[bp]       ; use a = b * (-1 - q) + (b - r)
    sbb     bx,b16[bp]
    not     cx              ; q = -1 - q (same as complement)
    not     di
negr:
    neg     bx
    neg     ax
    sbb     bx,*0
return:
    mov     sp,bp
    pop     si
    pop     bp
    ret

.align 2
zbminus:
    test    dl,dl           ; (-a) = (-b) * q + r ==> a = b * q + (-r)
    js      negr            ; use if initial a was minus
    mov     dx,ax           ; a = (-b) * q + r ==> a = b * (-q) + r
    or      dx,bx
    je      negq            ; use if r = 0
    sub     ax,b0[bp]       ; use a = b * (-1 - q) + (b + r)
                                ; (b is now -b)
    sbb     bx,b16[bp]
    not     cx
    not     di
    mov     sp,bp
    pop     si
    pop     bp
    ret

.align 2
negq:
    neg     di
    neg     cx
    sbb     di,*0
    mov     sp,bp
    pop     si
    pop     bp
    ret

.align 2
ltstl:
ltstul:
    test    bx,bx
    je      ltst_not_sure
    ret

.align 2
ltst_not_sure:
    test    ax,ax
    js      ltst_fix_sign
    ret

.align 2
ltst_fix_sign:
    inc     bx
    ret

.align 2
lmull:
lmulul:
    mov     cx,ax
    mul     word ptr 2[di]
    xchg    ax,bx
    mul     word ptr [di]
    add     bx,ax
    mov     ax,ptr [di]
    mul     cx
    add     bx,dx
    ret

.align 2
lsubl:
lsubul:
    sub     ax,[di]
    sbb     bx,2[di]
    ret

.align 2
laddl:
laddul:
    add     ax,[di]
    adc     bx,2[di]
    ret

.align 2
lorl:
lorul:
    or      ax,[di]
    or      bx,2[di]
    ret

.align 2
lsrul:
    mov     cx,di
    jcxz    lsru_exit
    cmp     cx,*32
    jae     lsru_zero
lsru_loop:
    shr     bx,*1
    rcr     ax,*1
    loop    lsru_loop
lsru_exit:
    ret

.align 2
lsru_zero:
    xor     ax,ax
    mov     bx,ax
    ret

.align 2
landl:
landul:
    and     ax,[di]
    and     bx,2[di]
    ret

.align 2
lcmpl:
lcmpul:
    sub     bx,2[di]
    je      lcmp_not_sure
    ret

.align 2
lcmp_not_sure:
    cmp     ax,[di]
    jb      lcmp_b_and_lt
    jge     lcmp_exit

    inc     bx
lcmp_exit:
    ret

.align 2
lcmp_b_and_lt:
    dec     bx
    ret

  ASM_END

typedef struct {
  Bit16u type;
  Bit16u flags;
  Bit32u vector;
  Bit32u description;
  Bit32u reserved;
  } ipl_entry_t;



static Bit8u          inb_cmos();

static Bit8u          read_byte();
static Bit16u         read_word();
static void           write_byte();
static void           write_word();
static void           bios_printf();

static void           int19_function();
static Bit16u         get_CS();
static Bit16u         get_SS();

static void           print_bios_banner();
static void           print_boot_device();

  Bit8u
inb_cmos(cmos_reg)
  Bit8u cmos_reg;
{
ASM_START
  push bp
  mov  bp, sp

    mov  al, 4[bp] ;; cmos_reg
    out 0x70, al
    in  al, 0x71

  pop  bp
ASM_END
}

  Bit8u
read_byte(seg, offset)
  Bit16u seg;
  Bit16u offset;
{
ASM_START
  push bp
  mov  bp, sp

    push bx
    push ds
    mov  ax, 4[bp] ; segment
    mov  ds, ax
    mov  bx, 6[bp] ; offset
    mov  al, [bx]
    ;; al = return value (byte)
    pop  ds
    pop  bx

  pop  bp
ASM_END
}

  Bit16u
read_word(seg, offset)
  Bit16u seg;
  Bit16u offset;
{
ASM_START
  push bp
  mov  bp, sp

    push bx
    push ds
    mov  ax, 4[bp] ; segment
    mov  ds, ax
    mov  bx, 6[bp] ; offset
    mov  ax, [bx]
    ;; ax = return value (word)
    pop  ds
    pop  bx

  pop  bp
ASM_END
}

  void
write_byte(seg, offset, data)
  Bit16u seg;
  Bit16u offset;
  Bit8u data;
{
ASM_START
  push bp
  mov  bp, sp

    push ax
    push bx
    push ds
    mov  ax, 4[bp] ; segment
    mov  ds, ax
    mov  bx, 6[bp] ; offset
    mov  al, 8[bp] ; data byte
    mov  [bx], al  ; write data byte
    pop  ds
    pop  bx
    pop  ax

  pop  bp
ASM_END
}

  void
write_word(seg, offset, data)
  Bit16u seg;
  Bit16u offset;
  Bit16u data;
{
ASM_START
  push bp
  mov  bp, sp

    push ax
    push bx
    push ds
    mov  ax, 4[bp] ; segment
    mov  ds, ax
    mov  bx, 6[bp] ; offset
    mov  ax, 8[bp] ; data word
    mov  [bx], ax  ; write data word
    pop  ds
    pop  bx
    pop  ax

  pop  bp
ASM_END
}

  Bit16u
get_CS()
{
ASM_START
  mov  ax, cs
ASM_END
}

  Bit16u
get_SS()
{
ASM_START
  mov  ax, ss
ASM_END
}

  void
wrch(c)
  Bit8u  c;
{
  ASM_START
  push bp
  mov  bp, sp

  push bx
  mov  ah, #0x0e
  mov  al, 4[bp]
  xor  bx,bx
  int  #0x10
  pop  bx

  pop  bp
  ASM_END
}

  void
send(action, c)
  Bit16u action;
  Bit8u  c;
{
  if (action & BIOS_PRINTF_SCREEN) {
    if (c == '\n') wrch('\r');
    wrch(c);
  }
}

  void
put_int(action, val, width, neg)
  Bit16u action;
  short val, width;
  bx_bool neg;
{
  short nval = val / 10;
  if (nval)
    put_int(action, nval, width - 1, neg);
  else {
    while (--width > 0) send(action, ' ');
    if (neg) send(action, '-');
  }
  send(action, val - (nval * 10) + '0');
}

  void
put_uint(action, val, width, neg)
  Bit16u action;
  unsigned short val;
  short width;
  bx_bool neg;
{
  unsigned short nval = val / 10;
  if (nval)
    put_uint(action, nval, width - 1, neg);
  else {
    while (--width > 0) send(action, ' ');
    if (neg) send(action, '-');
  }
  send(action, val - (nval * 10) + '0');
}

  void
put_luint(action, val, width, neg)
  Bit16u action;
  unsigned long val;
  short width;
  bx_bool neg;
{
  unsigned long nval = val / 10;
  if (nval)
    put_luint(action, nval, width - 1, neg);
  else {
    while (--width > 0) send(action, ' ');
    if (neg) send(action, '-');
  }
  send(action, val - (nval * 10) + '0');
}

void put_str(action, segment, offset)
  Bit16u action;
  Bit16u segment;
  Bit16u offset;
{
  Bit8u c;

  while (c = read_byte(segment, offset)) {
    send(action, c);
    offset++;
  }
}

//--------------------------------------------------------------------------
// bios_printf()
//   A compact variable argument printf function.
//
//   Supports %[format_width][length]format
//   where format can be x,X,u,d,s,S,c
//   and the optional length modifier is l (ell)
//--------------------------------------------------------------------------
  void
bios_printf(action, s)
  Bit16u action;
  Bit8u *s;
{
  Bit8u c, format_char;
  bx_bool  in_format;
  short i;
  Bit16u  *arg_ptr;
  Bit16u   arg_seg, arg, nibble, hibyte, shift_count, format_width, hexadd;

  arg_ptr = &s;
  arg_seg = get_SS();

  in_format = 0;
  format_width = 0;

  if ((action & BIOS_PRINTF_DEBHALT) == BIOS_PRINTF_DEBHALT)
    bios_printf (BIOS_PRINTF_SCREEN, "FATAL: ");

  while (c = read_byte(get_CS(), s)) {
    if ( c == '%' ) {
      in_format = 1;
      format_width = 0;
      }
    else if (in_format) {
      if ( (c>='0') && (c<='9') ) {
        format_width = (format_width * 10) + (c - '0');
        }
      else {
        arg_ptr++; // increment to next arg
        arg = read_word(arg_seg, arg_ptr);
        if (c == 'x' || c == 'X') {
          if (format_width == 0)
            format_width = 4;
          if (c == 'x')
            hexadd = 'a';
          else
            hexadd = 'A';
          for (i=format_width-1; i>=0; i--) {
            nibble = (arg >> (4 * i)) & 0x000f;
            send (action, (nibble<=9)? (nibble+'0') : (nibble-10+hexadd));
            }
          }
        else if (c == 'u') {
          put_uint(action, arg, format_width, 0);
          }
        else if (c == 'l') {
          s++;
          c = read_byte(get_CS(), s); /* is it ld,lx,lu? */
          arg_ptr++; /* increment to next arg */
          hibyte = read_word(arg_seg, arg_ptr);
          if (c == 'd') {
            if (hibyte & 0x8000)
              put_luint(action, 0L-(((Bit32u) hibyte << 16) | arg), format_width-1, 1);
            else
              put_luint(action, ((Bit32u) hibyte << 16) | arg, format_width, 0);
           }
          else if (c == 'u') {
            put_luint(action, ((Bit32u) hibyte << 16) | arg, format_width, 0);
           }
          else if (c == 'x' || c == 'X')
           {
            if (format_width == 0)
              format_width = 8;
            if (c == 'x')
              hexadd = 'a';
            else
              hexadd = 'A';
            for (i=format_width-1; i>=0; i--) {
              nibble = ((((Bit32u) hibyte <<16) | arg) >> (4 * i)) & 0x000f;
              send (action, (nibble<=9)? (nibble+'0') : (nibble-10+hexadd));
              }
           }
          }
        else if (c == 'd') {
          if (arg & 0x8000)
            put_int(action, -arg, format_width - 1, 1);
          else
            put_int(action, arg, format_width, 0);
          }
        else if (c == 's') {
          put_str(action, get_CS(), arg);
          }
        else if (c == 'S') {
          hibyte = arg;
          arg_ptr++;
          arg = read_word(arg_seg, arg_ptr);
          put_str(action, hibyte, arg);
          }
        else if (c == 'c') {
          send(action, arg);
          }
        else
          BX_PANIC("bios_printf: unknown format\n");
          in_format = 0;
        }
      }
    else {
      send(action, c);
      }
    s ++;
    }

  if (action & BIOS_PRINTF_HALT) {
    // freeze in a busy loop.
ASM_START
    cli
 halt2_loop:
    hlt
    jmp halt2_loop
ASM_END
    }
}

static char bios_svn_version_string[] = "$Revision: 1.2 $ $Date: 2008-10-04 23:11:53 $";

//--------------------------------------------------------------------------
// print_bios_banner
//   displays a the bios version
//--------------------------------------------------------------------------
void
print_bios_banner()
{
  printf("Zet BIOS - build: %s\n%s\n",
    BIOS_BUILD_DATE, bios_svn_version_string);
}

//--------------------------------------------------------------------------
// BIOS Boot Specification 1.0.1 compatibility
//
// Very basic support for the BIOS Boot Specification, which allows expansion
// ROMs to register themselves as boot devices, instead of just stealing the
// INT 19h boot vector.
//
// This is a hack: to do it properly requires a proper PnP BIOS and we aren't
// one; we just lie to the option ROMs to make them behave correctly.
// We also don't support letting option ROMs register as bootable disk
// drives (BCVs), only as bootable devices (BEVs).
//
// http://www.phoenix.com/en/Customer+Services/White+Papers-Specs/pc+industry+specifications.htm
//--------------------------------------------------------------------------

static char drivetypes[][10]={"", "Floppy","Hard Disk","CD-Rom", "Network"};

static void
init_boot_vectors()
{
  ipl_entry_t e;
  Bit16u count = 0;
  Bit16u ss = get_SS();

  /* Clear out the IPL table. */
  memsetb(IPL_SEG, IPL_TABLE_OFFSET, 0, IPL_SIZE);

  /* User selected device not set */
  write_word(IPL_SEG, IPL_BOOTFIRST_OFFSET, 0xFFFF);

  /*
   * Zet: We don't have support for floppy, hdd or cdrom
   */

  /* Remember how many devices we have */
  write_word(IPL_SEG, IPL_COUNT_OFFSET, count);
  /* Not tried booting anything yet */
  write_word(IPL_SEG, IPL_SEQUENCE_OFFSET, 0xffff);
}

static Bit8u
get_boot_vector(i, e)
Bit16u i; ipl_entry_t *e;
{
  Bit16u count;
  Bit16u ss = get_SS();
  /* Get the count of boot devices, and refuse to overrun the array */
  count = read_word(IPL_SEG, IPL_COUNT_OFFSET);
  if (i >= count) return 0;
  /* OK to read this device */
  memcpyb(ss, e, IPL_SEG, IPL_TABLE_OFFSET + i * sizeof (*e), sizeof (*e));
  return 1;
}

//--------------------------------------------------------------------------
// print_boot_device
//   displays the boot device
//--------------------------------------------------------------------------

void
print_boot_device(e)
  ipl_entry_t *e;
{
  Bit16u type;
  char description[33];
  Bit16u ss = get_SS();
  type = e->type;
  /* NIC appears as type 0x80 */
  if (type == IPL_TYPE_BEV) type = 0x4;
  if (type == 0 || type > 0x4) BX_PANIC("Bad drive type\n");
  printf("Booting from %s", drivetypes[type]);
  /* print product string if BEV */
  if (type == 4 && e->description != 0) {
    /* first 32 bytes are significant */
    memcpyb(ss, &description, (Bit16u)(e->description >> 16), (Bit16u)(e->description & 0xffff), 32);
    /* terminate string */
    description[32] = 0;
    printf(" [%S]", ss, description);
  }
  printf("...\n");
}

void
int19_function(seq_nr)
Bit16u seq_nr;
{
  Bit16u ebda_seg=read_word(0x0040,0x000E);
  Bit16u bootdev;
  Bit8u  bootdrv;
  Bit8u  bootchk;
  Bit16u bootseg;
  Bit16u bootip;
  Bit16u status;
  Bit16u bootfirst;

  ipl_entry_t e;

  // Here we assume that BX_ELTORITO_BOOT is defined, so
  //   CMOS regs 0x3D and 0x38 contain the boot sequence:
  //     CMOS reg 0x3D & 0x0f : 1st boot device
  //     CMOS reg 0x3D & 0xf0 : 2nd boot device
  //     CMOS reg 0x38 & 0xf0 : 3rd boot device
  //   boot device codes:
  //     0x00 : not defined
  //     0x01 : first floppy
  //     0x02 : first harddrive
  //     0x03 : first cdrom
  //     0x04 - 0x0f : PnP expansion ROMs (e.g. Etherboot)
  //     else : boot failure

  // Get the boot sequence
  bootdev = inb_cmos(0x3d);
  bootdev |= ((inb_cmos(0x38) & 0xf0) << 4);
  bootdev >>= 4 * seq_nr;
  bootdev &= 0xf;

  /* Read user selected device */
  bootfirst = read_word(IPL_SEG, IPL_BOOTFIRST_OFFSET);
  if (bootfirst != 0xFFFF) {
    bootdev = bootfirst;
    /* User selected device not set */
    write_word(IPL_SEG, IPL_BOOTFIRST_OFFSET, 0xFFFF);
    /* Reset boot sequence */
    write_word(IPL_SEG, IPL_SEQUENCE_OFFSET, 0xFFFF);
  } else if (bootdev == 0) BX_PANIC("No bootable device.\n");

  /* Translate from CMOS runes to an IPL table offset by subtracting 1 */
  bootdev -= 1;

  /* Read the boot device from the IPL table */
  if (get_boot_vector(bootdev, &e) == 0) {
    BX_INFO("Invalid boot device (0x%x)\n", bootdev);
    return;
  }

  /* Do the loading, and set up vector as a far pointer to the boot
   * address, and bootdrv as the boot drive */
  print_boot_device(&e);

  switch(e.type) {
  case IPL_TYPE_BEV: /* Expansion ROM with a Bootstrap Entry Vector (a far pointer) */
    bootseg = e.vector >> 16;
    bootip = e.vector & 0xffff;
    break;

  default: return;
  }

  /* Debugging info */
  BX_INFO("Booting from %x:%x\n", bootseg, bootip);

  /* Jump to the boot vector */
ASM_START
    mov  bp, sp
    ;; Build an iret stack frame that will take us to the boot vector.
    ;; iret pops ip, then cs, then flags, so push them in the opposite order.
    pushf
    mov  ax, _int19_function.bootseg + 0[bp]
    push ax
    mov  ax, _int19_function.bootip + 0[bp]
    push ax
    ;; Set the magic number in ax and the boot drive in dl.
    mov  ax, #0xaa55
    mov  dl, _int19_function.bootdrv + 0[bp]
    ;; Zero some of the other registers.
    xor  bx, bx
    mov  ds, bx
    mov  es, bx
    mov  bp, bx
    ;; Go!
    iret
ASM_END
}

ASM_START
;----------
;- INT18h -
;----------
int18_handler: ;; Boot Failure recovery: try the next device.

  ;; Reset SP and SS
  mov  ax, #0xfffe
  mov  sp, ax
  xor  ax, ax
  mov  ss, ax

  ;; Get the boot sequence number out of the IPL memory
  mov  bx, #IPL_SEG
  mov  ds, bx                     ;; Set segment
  mov  bx, IPL_SEQUENCE_OFFSET    ;; BX is now the sequence number
  inc  bx                         ;; ++
  mov  IPL_SEQUENCE_OFFSET, bx    ;; Write it back
  mov  ds, ax                     ;; and reset the segment to zero.

  ;; Carry on in the INT 19h handler, using the new sequence number
  push bx

  jmp  int19_next_boot

;----------
;- INT19h -
;----------
int19_relocated: ;; Boot function, relocated

  ;; int19 was beginning to be really complex, so now it
  ;; just calls a C function that does the work

  push bp
  mov  bp, sp

  ;; Reset SS and SP
  mov  ax, #0xfffe
  mov  sp, ax
  xor  ax, ax
  mov  ss, ax

  ;; Start from the first boot device (0, in AX)
  mov  bx, #IPL_SEG
  mov  ds, bx                     ;; Set segment to write to the IPL memory
  mov  IPL_SEQUENCE_OFFSET, ax    ;; Save the sequence number
  mov  ds, ax                     ;; and reset the segment.

  push ax

int19_next_boot:

  ;; Call the C code for the next boot device
  call _int19_function

  ;; Boot failed: invoke the boot recovery function
  int  #0x18

;--------------------
;- POST: EBDA segment
;--------------------
; relocated here because the primary POST area isnt big enough.
ebda_post:
  xor ax, ax            ; mov EBDA seg into 40E
  mov ds, ax
  mov word ptr [0x40E], #EBDA_SEG
  ret;;

rom_checksum:
  push ax
  push bx
  push cx
  xor  ax, ax
  xor  bx, bx
  xor  cx, cx
  mov  ch, [2]
  shl  cx, #1
checksum_loop:
  add  al, [bx]
  inc  bx
  loop checksum_loop
  and  al, #0xff
  pop  cx
  pop  bx
  pop  ax
  ret


;; We need a copy of this string, but we are not actually a PnP BIOS,
;; so make sure it is *not* aligned, so OSes will not see it if they scan.
.align 16
  db 0
pnp_string:
  .ascii "$PnP"


rom_scan:
  ;; Scan for existence of valid expansion ROMS.
  ;;   Video ROM:   from 0xC0000..0xC7FFF in 2k increments
  ;;   General ROM: from 0xC8000..0xDFFFF in 2k increments
  ;;   System  ROM: only 0xE0000
  ;;
  ;; Header:
  ;;   Offset    Value
  ;;   0         0x55
  ;;   1         0xAA
  ;;   2         ROM length in 512-byte blocks
  ;;   3         ROM initialization entry point (FAR CALL)

rom_scan_loop:
  push ax       ;; Save AX
  mov  ds, cx
  mov  ax, #0x0004 ;; start with increment of 4 (512-byte) blocks = 2k
  cmp [0], #0xAA55 ;; look for signature
  jne  rom_scan_increment
  call rom_checksum
  jnz  rom_scan_increment
  mov  al, [2]  ;; change increment to ROM length in 512-byte blocks

  ;; We want our increment in 512-byte quantities, rounded to
  ;; the nearest 2k quantity, since we only scan at 2k intervals.
  test al, #0x03
  jz   block_count_rounded
  and  al, #0xfc ;; needs rounding up
  add  al, #0x04
block_count_rounded:

  xor  bx, bx   ;; Restore DS back to 0000:
  mov  ds, bx
  push ax       ;; Save AX
  push di       ;; Save DI
  ;; Push addr of ROM entry point
  push cx       ;; Push seg
  push #0x0003  ;; Push offset

  ;; Point ES:DI at "$PnP", which tells the ROM that we are a PnP BIOS.
  ;; That should stop it grabbing INT 19h; we will use its BEV instead.
  mov  ax, #0xf000
  mov  es, ax
  lea  di, pnp_string

  mov  bp, sp   ;; Call ROM init routine using seg:off on stack
  db   0xff     ;; call_far ss:[bp+0]
  db   0x5e
  db   0
  cli           ;; In case expansion ROM BIOS turns IF on
  add  sp, #2   ;; Pop offset value
  pop  cx       ;; Pop seg value (restore CX)

  ;; Look at the ROM's PnP Expansion header.  Properly, we're supposed
  ;; to init all the ROMs and then go back and build an IPL table of
  ;; all the bootable devices, but we can get away with one pass.
  mov  ds, cx       ;; ROM base
  mov  bx, 0x001a   ;; 0x1A is the offset into ROM header that contains...
  mov  ax, [bx]     ;; the offset of PnP expansion header, where...
  cmp  ax, #0x5024  ;; we look for signature "$PnP"
  jne  no_bev
  mov  ax, 2[bx]
  cmp  ax, #0x506e
  jne  no_bev
  mov  ax, 0x1a[bx] ;; 0x1A is also the offset into the expansion header of...
  cmp  ax, #0x0000  ;; the Bootstrap Entry Vector, or zero if there is none.
  je   no_bev

  ;; Found a device that thinks it can boot the system.  Record its BEV and product name string.
  mov  di, 0x10[bx]            ;; Pointer to the product name string or zero if none
  mov  bx, #IPL_SEG            ;; Go to the segment where the IPL table lives
  mov  ds, bx
  mov  bx, IPL_COUNT_OFFSET    ;; Read the number of entries so far
  cmp  bx, #IPL_TABLE_ENTRIES
  je   no_bev                  ;; Get out if the table is full
  push cx
  mov  cx, #0x4                ;; Zet: Needed to be compatible with 8086
  shl  bx, cl                  ;; Turn count into offset (entries are 16 bytes)
  pop  cx
  mov  0[bx], #IPL_TYPE_BEV    ;; This entry is a BEV device
  mov  6[bx], cx               ;; Build a far pointer from the segment...
  mov  4[bx], ax               ;; and the offset
  cmp  di, #0x0000
  je   no_prod_str
  mov  0xA[bx], cx             ;; Build a far pointer from the segment...
  mov  8[bx], di               ;; and the offset
no_prod_str:
  push cx
  mov  cx, #0x4
  shr  bx, cl                  ;; Turn the offset back into a count
  pop  cx
  inc  bx                      ;; We have one more entry now
  mov  IPL_COUNT_OFFSET, bx    ;; Remember that.

no_bev:
  pop  di       ;; Restore DI
  pop  ax       ;; Restore AX
rom_scan_increment:
  push cx
  mov  cx, #5
  shl  ax, cl   ;; convert 512-bytes blocks to 16-byte increments
                ;; because the segment selector is shifted left 4 bits.
  pop  cx
  add  cx, ax
  pop  ax       ;; Restore AX
  cmp  cx, ax
  jbe  rom_scan_loop

  xor  ax, ax   ;; Restore DS back to 0000:
  mov  ds, ax
  ret

;; for 'C' strings and other data, insert them here with
;; a the following hack:
;; DATA_SEG_DEFS_HERE


;; the following area can be used to write dynamically generated tables
  .align 16
bios_table_area_start:
  dd 0xaafb4442
  dd bios_table_area_end - bios_table_area_start - 8;

;--------
;- POST -
;--------
.org 0xe05b ; POST Entry Point
post:

  xor ax, ax

normal_post:
  ; case 0: normal startup

  cli
  mov  ax, #0xfffe
  mov  sp, ax
  xor  ax, ax
  mov  ds, ax
  mov  ss, ax

  ;; zero out BIOS data area (40:00..40:ff)
  mov  es, ax
  mov  cx, #0x0080 ;; 128 words
  mov  di, #0x0400
  cld
  rep
    stosw

  ;; set all interrupts to default handler
  xor  bx, bx         ;; offset index
  mov  cx, #0x0100    ;; counter (256 interrupts)
  mov  ax, #dummy_iret_handler
  mov  dx, #0xF000

post_default_ints:
  mov  [bx], ax
  add  bx, #2
  mov  [bx], dx
  add  bx, #2
  loop post_default_ints

  ;; set vector 0x79 to zero
  ;; this is used by 'gardian angel' protection system
  SET_INT_VECTOR(0x79, #0, #0)

  ;; base memory in K 40:13 (word)
  mov  ax, #BASE_MEM_IN_K
  mov  0x0413, ax


  ;; Manufacturing Test 40:12
  ;;   zerod out above

  ;; Warm Boot Flag 0040:0072
  ;;   value of 1234h = skip memory checks
  ;;   zerod out above

  ;; Bootstrap failure vector
  SET_INT_VECTOR(0x18, #0xF000, #int18_handler)

  ;; Bootstrap Loader vector
  SET_INT_VECTOR(0x19, #0xF000, #int19_handler)

  ;; EBDA setup
  call ebda_post

  ;; Video setup
  SET_INT_VECTOR(0x10, #0xF000, #int10_handler)

  mov  cx, #0xc000  ;; init vga bios
  mov  ax, #0xc780
  call rom_scan

  call _print_bios_banner

  call _init_boot_vectors

  mov  cx, #0xc800  ;; init option roms
  mov  ax, #0xe000
  call rom_scan

  sti        ;; enable interrupts
  int  #0x19

;----------
;- INT19h -
;----------
.org 0xe6f2 ; INT 19h Boot Load Service Entry Point
int19_handler:

  jmp int19_relocated

.org 0xf045 ; INT 10 Functions 0-Fh Entry Point
  ;; HALT(__LINE__)
  iret

;----------
;- INT10h -
;----------
.org 0xf065 ; INT 10h Video Support Service Entry Point
int10_handler:
  ;; dont do anything, since the VGA BIOS handles int10h requests
  iret

.org 0xf0a4 ; MDA/CGA Video Parameter Table (INT 1Dh)

;------------------------------------------------
;- IRET Instruction for Dummy Interrupt Handler -
;------------------------------------------------
.org 0xff53 ; IRET Instruction for Dummy Interrupt Handler
dummy_iret_handler:
  iret

.org 0xfff0 ; Power-up Entry Point
  jmp 0xf000:post

.org 0xfff5 ; ASCII Date ROM was built - 8 characters in MM/DD/YY
.ascii BIOS_BUILD_DATE

.org 0xfffe ; System Model ID
db SYS_MODEL_ID
db 0x00   ; filler
ASM_END

ASM_START
.org 0xcc00
bios_table_area_end:
// bcc-generated data will be placed here
ASM_END

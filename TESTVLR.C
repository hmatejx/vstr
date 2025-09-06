/* TESTVLR.C - Borland C++ 3.1 / DOS
   CT65550 FP vertical replication live tester (hotkeys), with classic heights:
     200, 350, 400, 480, 600 lines

   Modes:
     [1] 320x200x256 (VGA  0x13)  -> 200
     [2] 640x350x16  (EGA  0x10)  -> 350  (planar fill)
     [3] 640x400x256 (VESA 0x100) -> 400
     [4] 640x480x256 (VESA 0x101) -> 480
     [5] 800x600x256 (VESA 0x103) -> 600

   Pattern: even/odd scanlines + 20px white tick every 10th line
   Graphics text via BIOS teletype; DAC 0,1,2,15 initialized (helps VESA paths)

   FR programming order (proven on Tecra 800×600):
     FR4D  (VLR low..high)
     FR4E = 0x3F  (bit=1 ENABLES stretching for VDE classes)
     FR48 = 0x17  (EVCP|EVLR +…; 'e' toggles EVCP/LR)
     FR41 = 0x07  (Horizontal stretching register)
     FR40 = 0x3F  (Horizontal compensation; final latch)
     FR01 bit1 |= 1 (FP path). vblank wait before applying.

   Hotkeys:
     + / -   set FIXED N up/down  (FR4D = N|N)
     l / L   low nibble -- / ++   (0..high)
     h / H   high nibble -- / ++  (low..F)
     e       toggle FR48 EVCP/LR, then re-apply current FR4D
     w       write arbitrary FRxx (index,value) in hex; then dump
     d       dump FR40..FR4F
     g       restore saved originals
     q       quit (restore & text)

   Build: Large model.
*/

#include <dos.h>
#include <conio.h>
#include <stdio.h>
#include <mem.h>

#define PORT_IDX  0x3D0
#define PORT_DAT  0x3D1
#define VGA_DAC_WRITE 0x3C8
#define VGA_DAC_DATA  0x3C9

/* ---------- FR I/O ---------- */
static unsigned char rr(unsigned char idx){ outp(PORT_IDX, idx); return inp(PORT_DAT); }
static void wr(unsigned char idx, unsigned char val){ outp(PORT_IDX, idx); outp(PORT_DAT, val); }

/* ---------- BIOS teletype ---------- */
static void bios_putc(char ch){ union REGS r; r.h.ah=0x0E; r.h.al=(unsigned char)ch; r.h.bh=0; r.h.bl=0x0F; int86(0x10,&r,&r); }
static void bios_puts(const char* s){ while(*s) bios_putc(*s++); }
static void bios_crlf(void){ bios_putc('\r'); bios_putc('\n'); }

/* ---------- palette (helps VESA modes) ---------- */
static void set_palette_rgb(unsigned char idx, unsigned char r, unsigned char g, unsigned char b){
    outp(VGA_DAC_WRITE, idx); outp(VGA_DAC_DATA, r); outp(VGA_DAC_DATA, g); outp(VGA_DAC_DATA, b);
}
static void init_basic_palette(void){
    set_palette_rgb(0,  0,  0,  0);   /* black */
    set_palette_rgb(1,  0,  0, 63);   /* blue  */
    set_palette_rgb(2,  0, 63,  0);   /* green */
    set_palette_rgb(15,63, 63, 63);   /* white */
}

/* ---------- VBE structs/helpers (for 0x100/0x101/0x103) ---------- */
#pragma pack(1)
typedef struct {
    unsigned short  ModeAttributes;
    unsigned char   WinAAttributes, WinBAttributes;
    unsigned short  WinGranularity, WinSize;
    unsigned short  WinASegment, WinBSegment;
    unsigned long   WinFuncPtr;
    unsigned short  BytesPerScanLine;
    unsigned short  XResolution, YResolution;
    unsigned char   XCharSize, YCharSize, NumberOfPlanes, BitsPerPixel;
    unsigned char   NumberOfBanks, MemoryModel, BankSize, NumberOfImagePages, Reserved1;
    unsigned char   RedMaskSize, RedFieldPosition, GreenMaskSize, GreenFieldPosition;
    unsigned char   BlueMaskSize, BlueFieldPosition, RsvdMaskSize, RsvdFieldPosition, DirectColorModeInfo;
    unsigned long   PhysBasePtr, OffScreenMemOffset;
    unsigned short  OffScreenMemSize;
    unsigned char   Reserved2[206];
} VbeModeInfoBlock;
#pragma pack()

static int vbe_set_mode(unsigned short mode){ union REGS r; r.x.ax=0x4F02; r.x.bx=mode; int86(0x10,&r,&r); return (r.x.ax==0x004F); }
static int vbe_get_mode_info(unsigned short mode, VbeModeInfoBlock far* p){
    union REGS r; struct SREGS s; segread(&s); s.es=FP_SEG(p);
    r.x.ax=0x4F01; r.x.cx=mode; r.x.di=FP_OFF(p); int86x(0x10,&r,&r,&s); return (r.x.ax==0x004F);
}
static int vbe_set_bank_A(unsigned short bank){ union REGS r; r.x.ax=0x4F05; r.h.bh=0; r.h.bl=0; r.x.dx=bank; int86(0x10,&r,&r); return (r.x.ax==0x004F); }

/* ---------- banked VRAM writer (VESA) ---------- */
static void vram_write_block_vesa(const VbeModeInfoBlock far* mi, unsigned long addr, const void far* src, unsigned long len){
    unsigned long gran_bytes=(unsigned long)mi->WinGranularity*1024UL;
    unsigned long win_size  =(unsigned long)mi->WinSize      *1024UL;
    unsigned short seg=mi->WinASegment;
    const unsigned char far* ps=(const unsigned char far*)src;
    while(len>0UL){
        unsigned long bank=addr/gran_bytes, offset=addr-bank*gran_bytes, room=win_size-offset;
        unsigned long chunk=(len<room)?len:room;
        vbe_set_bank_A((unsigned short)bank);
        { unsigned char far* dst=(unsigned char far*)MK_FP(seg,(unsigned)offset); _fmemcpy(dst, ps, (unsigned)chunk); }
        addr+=chunk; ps+=chunk; len-=chunk;
    }
}
static void fill_line_vesa(const VbeModeInfoBlock far* mi, unsigned short y, unsigned short width, unsigned char color){
    static unsigned char buf[1024]; /* big enough for width<=1024 */
    unsigned long addr=(unsigned long)y*(unsigned long)mi->BytesPerScanLine;
    _fmemset(buf, color, width); vram_write_block_vesa(mi, addr, buf, width);
}
static void tick_line_vesa(const VbeModeInfoBlock far* mi, unsigned short y, unsigned short width){
    unsigned char buf[20]; unsigned short i, tw=(width>=20)?20:width;
    for(i=0;i<tw;++i) buf[i]=15;
    vram_write_block_vesa(mi, (unsigned long)y*(unsigned long)mi->BytesPerScanLine, buf, tw);
}
static void draw_pattern_vesa(const VbeModeInfoBlock far* mi){
    unsigned short width=mi->XResolution, height=mi->YResolution, y;
    for(y=0;y<height;++y){ unsigned char c=(y&1)?2:1; fill_line_vesa(mi,y,width,c); if((y%10)==0) tick_line_vesa(mi,y,width); }
}

/* ---------- VGA 0x13 linear ---------- */
static void fill_line_13h(unsigned short y, unsigned char color){
    unsigned char far* vram=(unsigned char far*)MK_FP(0xA000,0x0000);
    _fmemset(vram + (unsigned long)y*320UL, color, 320);
}
static void tick_line_13h(unsigned short y){
    unsigned char far* vram=(unsigned char far*)MK_FP(0xA000,0x0000); unsigned short x;
    for(x=0;x<20;++x) vram[(unsigned long)y*320UL + x]=15;
}
static void draw_pattern_13h(void){
    unsigned short y; for(y=0;y<200;++y){ unsigned char c=(y&1)?2:1; fill_line_13h(y,c); if((y%10)==0) tick_line_13h(y); }
}

/* ---------- EGA 0x10 (640x350x16) planar line fill ---------- */
#define SEQ_INDEX  0x3C4
#define SEQ_DATA   0x3C5
#define GC_INDEX   0x3CE
#define GC_DATA    0x3CF
static void ega_set_map_mask(unsigned char mask){ outp(SEQ_INDEX, 0x02); outp(SEQ_DATA, mask); }
static void gc_write(unsigned char idx, unsigned char val){ outp(GC_INDEX, idx); outp(GC_DATA, val); }
static void ega_planar_prep(void){
    ega_set_map_mask(0x0F);
    gc_write(0x01, 0x0F);   /* Enable Set/Reset */
    gc_write(0x03, 0x00);   /* Data Rotate = 0 */
    gc_write(0x08, 0xFF);   /* Bit Mask = FF */
    gc_write(0x05, 0x00);   /* Write Mode 0 */
}
static void ega_fill_line_350(unsigned short y, unsigned char color){
    unsigned char far* vram=(unsigned char far*)MK_FP(0xA000,0x0000);
    unsigned short i; unsigned long off=(unsigned long)y*80UL; /* 640/8=80 bytes */
    gc_write(0x00, (unsigned char)(color & 0x0F)); /* Set/Reset = color */
    for(i=0;i<80;++i) vram[off + i] = 0xFF;
}
static void ega_tick_line_350(unsigned short y){
    unsigned char far* vram=(unsigned char far*)MK_FP(0xA000,0x0000);
    unsigned long off=(unsigned long)y*80UL;
    gc_write(0x00, 15);  /* white */
    vram[off + 0] = 0xFF; vram[off + 1] = 0xFF; vram[off + 2] = 0x0F;
}
static void draw_pattern_ega10h(void){
    unsigned short y; ega_planar_prep();
    for(y=0;y<350;++y){
        unsigned char c=(y&1)?2:1; ega_fill_line_350(y, c);
        if((y%10)==0) ega_tick_line_350(y);
    }
}

/* ---------- saves & helpers ---------- */
static unsigned char fr01_orig, fr4d_orig, fr4e_orig, fr48_orig, fr41_orig, fr40_orig;
static void wait_vblank(void){ while (inp(0x3DA) & 0x08) {;} while ((inp(0x3DA) & 0x08) == 0) {;} }

/* hex helpers */
static int hex_nibble(int ch){
    if(ch>='0'&&ch<='9') return ch-'0';
    if(ch>='a'&&ch<='f') return ch-'a'+10;
    if(ch>='A'&&ch<='F') return ch-'A'+10;
    return -1;
}
static unsigned char prompt_hex_byte(const char* label){
    int c1, c2, n1, n2; bios_puts(label); bios_puts(" (00-FF): ");
    for(;;){
        c1=getch(); bios_putc((char)c1); n1=hex_nibble(c1); if(n1<0){ bios_puts("\r\n! hex\r\n"); continue; }
        c2=getch(); bios_putc((char)c2); n2=hex_nibble(c2); if(n2<0){ bios_puts("\r\n! hex\r\n"); continue; }
        bios_crlf(); return (unsigned char)((n1<<4)|n2);
    }
}

/* dump FR40..FR4F */
static void dump_fr_block(void){
    unsigned char i, v; char buf[16]; bios_puts("FR40..4F:");
    for(i=0x40;i<=0x4F;i++){ v=rr(i); sprintf(buf," %02X", v); bios_puts(buf); }
    bios_crlf();
}

/* apply current low/high with proven sequence */
static unsigned char cur_lo = 0, cur_hi = 0;        /* start with FR4D=0x00 */
static unsigned char cur_fr48 = 0x17;               /* EVCP|EVLR set */
static void apply_vrep_range(unsigned char lo, unsigned char hi){
    if(lo>hi) lo=hi;
    wait_vblank();
    wr(0x4D, (unsigned char)((hi<<4) | (lo & 0x0F)));   /* FR4D */
    wr(0x4E, 0x3F);                                     /* FR4E: enable all */
    wr(0x48, cur_fr48);                                 /* FR48: EVCP/EVLR etc */
    wr(0x41, 0x07);                                     /* FR41 */
    wr(0x40, 0x3F);                                     /* FR40 latch */
}
static void apply_vrep_fixed(unsigned char n){ apply_vrep_range(n,n); }
static void show_regs_line(const char* hdr){
    char buf[96];
    bios_puts(hdr);
    sprintf(buf,"FR4D=%02X FR4E=%02X FR48=%02X FR41=%02X FR40=%02X",
            rr(0x4D), rr(0x4E), rr(0x48), rr(0x41), rr(0x40));
    bios_puts(buf); bios_crlf();
}
static void restore_regs(void){
    wr(0x40, fr40_orig); wr(0x41, fr41_orig); wr(0x48, fr48_orig);
    wr(0x4E, fr4e_orig); wr(0x4D, fr4d_orig); wr(0x01, fr01_orig);
}

/* ---------- mode helpers ---------- */
static void set_text_mode(void){ union REGS r; r.h.ah=0x00; r.h.al=0x03; int86(0x10,&r,&r); }
static int  set_mode_13h(void){ union REGS r; r.h.ah=0x00; r.h.al=0x13; int86(0x10,&r,&r); return 1; }
static int  set_mode_ega10h(void){ union REGS r; r.h.ah=0x00; r.h.al=0x10; int86(0x10,&r,&r); return 1; }

/* ---------- main ---------- */
int main(void)
{
    char modestr[12];
    cprintf("\r\nCT65550 V-Replication Live Tester\r\n"
	    "[1] 320x200x256 (VGA  0x13)\r\n"
	    "[2] 640x350x16  (EGA  0x10)\r\n"
	    "[3] 640x400x256 (VESA 0x100)\r\n"
	    "[4] 640x480x256 (VESA 0x101)\r\n"
	    "[5] 800x600x256 (VESA 0x103)\r\n"
	    "Select mode: ");
    {
	int sel=getch(); cprintf("%c\r\n", sel);

	if(sel=='1'){
	    sprintf(modestr, "320x200");
	    if(!set_mode_13h()){ cprintf("Mode 0x13 failed.\r\n"); return 1; }
	    init_basic_palette(); draw_pattern_13h();
	} else if(sel=='2'){
	    sprintf(modestr, "640x350");
	    if(!set_mode_ega10h()){ cprintf("Mode 0x10 failed.\r\n"); return 1; }
	    draw_pattern_ega10h();
	} else if(sel=='3'){
	    sprintf(modestr, "640x400");
	    {
	    VbeModeInfoBlock far* mi=(VbeModeInfoBlock far*)MK_FP(0x9000,0x0000);
	    if(!vbe_get_mode_info(0x100, mi)){ cprintf("VBE 0x4F01 failed.\r\n"); return 1; }
	    if(!vbe_set_mode(0x100)){ cprintf("VBE 0x4F02 failed.\r\n"); return 1; }
	    init_basic_palette(); draw_pattern_vesa(mi); }
	} else if(sel=='4'){
	    sprintf(modestr, "640x480");
	    {
	    VbeModeInfoBlock far* mi=(VbeModeInfoBlock far*)MK_FP(0x9000,0x0000);
	    if(!vbe_get_mode_info(0x101, mi)){ cprintf("VBE 0x4F01 failed.\r\n"); return 1; }
	    if(!vbe_set_mode(0x101)){ cprintf("VBE 0x4F02 failed.\r\n"); return 1; }
	    init_basic_palette(); draw_pattern_vesa(mi); }
	} else if(sel=='5'){
	    sprintf(modestr, "800x600");
	    {
	    VbeModeInfoBlock far* mi=(VbeModeInfoBlock far*)MK_FP(0x9000,0x0000);
	    if(!vbe_get_mode_info(0x103, mi)){ cprintf("VBE 0x4F01 failed.\r\n"); return 1; }
	    if(!vbe_set_mode(0x103)){ cprintf("VBE 0x4F02 failed.\r\n"); return 1; }
	    init_basic_palette(); draw_pattern_vesa(mi); }
	} else {
	    cprintf("Invalid selection.\r\n"); return 1;
	}
    }

    /* Save originals & enable FP path */
    fr01_orig = rr(0x01);
    fr4d_orig = rr(0x4D);
    fr4e_orig = rr(0x4E);
    fr48_orig = rr(0x48);
    fr41_orig = rr(0x41);
    fr40_orig = rr(0x40);
    wr(0x01, (unsigned char)(fr01_orig | 0x02)); /* FP path on */

    /* Apply current (from FR4D) once to ensure proper latch values */
    apply_vrep_range(cur_lo, cur_hi);

    /* Help */
    bios_puts(modestr);
    bios_puts(" KEYS: ^/v FR4D   Q QUIT   X EXIT\r\n");

    for(;;){
	int k=getch();
	int spec=0;
	if(k==0 || k==0xE0) { k = getch(); spec=1; } //handle special keys
	if(k=='+' || (spec && k==72)) {
	    unsigned char n=(rr(0x4D)&0x0F); if(n<15) n++;
	    cur_lo = cur_hi = n; apply_vrep_fixed(n); show_regs_line("");
	} else if(k=='-' || (spec && k==80)) {
	    unsigned char n=(rr(0x4D)&0x0F); if(n>0) n--;
	    cur_lo = cur_hi = n; apply_vrep_fixed(n); show_regs_line("");
	} else if(k=='e'||k=='E'){
	    unsigned char v=rr(0x48); v ^= 0x05; cur_fr48=v; /* toggle EVCP/LR */
	    apply_vrep_range(cur_lo, cur_hi); show_regs_line("FR48 toggled ");
	} else if(k=='q'||k=='Q'){
	    restore_regs(); set_text_mode(); cprintf("Done.\r\n"); return 0;
	} else if(k=='x'||k=='X'){
	    set_text_mode(); cprintf("Done.\r\n"); return 0;
	}
    }
}

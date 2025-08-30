/*  VSTR.CPP - CT65555 scaler (DOS/BC++ 3.1) by github.com/hmatejx
 *
 *  Build:
 *      bcc -mt -Os vstr.cpp
 *
 *  Usage:
 *      VSTR /ON  [/P:800|/P:1024]
 *      VSTR /OFF
 *      VSTR /DUMP
 *      VSTR /SAVE
 *      VSTR /DIFF
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dos.h>

typedef unsigned char  u8;
typedef unsigned short u16;

// Ports
#define FR_INDEX   0x3D0
#define FR_DATA    0x3D1
#define XR_INDEX   0x3D6
#define XR_DATA    0x3D7
#define ST01       0x3DA
#define CRTC_IDX   0x3D4
#define CRTC_DAT   0x3D5

// Relevant FR registers we touch/inspect
#define FR40_HCOMP       0x40
#define FR41_HSTR        0x41
#define FR48_VCTRL       0x48
#define FR4D_VREPL       0x4D
#define FR4E_VSELDSBL    0x4E
#define FR49_TXTREP0     0x49
#define FR4A_TXTREP1     0x4A
#define FR4B_TXTREP2     0x4B
#define FR4C_TXTREP3     0x4C
// Panel timing window registers
#define FR20_PAN_HS      0x20
#define FR37_PAN_VTOT    0x37

// Register snapshot file (binary)
#define SNAPFILE "VSTR.SAV"

// I/O helpers
static void wr_fr(u8 idx, u8 val){ outp(FR_INDEX, idx); outp(FR_DATA, val); }
static u8   rd_fr(u8 idx)        { outp(FR_INDEX, idx); return inp(FR_DATA); }
static void wr_crtc(u8 idx,u8 v) { outp(CRTC_IDX, idx); outp(CRTC_DAT, v); }
static u8   rd_crtc(u8 idx)      { outp(CRTC_IDX, idx); return inp(CRTC_DAT); }

static void wait_vblank(void) {
    while (inp(ST01) & 0x08) {;}
    while ((inp(ST01) & 0x08) == 0) {;}
}

// Structure to hold register values
typedef struct {
    u8 fr40, fr41, fr48, fr4d, fr4e, fr49, fr4a, fr4b, fr4c;
    u16 sig;
} StretchRegs;

// Register Snapshot read/save/apply helpers
static void snapshot_read(StretchRegs* s) {
    s->fr40 = rd_fr(FR40_HCOMP);
    s->fr41 = rd_fr(FR41_HSTR);
    s->fr48 = rd_fr(FR48_VCTRL);
    s->fr4d = rd_fr(FR4D_VREPL);
    s->fr4e = rd_fr(FR4E_VSELDSBL);
    s->fr49 = rd_fr(FR49_TXTREP0);
    s->fr4a = rd_fr(FR4A_TXTREP1);
    s->fr4b = rd_fr(FR4B_TXTREP2);
    s->fr4c = rd_fr(FR4C_TXTREP3);
    s->sig  = 0x6555; // signature for checking
}
static int snapshot_write_file(const StretchRegs* s) {
    FILE* f = fopen(SNAPFILE,"wb");
    if (!f) return 0;

    fwrite(s,sizeof(*s),1,f);
    fclose(f);
    return 1;
}
static int snapshot_read_file(StretchRegs* s) {
    FILE* f = fopen(SNAPFILE,"rb");
    if (!f) return 0;

    fread(s,sizeof(*s),1,f);
    fclose(f);
    return (s->sig == 0x6555); // check for valid signature
}
static void snapshot_apply(const StretchRegs* s) {
    wait_vblank();
    wr_fr(FR49_TXTREP0, s->fr49);
    wr_fr(FR4A_TXTREP1, s->fr4a);
    wr_fr(FR4B_TXTREP2, s->fr4b);
    wr_fr(FR4C_TXTREP3, s->fr4c);
    wr_fr(FR4D_VREPL,   s->fr4d);
    wr_fr(FR4E_VSELDSBL,s->fr4e);
    wr_fr(FR48_VCTRL,   s->fr48);
    wr_fr(FR41_HSTR,    s->fr41);
    wr_fr(FR40_HCOMP,   s->fr40);
}

// Presets
#define FR48_EVCP  0x01
#define FR48_EVLR  0x04
#define FR48_ETVS  0x10
#define FR4E_STRETCH_ALL 0x00
static const StretchRegs PRESET_800 = {
    0x3F,                // FR40_HCOMP
    0x07,                // FR41_HSTR
    0x17,                // FR48_VCTRL
    0x40,                // FR4D_VREPL
    0x3F,                // FR4E_VSELDSBL, 3C for text stretch only
    0x80,0x03,0x80,0x01  // TXTREP0..4
};
static const StretchRegs PRESET_1024 = {
    0x3F,                // FR40_HCOMP
    0x07,                // FR41_HSTR
    0x17,                // FR48_VCTRL
    0x50,                // FR4D_VREPL
    0x3F,                // FR4E_VSELDSBL, 3C for text stretch only
    0x80,0x03,0x80,0x01  // TXTREP0..4
};
// Preset apply helper
static void apply_preset(const StretchRegs* p){
    wait_vblank();
    // Crucial to set registers in exactly this order
    wr_fr(FR49_TXTREP0,  p->fr49);
    wr_fr(FR4A_TXTREP1,  p->fr4a);
    wr_fr(FR4B_TXTREP2,  p->fr4b);
    wr_fr(FR4C_TXTREP3,  p->fr4c);
    wr_fr(FR4D_VREPL,    p->fr4d);
    wr_fr(FR4E_VSELDSBL, p->fr4e);
    wr_fr(FR48_VCTRL,    p->fr48);
    wr_fr(FR41_HSTR,     p->fr41);
    wr_fr(FR40_HCOMP,    p->fr40);
}

// Debug helpers
static void print_mode_and_crtc(FILE* out){
    union REGS r;
    // BIOS video mode
    r.h.ah = 0x0F;
    int86(0x10, &r, &r);
    fprintf(out,"DOS Video mode: 0x%02X, Page: %u\n", r.h.al, r.h.bh);

    // CRTC vertical display end (low 8 bits in CR12, high bits in CR07)
    // CR07 bit1 is VDE bit8 in VGA text/graphics; other extended bits may exist on CT chips.
    // This is a rough peek just to classify 200/350/400/480-ish.
    u8 cr12 = rd_crtc(0x12);
    u8 cr07 = rd_crtc(0x07);
    int vde = cr12 | ((cr07 & 0x02) ? 0x100 : 0x000);
    fprintf(out,"CRTC VDE (approx): %d lines\n", vde+1);
}

static void dump_fr_block(FILE* out) {
    u8 fr;
    fprintf(out, "\n[FR4x Stretch Block]\n");
    fr = rd_fr(FR48_VCTRL);
    fprintf(out, "FR48\t  VCTRL   = %02X  (EVCP=%d, EVLR=%d, ETVS=%d)\n",
	fr, (fr&0x01)!=0, (fr&0x04)!=0, (fr&0x10)!=0);
    fprintf(out, "FR4D\t  VREPL   = %02X\n", rd_fr(FR4D_VREPL));
    fprintf(out, "FR4E\t  VSELDSB = %02X  (0->stretch allowed)\n", rd_fr(FR4E_VSELDSBL));
    fprintf(out, "FR41\t  HSTR    = %02X\n", rd_fr(FR41_HSTR));
    fprintf(out, "FR40\t  HCOMP   = %02X\n", rd_fr(FR40_HCOMP));
    fprintf(out, "FR49..4C  TEXT    = %02X %02X %02X %02X\n",
	rd_fr(FR49_TXTREP0), rd_fr(FR4A_TXTREP1), rd_fr(FR4B_TXTREP2), rd_fr(FR4C_TXTREP3));
}

static void dump_fr_panel_timing(FILE* out){
    int i;
    fprintf(out, "\n[FR20..FR37 Panel Timing (read-only sanity)]\n");
    for (i = FR20_PAN_HS; i <= FR37_PAN_VTOT; ++i){
	if (((i - FR20_PAN_HS) % 8) == 0) fprintf(out, "%02X: ", i);
	fprintf(out,"%02X ", rd_fr((u8)i));
	if (((i - FR20_PAN_HS) % 8) == 7) fprintf(out, "\n");
    }
}

static void dump_all(FILE* out) {
    fprintf(out,"=== VSTR /DUMP ===\n");
    print_mode_and_crtc(out);
    dump_fr_block(out);
    dump_fr_panel_timing(out);
    fprintf(out,"======================\n");
}

static void save_snapshot() {
    StretchRegs s; snapshot_read(&s);
    if (!snapshot_write_file(&s))
	printf("Warning: couldn't write snapshot '%s'.\n", SNAPFILE);
}

static void diff_against_snapshot(FILE* out) {
    StretchRegs s;
    if (!snapshot_read_file(&s)) {
	fprintf(out,"No '%s' snapshot found.\n", SNAPFILE);
	return;
    }
    fprintf(out,"=== VSTR /DIFF vs %s ===\n", SNAPFILE);

    #define SHOW(name,idx,val) \
	do{ u8 cur=rd_fr(idx); fprintf(out,"%-12s: cur=%02X  snap=%02X  %s\n", name, cur, val, (cur==val) ? "==" : "**"); }while(0)

    SHOW("FR40 HCOMP", FR40_HCOMP,    s.fr40);
    SHOW("FR41 HSTR",  FR41_HSTR,     s.fr41);
    SHOW("FR48 VCTRL", FR48_VCTRL,    s.fr48);
    SHOW("FR4D VREPL", FR4D_VREPL,    s.fr4d);
    SHOW("FR4E VSEL",  FR4E_VSELDSBL, s.fr4e);
    SHOW("FR49 T0",    FR49_TXTREP0,  s.fr49);
    SHOW("FR4A T1",    FR4A_TXTREP1,  s.fr4a);
    SHOW("FR4B T2",    FR4B_TXTREP2,  s.fr4b);
    SHOW("FR4C T3",    FR4C_TXTREP3,  s.fr4c);
    #undef SHOW
    fprintf(out,"( '**' marks differences )\n");
}

// Command line parameter parsing helper
static int ieq(const char* a, const char* b){
    while (*a && *b){
	char ca = (*a >= 'a' && *a <= 'z') ? *a - 32 : *a;
	char cb = (*b >= 'a' && *b <= 'z') ? *b - 32 : *b;
	if (ca != cb) return 0;
	++a; ++b;
    }
    return (*a == 0 && *b == 0);
}


int main(int argc, char** argv) {
    int do_on=0, do_off=0, do_dump=0, do_diff=0, do_save=0;
    const StretchRegs* preset = &PRESET_800; // default preset

    for (int i = 1; i < argc; ++i) {
	if (argv[i][0] == '/' || argv[i][0] == '-') {
	    const char* sw = argv[i] + 1;
	    if (ieq(sw, "ON"))   { do_on = 1; continue; }
	    if (ieq(sw, "OFF"))  { do_off = 1; continue; }
	    if (ieq(sw, "DUMP")) { do_dump = 1; continue; }
	    if (ieq(sw, "SAVE")) { do_save = 1; continue; }
	    if (ieq(sw, "DIFF")) { do_diff = 1; continue; }
	    if (sw[0] == 'P' || sw[0] == 'p') {
		const char* v = strchr(sw, ':'); if (v) {
		    ++v;
		    if (ieq(v, "800")) preset = &PRESET_800;
		    else if (ieq(v, "1024")) preset = &PRESET_1024;
		    else { printf("Unknown panel preset: %s\n", v); return 1; }
		} continue;
	    }
	}
	printf("Unknown switch: %s\n", argv[i]);
	return 1;
    }

    if (!(do_on || do_off || do_dump || do_diff || do_save)) {
	printf("VSTR - CT65555 scaler by github.com/hmatejx\n");
	printf("Usage:\n");
	printf("  VSTR /ON  [/P:800|/P:1024]\n");
	printf("  VSTR /OFF\n");
	printf("  VSTR /DUMP\n");
	printf("  VSTR /SAVE\n");
	printf("  VSTR /DIFF\n");
	return 0;
    }

    if (do_on) {
	apply_preset(preset);
	printf("Scaling enabled (preset %s).\n",
	       (preset == &PRESET_800) ? "800x600" : "1024x768");
    }

    if (do_save) {
	save_snapshot();
    }

    if (do_off) {
	StretchRegs s;
	if (!snapshot_read_file(&s)) {
	    printf("No valid snapshot to restore.\n");
	    return 1;
	}
	snapshot_apply(&s);
	printf("Scaling restored to previous values.\n");
    }

    if (do_dump) {
	dump_all(stdout);
    }

    if (do_diff) {
	diff_against_snapshot(stdout);
    }

    return 0;
}

# VSTR - Video Stretch Utility for Chips & Technologies CT6555x

VSTR is a small DOS command-line utility to enable or disable LCD panel scaling  (horizontal and vertical stretching) on laptops using the C&T CT6555x graphics chipsets (CT65550, CT65554, CT65555). 

It is designed as a functional replacement and extension of the older [VEXP](https://archive.org/details/vexp13_1997) utility, but adds support for additional preset for 1024×768 panels.

Note: Some OEM documentation and chip markings use the **F** prefix or suffix (e.g. CT F6555x). These are equivalent to CT 6555x. The "F" was a vendor/marketing notation added sometime after the initial introduction of these chips denoting flat panel support.

---

## Background

Many Toshiba and similar laptops of the mid-1990s used flat-panel LCDs with native  resolutions such as 800×600 or 1024×768. In standard DOS modes (320×200, 640×480, etc.),  the BIOS usually doubles pixels but **does not stretch the image** to fill the panel, 
leaving a centered picture with black borders. While BIOS typically contains settings that enable text mode stretching, the graphics modes, such as those those used by games, are usually not stretched by default.

The CT6555x family includes built-in hardware scalers that can stretch these modes to the full panel. The scaler is controlled through a set of **extended registers**. 

By toggling these registers, `VSTR` allows you to enable or disable stretching at will.

### Best possible expected result

**1024x768**

![1024x768_output](/img/1024x768_output.png)

**800x600**

![800x600_output](/img/800x600_output.png)

---

## Registers Used

The CT6555x chip exposes a set of **extended registers** (via ports 0x3D6/0x3D7) that control the LCD panel’s stretch logic:

- **FR40 – Horizontal Compensation Control**  
  Enables horizontal stretching in graphics modes and defines how 8-dot and 
  9-dot text characters are handled when the panel width differs from the 
  character cell width.

- **FR41 – Horizontal Stretch Enable**  
  Bitmask controlling which source widths (320/640, 400/800, 512/1024) 
  are eligible for horizontal stretching.

- **FR48 – Vertical Compensation Control**  
  Governs vertical scaling behavior. Contains enables for:  
  - Bit 4: Enable text mode vertical stretching (ETVS)  
  - Bit 3: Select priority between text stretching (EVTS) vs. line replication (EVLR)
  - Bit 2: Enable vertical line replication (EVLR)  
  - Bit 1: Enable vertical centering  
  - Bit 0: Enable vertical compensation (EVCP)  

- **FR4D – Vertical Replication Register**  
  Stretch distribution control register - it gives the scaler a range of line spacing
  to use while it spreads replicated lines across the frame.

- **FR4E – Vertical Stretch Enable Mask**  
  Bitmask controlling which vertical source classes (200/350/400/480 lines) 
  are eligible for vertical stretching.

Note: Registers FR49–FR4C control *text mode vertical replication*. These are not modified by VSTR. On Toshiba laptops the BIOS already programs sensible defaults for text scaling, and changing them can distort characters. VSTR intentionally avoids them.

For detailed descriptions, refer to the official chipset documentation:  
- [CT65550 Datasheet (PDF)](http://bitsavers.informatik.uni-stuttgart.de/components/chipsAndTech/CHIPS_65550_199710.pdf)
- [CT65554 Datasheet (PDF)](https://www.vgamuseum.info/index.php/cpu/item/download/275_f6c6c4863a2c754f97ba1e7d628725f8)
- [CT65555 Datasheet (PDF)](https://www.dosdays.co.uk/media/c_and_t/ct65555_datasheet.pdf)


---

## Build Instructions

Prerequisites: **Borland C++ 3.1** (or Turbo C++ 3.x), memory model: **Tiny**

```bat
bcc -mt -Os -f- -N- -G- vstr.cpp
```

or run the provided `MAKE.BAT` file.

## Usage

```
VSTR [options]
```

### Options

- **`/ON`**  
  Enable stretching using either the current panel preset or the last selected one. Does *not* save original register state unless /SAVE is specified.
  
- **`/OFF`**  
  Disable stretching and restore original register values if a save exists.

- **`/P:800`**  
  Apply preset for an 800×600 panel (e.g. Toshiba Tecra 710CDT).

- **`/P:1024`**  
  Apply preset for a 1024×768 panel (later Tecra/Satellite/Libretto and similar).

- **`/SAVE`**  
  Save the current register state into `VSTR.SAV` file.

- **`/DIFF`**  
  Show a side-by-side dump of current register values compared to saved values.

---

## Examples

Save BIOS values after clean boot:

```
VSTR /SAVE
```

Enable stretching on an 800×600 panel:

```
VSTR /P:800 /ON
```

Enable stretching on a 1024×768 panel and save original state:

```
VSTR /P:1024 /ON /SAVE
```

Restore values from previous save:

```
VSTR /OFF
```

Check which registers differ from saved state:

```
VSTR /DIFF
```

---

## Notes

- Registers are only modified once; the utility exits immediately.  
- If a game or program resets registers afterwards, simply rerun VSTR.  
- On first run, use /SAVE to capture BIOS defaults for safety.  
- Only tested on Toshiba laptops with CT65550/554/555 chipsets, but should apply 
  to similar systems.

---

# TESTVLR - CT6555x Vertical Line Replication Test Utility

This is a small DOS program to experiment with **vertical line replication** in graphics modes on laptops with the **Chips & Technologies 6555x** graphics controller. 

The tool makes it possible to explore **FR4D** (Vertical Line Replication Register) on various hardware and related registers in real time. This input may be needed to fine-tune the VSTR utility to work on as wide range of compatible hardware as possible.

---

## Features

- Supports the common DOS graphics modes that exercise different input heights:
  - **320×200×256** (VGA mode 0x13 → 200 lines, double-scanned to 400)
  - **640×350×16** (EGA mode 0x10 → 350 lines)
  - **640×400×256** (VESA mode 0x100 → 400 lines)
  - **640×480×256** (VESA mode 0x101 → 480 lines)
  - **800×600×256** (VESA mode 0x103 → 600 lines)

- Draws an **even/odd stripe pattern** with a **20-pixel tick mark** every 10 lines.  
  This makes it easy to see when lines are **replicated** (tripled lines appear clearly).

- Provides **interactive hotkeys** to tweak `FR4D` (low/high nibbles) and related registers live, without rebooting or recompiling.

---

## Build Instructions

Prerequisites: **Borland C++ 3.1** (or Turbo C++ 3.x), memory model: **Large**  

```bat
bcc -ml testvlr.c 
```

or run the provided `MAKE.BAT` file.

## Usage

1. Run `TESTVLR.EXE` in DOS.  
2. Select the desired video mode:
 - `[1]` 320×200×256 (200 lines)
 - `[2]` 640×350×16 (350 lines)
 - `[3]` 640×400×256 (400 lines)
 - `[4]` 640×480×256 (480 lines)
 - `[5]` 800×600×256 (600 lines)

3. The program draws the stripe/tick pattern and enables flat-panel scaling logic.

4. Use hotkeys to experiment with scaling:

| Key            | Action                                                       |
| -------------- | ------------------------------------------------------------ |
| `+`/up arrow   | Increase N (fixed replication interval, FR4D low=high)       |
| `-`/down arrow | Decrease N (fixed replication interval, FR4D low=high)       |
| `e`            | Toggle FR48 EVCP/LR bits (enable/disable scaling engine)     |
| `q`            | Quit to text mode and restore registers                      |
| `x`            | Quit to text mode and **do not** restore registers (useful for testing in games, for example) |

5. Observe how the picture stretches or leaves black bars depending on FR4D values.

---

## Background

- **FR4D** controls the **Vertical Line Replication interval**:  
- Low nibble = minimum interval  
- High nibble = maximum interval  
- The scaler starts with the low value, and if the result would **overflow** the panel, it increases up to the high nibble.  
- **FR4E=0x3F** enables stretching for all vertical display end (VDE) classes.  
- **FR48=0x17** enables vertical compensation and line replication.  
- **FR41=0x07, FR40=0x3F** are needed for proper latch/compensation.  
- The program sets **FR01[1]=1** to route through the flat-panel engine.  

---

## For 800×600 Panel Users

On an 800×600 panel, all classic modes (200/350/400/480/600) can be stretched to (almost) fill the screen. The tool confirms the correct `FR4D` values and helps visualize how repeated lines are inserted.

---

## For 1024×768 Panel Users

On a 1024×768 panel, stretching behavior is different. Some modes (e.g. 480-line input) can nearly fill the screen, while others will leave blank space.  

**Please help test!**
- Try each mode (200, 350, 400, 480, 600).
- Start with `FR4D=0x00` (low=0, high=0).
- Use `+/-`  or up arrow/down arrow keys to adjust until the picture nearly fills vertically.
- Report back which values give the best fit and how many blank lines remain.

---

## Safety / Notes

- The tool **does not write to disk**. It only pokes VGA/CT65550 registers.  
- Exit with `q` so original register values are restored.  
- On some BIOSes, unsupported modes (like 0x100/0x103) may not work — that’s normal.  


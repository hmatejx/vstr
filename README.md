# VSTR - Video Stretch Utility for Chips & Technologies CT6555x

VSTR is a small DOS command-line utility to enable or disable LCD panel scaling 
(horizontal and vertical stretching) on laptops using the C&T CT6555x graphics chipsets 
(CT65550, CT65554, CT65555).  

It is designed as a functional replacement and extension of the older
[VEXP](https://archive.org/details/vexp13_1997) utility, but adds support for newer
presets such as 1024×768 panels.

---

## Background

Many Toshiba and similar laptops of the mid-1990s used flat-panel LCDs with native 
resolutions such as 800×600 or 1024×768. In standard DOS modes (320×200, 640×480, etc.), 
the BIOS usually doubles pixels but **does not stretch the image** to fill the panel, 
leaving a centered picture with black borders. While BIOS typically contains settings that
enable text mode stretching, the graphics modes, such as those those used by games,
are usually not stretched by default.

The CT6555x family includes built-in hardware scalers that can stretch these modes 
to the full panel. The scaler is controlled through a set of **extended registers**.  

By toggling these registers, `VSTR` allows you to enable or disable stretching at will.

---

## Registers Used

The CT6555x chip exposes a set of **extended registers** (via ports 0x3D6/0x3D7) that 
control the LCD panel’s stretch logic:

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

Note: Registers FR49–FR4C control *text mode vertical replication*.
These are not modified by VSTR. On Toshiba laptops the BIOS already programs sensible
defaults for text scaling, and changing them can distort characters.
VSTR intentionally avoids them.

For detailed descriptions, refer to the official chipset documentation:  
- [CT65550 Datasheet (PDF)](http://bitsavers.informatik.uni-stuttgart.de/components/chipsAndTech/CHIPS_65550_199710.pdf)
- [CT65554 Datasheet (PDF)](https://www.vgamuseum.info/index.php/cpu/item/download/275_f6c6c4863a2c754f97ba1e7d628725f8)
- [CT65555 Datasheet (PDF)](https://www.dosdays.co.uk/media/c_and_t/ct65555_datasheet.pdf)


---

## Usage

```
VSTR [options]
```

### Options

- **`/ON`**  
  Enable stretching using either the current panel preset or the last selected one.  
  Does *not* save original register state unless /SAVE is specified.

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

## Compatibility Note

Some OEM documentation and chip markings use the **F** prefix or suffix (e.g. CT F6555x).
These are equivalent to CT 6555x. The "F" was a vendor/marketing notation added sometime after
the initial introduction of these chips denoting flat panel support.

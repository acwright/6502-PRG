# VDP assessment: 6502-PRG

> An outline, not a plan. The detailed plan for this repository goes in `VDP-PLAN.md`,
> written in a session of its own. Surveyed 2026-09-16 across the whole workspace.

## The change

The ACE moves from a Pico9918 running stock TMS9918A firmware to the **6502-PICOVDP**
(`6502-PICOVDP/SPEC.md`) on PICO9918 PRO v2.0 hardware, running **BIOS 2.x**. Everything
else stays where it is: COB, DEV, KIM, VCS, PicoCalc, and any ACE whose card cannot be
reflashed (RP2040 pico9918 v1.0–1.3). Those keep the stock firmware and **BIOS 1.x**,
whose last release is **1.6**.

- **Legacy** in these documents means TMS9918A + BIOS 1.x. **VDP** means PICOVDP + BIOS 2.x.
- **Compatibility runs one way.** The PICOVDP's legacy submode runs Text and Graphics I
  programs unchanged, so BIOS 1.x and existing cartridges run on it. Graphics II and
  Multicolor fall back to Graphics I and draw garbage. Register writes above 7 no longer
  alias, so F18A tricks break. Sprites per line are 16 by default, not 4. Nothing written
  for the VDP runs on a TMS9918A.

## Decisions already made

- **No new repositories.**
- **BIOS 1.6 is the last 1.x release.** It is 1.5 plus the NVRAM save slots in
  `6502-BIOS/PLAN.md`, and nothing else. It ships in emulator **2.7.0**, and the frozen
  legacy docs document it.
- **BIOS 2.0 is 1.6 plus:**
  - The PICOVDP work in `6502-EMULATOR`'s `docs/handoff/6502-BIOS.md` (branch `v3-vdp`):
    card detection, hardware scroll, port B for interrupt handlers, `WaitVBlank`.
  - A console in the PICOVDP's **Text mode** (`VMODE $1`, 40×24, 6×8 cells) with a
    **per-cell colour table**. It keeps the ROM font and every screen layout.
  - **No Monitor.** The machine **boots straight to BASIC**, with a new header and a colour
    logo drawn from the ROM font's CP437 block characters. Wozmon stays at `$FF00`.
  - **BASIC takes the Monitor's 4.3 KB** (`$C000–$FEFF`). The Kernal (`$A000–$B7FF`) and
    the character set (`$B800`) do not move, because cartridges overlay `$C000–$FFFF`.
    The Kernal holds the primitives cartridges need; BASIC-only work lives in BASIC.
  - **New BASIC commands with matching Kernal entries.**
    - Core: `SCREEN`, `VPOKE`/`VPEEK`, `VREG`, `PALETTE`, `VSYNC`, `VLOAD`.
    - Second tier, if room is found: `SPRITE`, `SCROLL`, `LAYER`, `VSTAT`.
    - Save-slot commands, if room is found.
    - `SYS addr[,a,x,y]`, and `BLOAD`/`BSAVE` over XModem when given no filename.
    - BASIC returns to the text console when a program stops.
  - **Tokens:** every 1.x token keeps its value, and new keywords are appended after `$D4`.
    The `BRK` statement is retired and its token `$B4` goes to a new keyword.
  - **A BRK instruction** prints `BREAK $nn AT $xxxx  A= X= Y= P= S=` and warm-starts
    BASIC. `BRK_PTR` stays hookable.
  - **`COLOR fg[,bg[,border]]`** sets the pen for later output, `CLS` fills the screen with
    it, and `border` is register 7's low nibble.
  - **Existing jump-table addresses do not move.** New entries are appended.
- **6502-EMULATOR** makes the video card an option (TMS9918A or PICOVDP): one app, one
  site. It also publishes a frozen **2.7.0** web build at `/6502-EMULATOR/v2/` for the
  legacy docs.
- **6502-DOCS** is versioned: legacy docs (BIOS 1.6) are frozen at `/6502-DOCS/v1/`, and
  the main site is rewritten for the VDP and BIOS 2.x.
- **6502-BIOS** gets a `v1.x` branch cut at `v1.6`; `main` becomes 2.x.
- **Assembly and C projects** get a VDP include chosen by a build option, not branches.
  The legacy `6502.inc` gets one last update, for 1.6.
- **EhBASIC and vc83basic** stay 1.x. **PicoCalc** stays legacy. **The YouTube series**
  teaches the legacy VDP and mentions the new features.

## Order across the workspace

**Part 1: BIOS 1.6, the last legacy release**

1. **6502-BIOS:** build 1.6 on `main`, tag `v1.6`, and cut `v1.x` from it.
2. **6502-EMULATOR `main`:** bundle 1.6, re-capture the `bios/` goldens (the splash says
   v1.6), and release **2.7.0**. Then merge `main` into `v3-vdp` and re-capture there.
3. **6502-PICOVDP:** re-sync `tests/oracle/`, whose pinned `bios` goldens moved.
4. **The legacy include** gains the NVRAM entries in every copy: 6502-ASM, 6502-CRT,
   6502-PRG, 6502-BIN, 6502-EHBASIC, 6502-C (with `6502.h`) and WIZARDSLAB.
5. **6502-DOCS `main`** documents 1.6 and pins 2.7.0. Then it cuts `v1`, published at
   `/6502-DOCS/v1/`, against the emulator's frozen 2.7.0 build at `/6502-EMULATOR/v2/`.

**Part 2: the VDP**

6. **6502-PICOVDP:** firmware proven on the PRO (its Phases 9–11). This gates the
   hardware switch, not the software work.
7. **6502-EMULATOR:** `v3-vdp` merged, with the card as an option; tagged 3.x.
8. **6502-BIOS:** 2.0 on `main`. This can start once step 1 is done, because the `v3-vdp`
   emulator already runs the PICOVDP.
9. **6502-ASM** sets the VDP include convention. 6502-CRT, 6502-PRG, 6502-BIN and 6502-C
   follow it.
10. **Everything else follows BIOS 2.0:**
    - The emulator bundles BIOS 2.0.
    - 6502-DOCS `main` is rewritten.
    - bastok gains the 2.x token table.
    - 6502-ACE, WIZARDSLAB, 6502-EHBASIC, vc83basic, cffs and 6502-ASSEMBLY follow.

---

## This repository's role

The program template: `Program.asm` → `Program.prg`, loaded at `$0800` under BASIC with
a `SYS` stub. It also produces `Program.woz` (bin2woz) and `Program.img` (cffs).

## Where it stands

- `Makefile` targets: `build`, `view`, `run` (`6502 run Program.prg`), `woz`, `cf`.
- Its `6502.inc` is the settled legacy include, byte-identical across the workspace and
  checked against the BIOS v1.5 build (see 6502-ASM's assessment). It takes BIOS 1.6's
  NVRAM entries when 6502-ASM updates the copies.
- `README.md` describes loading through the Monitor's `L` (and `X` back to BASIC).
- `README.md` notes minimum BIOS versions ("Requires BIOS v1.3 or later" for `LOAD`).

## Work outline

**Part 1:** take the 1.6 legacy include from 6502-ASM; nothing else changes.

**Part 2 is blocked on 6502-ASM's convention.**

1. Adopt the VDP include and build option exactly as 6502-ASM defines them. `woz` and
   `cf` outputs need distinct names per build if both are kept.
2. `run` for a VDP build selects the emulator's PICOVDP card.
3. `README.md`:
   - A VDP program needs BIOS 2.x and a converted ACE. A `.prg` gives no warning when
     loaded on a legacy machine, so recommend checking `KernalVersion` and the card-type
     byte.
   - Add BIOS 2.0 to the minimum-version notes.
   - The Monitor `L` route is 1.x only. On 2.x a `.prg` loads with `LOAD`, from CF or over
     XModem.
4. **bin2woz and cffs:** no changes expected. The Wozmon and CF filesystem formats are
   unchanged in the BIOS 2.0 baseline.

## Linked repositories

| Repository | Path | Why |
|---|---|---|
| 6502-ASM | `~/Developer/Assembly/6502-ASM` | Defines the include and build-option convention |
| 6502-BIOS | `~/Developer/Assembly/6502-BIOS` | BIOS 2.0 jump table, card detection byte |
| 6502-EMULATOR | `~/Developer/NodeJS/6502-EMULATOR` | Card flag for `make run` |
| 6502-CRT, 6502-BIN | `~/Developer/Assembly/6502-CRT`, `~/Developer/Assembly/6502-BIN` | Sibling templates with the same include; keep them in step |
| cffs, bin2woz | `~/Developer/NodeJS/cffs`, `~/Developer/NodeJS/bin2woz` | Used by the Makefile; unaffected unless BIOS 2 changes storage or Wozmon |

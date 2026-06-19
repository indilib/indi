# SPK Pointing Kernel

Vendored copy of P.T. Wallace's SPK pointing kernel library.

**Upstream source:** https://www.tpointsw.uk/spk.htm

## Files

| File | Description |
|---|---|
| `spk.h` | Library header |
| `astr.c` | Astrometry setup (`spkAstr`) |
| `ctar.c` | Catalog target transform (`spkCtar`) |
| `pmfit.c` | Pointing model least-squares fit (`Pmfit`) |
| `vtel.c` | Virtual telescope / encoder demands (`spkVtel`) |
| `s2e.c` | One-call star-to-encoders wrapper (`spkS2e`) |
| `altaz.c` | AltAz mount demonstrator (not compiled) |
| `equat.c` | Equatorial mount demonstrator (not compiled) |
| `point.c` | Minimal usage example (not compiled) |
| `pmfit_altaz.dat` | Sample AltAz pointing model data |
| `pmfit_equat.dat` | Sample equatorial pointing model data |
| `spk.pdf` / `spk.tex` | Wallace's reference documentation |
| `Makefile` | Original build (reference only; INDI uses CMake) |
| `sofa.h` | INDI-added shim redirecting SOFA `iauXxx` calls to ERFA |

## Modifications

- `sofa.h`: added by INDI to redirect SOFA API calls to ERFA, avoiding
  SOFA's restrictively licensed sources.
- `vtel.c`: corrected VD (vertical deflection) formula from `pvd` to
  `pvd*cos(el)` to match the `TF` coefficient shape used by `pmfit.c`.
- `pmfit.c`: reordered `Bfun` basis functions so that polar-axis terms
  (ME/MA for equatorial, AN/AW for altazimuth) precede collimation (CH/CA).
  Original upstream order: IH, ID, CH, ME, MA, TF / IA, IE, CA, AN, AW, TF.
  INDI order: IH, ID, ME, MA, CH, TF / IA, IE, AN, AW, CA, TF.
  This ensures a 3-point fit produces a well-conditioned 4-term polar solution
  (IH, ID, ME, MA) rather than including the correlated collimation term.

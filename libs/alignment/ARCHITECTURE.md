# Alignment Math Subsystem Architecture

This document defines the mathematical and conceptual architecture of INDI's telescope pointing model subsystem. Because telescope tracking depends on translating idealized sky coordinates to physical mechanical hardware, INDI provides different math plugins to model these discrepancies depending on user needs and hardware setups.

This guide details the shared conventions across the subsystem and outlines the two primary approaches used by the plugins: **Matrix-Based local mapping** and **Physics-Based unified modeling**.

---

## 1. Shared Core Concepts

Regardless of the solver algorithm, all alignment plugins must negotiate standard coordinate systems and sidereal time.

### Coordinate Systems
The subsystem explicitly manages mappings between standard astronomical conventions and the existing INDI simulator definitions:

| System | Origin | Azimuth/Roll Direction | Notes |
| :--- | :--- | :--- | :--- |
| **INDI/Simulator** | South ($0^\circ$) | West positive (CW) | The domain of demanded vectors. |
| **Standard Astrometry** | North ($0^\circ$) | East positive (RH) | The domain used internally by standard libraries like SOFA/ERFA. |
| **Equatorial Space** | South ($0^\circ$) | West positive (CW) | Hour Angle defined as ($\text{LST} - \text{RA}$). |

> [!IMPORTANT]
> **Azimuth/Roll Conversion Notes**
> - The Simulator (SWN) uses South=0, West=90, North=180, East=270.
> - Formal Astrometry (NEP) uses North=0, East=90, South=180, West=270.
> Because of this, mapping requires $Az_{astrometry} = Az_{sim} + \pi$. Likewise, roll signs must be inverted: $Roll_{astrometry} = -Az_{sim}$ to handle CW vs RH parity.

### Time and Sidereal Synchronization
By architectural design, all plugins rely on **libnova** to determine Local Sidereal Time (LST) to maintain tight integration and backward compatibility with the INDI Simulator and driver ecosystem. 

Some plugins (like SPK) optionally incorporate independent astrometry libraries (like `ERFA`). To ensure there is no clock drift between `libnova` and the internal engines, synchronization occurs using the Earth Rotation Angle (ERA):

$$ \text{LST}_{rad} = (\text{GAST}_{hrs} + \lambda_{hrs}) \times \frac{\pi}{12} $$
$$ \text{eral} = \text{LST}_{rad} + \text{EO} $$
*(Where EO is the Equation of the Origins)*

---

## 2. Mathematical Philosophies

The Math Subsystem operates two distinct classes of plugin solvers, each excelling in different deployment scenarios.

### A. Matrix-Based Pointing (SVD & BuiltIn)
These matrix plugins treat pointing as a geometric mapping problem, without requiring explicit parameterization of telescope hardware constraints (like bent tubes or drooping vertical axes). They solve a transformation matrix $T$ that takes a true Celestial Vector ($A$) and maps it to a mechanical Mount Vector ($B$).

Because a single transformation matrix cannot account for physical flexure across the full sky, **these plugins rely on a highly effective piecewise local mesh:**
1. The `BasicMathPlugin` engine connects all alignment stars into a Delaunay triangle mesh.
2. When slewing to a target, the subsystem intersects the target path with the mesh to find the exact 3 nearest stars surrounding the target.
3. The plugin generates a highly localized matrix just for that small patch of sky, which organically absorbs local hardware flexure without needing to mathematically model the physics.

**The Convex Hull Consideration:** This local mesh strategy works best when the target is inside the "boundary" or Convex Hull of all aligned stars. Slewing significantly outside the boundary requires the subsystem to extrapolate, which can reduce relative accuracy.

#### BuiltIn Plugin (Algebraic Inversion)
Given 3 local stars, it does raw inversion: $T = B \cdot A^{-1}$
> [!NOTE]
> Because BuiltIn optimizes for an exact fit to the three stars, $T$ may mathematically skew or scale the local coordinate space to force the triangle to match. This can occasionally affect precision if the mechanical axes are heavily misaligned.

#### SVD Plugin (Markley's Method / Kabsch)
Given the same 3 local stars, SVD applies rigid-body constraints. It uses Singular Value Decomposition to calculate the "best-fit" matrix $T$ while demanding that $T$ remains a **pure rigid 3D rotation** (an orthogonal matrix with determinant = $+1$). This protects the geometry of the coordinate sphere while solving both Index Errors and Polar Misalignment simultaneously.

---

### B. Physics-Based Pointing (SPK)
The SPK plugin derives from Patrick Wallace's TCSPK library (`tpointsw.uk`). Instead of mapping localized sky patches, it models the entire sky using a unified physical parameterization. It predicts the alignment by estimating the mechanical traits of the mount.

It maps 6 distinct terms:
1. `IA` (Index Roll Error)
2. `IB` (Index Pitch Error)
3. `ME` (Polar Elevation)
4. `MA` (Polar Azimuth)
5. `CH` (Collimation/Non-perpendicularity proxy)
6. `VD` (Vertical droop / flexure proxy)

#### The Progression Constraints
Because it uses a unified physics model, it requires sufficient data points to differentiate between similar mechanical errors (such as Collimation vs Polar Misalignment). The math subsystem enforces a progression based on how many points the user has synced to maintain mathematical stability:
*   **1-2 Points**: Fits `IA` and `IB` only (Offset sync / Indexing).
*   **3-5 Points**: Fits `IA, IB, CH, ME, MA` (Polar approximation modeling SVD parity).
*   **6+ Points**: Solves all terms (Activates the full physical structural model).

---

## 3. Implementation Specifics (SPK/Physics)

The SPK model involves a few specific implementation abstractions to interface with the rest of INDI:

### SPK Transformation Pipeline
Transformations occur via a "Virtual Telescope" model.
1. `TransformCelestialToTelescope(RA, Dec)` $\rightarrow$ `Vector`
   * It takes the target coordinate and calls the `spkVtel` internal solver twice (a two-pass loop).
   * Because tube flexure depends on the current tracking elevation, but the plugin operates statelessly, the first pass calculates a hypothetical mount position (using $0,0,0$). The second pass feeds that result back into `spkVtel` to calculate a refined, self-consistent tube droop correction.

### ERFA / SOFA Compatibility Layer
The SPK C sources (`spk_vtel.c`, `spk_pmfit.c`, `spk_astr.c`) were natively designed for the SOFA (`iauXxx`) API. Since SOFA has a non-standard restrictive license, INDI uses the open-source **ERFA** fork (which is mathematically identical).

A C-header shim (`libs/alignment/sofa.h`) is used to map the API at compile time without modifying Wallace's core source:
```c
typedef eraASTROM iauASTROM;
#define iauXxx eraXxx;
```

#### ERFA Rotation Matrix Convention Note
The internal matrix generator (`spkVtel`) uses `iauRx`, `iauRy`, and `iauRz`. It is important to note that ERFA does **not** follow the standard "active" rotation matrix format.
1. **Passive**: They are "alias" rotations (they rotate the coordinate frame, not the vector). Each matrix is the *transpose* of standard active matrices.
2. **Left-multiplication**: They stack left-to-right rather than right-to-left.
Substituting standard $M^T_{active}$ math engines here will result in polar offset (`AN, AW`) coefficients being applied inversely, which would cause systematic pointing errors.

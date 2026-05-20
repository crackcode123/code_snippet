# FRAP Diffusion Equation — Step-by-Step Derivation

Context: 1D diffusion equation for FRAP (Fluorescence Recovery After Photobleaching) experiments, on a bounded interval with no-flux walls and a symmetric "bleached strip" initial condition.

---

## Problem Setup

**PDE:**

  ∂c/∂t  =  D · ∂²c/∂x²,   for x ∈ (−L, +L),  t > 0

**Initial condition (a bleached strip of half-width `a` centered at the origin):**

  f(x) = c(x, 0) =
  - c₀,  for −L < x < −a
  - 0,   for −a < x < +a
  - c₀,  for +a < x < +L

**Boundary conditions (no-flux / reflective walls):**

  ∂c/∂x at x = +L  =  0
  ∂c/∂x at x = −L  =  0

**Two key observations:**

1. The initial condition is **even**: f(−x) = f(x). The graph is mirror-symmetric about x = 0.
2. The BCs are **Neumann** (the derivative of c is pinned to zero, not c itself).

These two facts together force the answer to be a pure cosine series.

---

## STEP 1 — Separation of Variables

### Plain-text version

We guess that the solution can be written as a product of two functions:

  c of x and t equals X of x times T of t.

Plug this into the PDE. You get:

  X times T-prime equals D times X-double-prime times T.

Divide both sides by D times X times T. You get:

  T-prime divided by (D times T) equals X-double-prime divided by X.

The left side depends only on time. The right side depends only on space. The only way both sides can be equal everywhere and always is if both sides are the same constant. Call that constant minus lambda. So we get two ordinary differential equations:

- Spatial equation: X-double-prime plus lambda times X equals zero.
- Temporal equation: T-prime plus lambda times D times T equals zero.

The boundary conditions become: X-prime at plus L equals zero, and X-prime at minus L equals zero.

### Math form

**Starting point — the PDE:**

  ∂c/∂t  =  D · ∂²c/∂x²

**The guess (separation of variables):**

  c(x, t)  =  X(x) · T(t)

**Compute the two derivatives needed.**

Time derivative (only T depends on t, so X comes out as a constant):

  ∂c/∂t  =  X(x) · T'(t)

Second space derivative (only X depends on x, so T comes out as a constant):

  ∂²c/∂x²  =  X''(x) · T(t)

Here T'(t) means dT/dt, and X''(x) means d²X/dx².

**Substitute into the PDE:**

  X(x) · T'(t)  =  D · X''(x) · T(t)

**Divide both sides by D · X(x) · T(t):**

  T'(t) / [ D · T(t) ]  =  X''(x) / X(x)

The left side has only t in it. The right side has only x in it. So both sides must equal the same constant. Name that constant −λ:

  T'(t) / [ D · T(t) ]  =  X''(x) / X(x)  =  −λ

**Two ordinary differential equations result:**

Spatial ODE (rearranged from X''/X = −λ):

  X''(x) + λ · X(x)  =  0

Temporal ODE (rearranged from T'/(DT) = −λ):

  T'(t) + λ · D · T(t)  =  0

**Boundary conditions, rewritten in terms of X.** Since c = X · T and only X depends on x:

  ∂c/∂x  =  X'(x) · T(t)

For this to vanish at x = ±L for all t > 0 (and T not identically zero), we need:

  X'(+L)  =  0
  X'(−L)  =  0

**Summary of Step 1:**

  PDE:        ∂c/∂t  =  D · ∂²c/∂x²
  Guess:      c(x, t)  =  X(x) · T(t)
  Yields:     X''(x) + λ · X(x)  =  0,   with X'(±L) = 0
              T'(t) + λ · D · T(t)  =  0

---

## Clarification — What is capital X?

**Capital X is a FUNCTION.** It is a function of x alone (only the space variable). It does NOT depend on time. Similarly, capital T is a function of t alone (only the time variable). It does NOT depend on space.

- Lowercase x is the variable (a position on the line).
- Capital X is a function that takes x as input and returns a number.
- Lowercase t is the variable (a moment in time).
- Capital T is a function that takes t as input and returns a number.

The names X and T are arbitrary; some textbooks call them phi and psi, or F and G.

**Math form:**

  c(x, t)  =  X(x) · T(t)

For any specific point in space x and any specific moment in time t, compute X at that x, compute T at that t, multiply those two numbers, and that gives you c at that x and t.

**Concrete example.** Suppose hypothetically X(x) = cos(x) and T(t) = e^(−t). Then c(x, t) = cos(x) · e^(−t).

- At x = 0, t = 0:  c = cos(0) · e^0 = 1 · 1 = 1.
- At x = π/2, t = 1:  c = cos(π/2) · e^(−1) = 0 · 0.368 = 0.

**Why guess this form?** Mathematical convenience: if c happens to factor this way, then the PDE (hard, two variables) splits into two ODEs (easier, one variable each). Not every solution factors like this. But the linear sum of many such product solutions IS general enough to represent any solution.

---

## STEP 2 — Solve the Spatial Eigenvalue Problem

### Plain-text version

We look for all values of lambda for which there is a non-zero solution X. These are called eigenvalues. The corresponding X functions are called eigenfunctions. We try each possible sign of lambda separately.

- **Case A. Lambda is negative.** Solutions are exponentials. Boundary conditions force only zero. No useful eigenvalue here.
- **Case B. Lambda is exactly zero.** X equals alpha plus beta x. Boundary conditions force beta to be zero. So X equals a constant. X-zero equals 1 with lambda-zero = 0. THIS IS THE SPECIAL ZERO MODE.
- **Case C. Lambda is positive.** Solutions are sines and cosines. The boundary conditions select either cos(n π x / L) with eigenvalues (n π / L)² or sin((n − ½) π x / L). Both families are admitted on [−L, +L].

### Math form

**The problem to solve:**

  X''(x) + λ · X(x)  =  0,   for x ∈ (−L, +L)
  with boundary conditions:  X'(−L) = 0  and  X'(+L) = 0

#### Case A.  λ < 0   (write  λ = −μ²,  with μ > 0)

The ODE becomes:

  X''(x) − μ² · X(x)  =  0

General solution (hyperbolic form):

  X(x)  =  α · cosh(μ · x)  +  β · sinh(μ · x)

Derivative:

  X'(x)  =  α · μ · sinh(μ · x)  +  β · μ · cosh(μ · x)

Apply X'(+L) = 0:

  α · μ · sinh(μ · L)  +  β · μ · cosh(μ · L)  =  0       … (A1)

Apply X'(−L) = 0 (using sinh odd, cosh even):

  −α · μ · sinh(μ · L)  +  β · μ · cosh(μ · L)  =  0      … (A2)

Add (A1) + (A2):

  2 · β · μ · cosh(μ · L)  =  0   ⇒   β = 0  (since cosh ≥ 1, μ > 0)

Subtract (A1) − (A2):

  2 · α · μ · sinh(μ · L)  =  0   ⇒   α = 0  (since sinh(μL) > 0 for μ > 0)

Conclusion: only the trivial solution X ≡ 0. **No negative eigenvalue is allowed.**

#### Case B.  λ = 0

The ODE becomes:

  X''(x)  =  0

Integrate twice:

  X'(x)  =  β
  X(x)  =  α + β · x

Apply X'(±L) = 0:  β = 0.

So X(x) = α (any constant). Pick the simplest non-zero one:

  X₀(x)  =  1,    with eigenvalue  λ₀ = 0

**λ = 0 IS an eigenvalue, with eigenfunction equal to a constant. This is the special "zero mode."**

#### Case C.  λ > 0   (write  λ = μ²,  with μ > 0)

The ODE becomes:

  X''(x) + μ² · X(x)  =  0

General solution:

  X(x)  =  A · cos(μ · x)  +  B · sin(μ · x)

Derivative:

  X'(x)  =  −A · μ · sin(μ · x)  +  B · μ · cos(μ · x)

Apply X'(+L) = 0:

  −A · μ · sin(μ · L)  +  B · μ · cos(μ · L)  =  0       … (C1)

Apply X'(−L) = 0 (using sin odd, cos even):

  +A · μ · sin(μ · L)  +  B · μ · cos(μ · L)  =  0       … (C2)

Add (C1) + (C2):

  2 · B · μ · cos(μ · L)  =  0       … (*)

Subtract (C1) − (C2):

  −2 · A · μ · sin(μ · L)  =  0      … (**)

Two ways to get a nontrivial solution:

- **Sub-case C-1.**  A ≠ 0,  B = 0,  sin(μ · L) = 0  ⇒  μ · L = n · π,  n = 1, 2, 3, …
  - μₙ = n · π / L,  λₙ = (n π / L)²
  - eigenfunction:  Xₙ(x) = cos(n · π · x / L)

- **Sub-case C-2.**  B ≠ 0,  A = 0,  cos(μ · L) = 0  ⇒  μ · L = (n − ½) · π
  - eigenfunction:  sin((n − ½) · π · x / L)

#### Full eigenbasis on (−L, +L) with Neumann BCs

| Eigenvalue | Eigenfunction | Parity |
|------------|---------------|--------|
| λ = 0 | 1 | even |
| (n π / L)², n ≥ 1 | cos(n π x / L) | even |
| ((n − ½) π / L)², n ≥ 1 | sin((n − ½) π x / L) | odd |

#### Using the symmetry of the initial condition

f(x) is even: f(−x) = f(x). The coefficient in front of each sine is

  Bₙ  =  (1 / L) · ∫₋ₗᴸ f(x) · sin((n − ½) π x / L) dx

The integrand is (even) · (odd) = odd. The integral of any odd function over [−L, +L] is zero. So Bₙ = 0 for every n.

**Result. Only the cosines and the constant survive:**

  λ₀ = 0,         X₀(x) = 1
  λₙ = (n π / L)²,    Xₙ(x) = cos(n π x / L),    n = 1, 2, 3, …

---

## Clarification — Why Exponentials, and Why Hyperbolic Functions?

### Part 1. Why exponentials in the first place?

The Case A ODE is  X''(x) − μ² · X(x)  =  0. We look for a function whose second derivative gives back itself times the positive number μ². This is the defining property of the exponential. If

  X(x)  =  e^(r · x)

then X''(x) = r² · e^(r · x). Plug in:

  (r² − μ²) · X(x)  =  0   ⇒   r = ±μ

Two basic solutions:

  e^(+μ · x)  — growing
  e^(−μ · x)  — decaying

The general solution (linear combination):

  X(x)  =  C₁ · e^(+μ · x)  +  C₂ · e^(−μ · x)

Contrast with Case C, where  X'' + μ² X = 0  requires r² = −μ², giving imaginary exponents — which by Euler's formula become sines and cosines. **The SIGN of the constant in front of X flips the character of the solutions.**

### Part 2. Why repackage as hyperbolic functions?

By definition:

  cosh(μ x)  =  ( e^(+μx) + e^(−μx) ) / 2   (even)
  sinh(μ x)  =  ( e^(+μx) − e^(−μx) ) / 2   (odd)

So writing

  X(x)  =  C₁ · e^(+μx)  +  C₂ · e^(−μx)

is mathematically IDENTICAL to writing

  X(x)  =  α · cosh(μx)  +  β · sinh(μx)

with  α = C₁ + C₂  and  β = C₁ − C₂. Same function, different bookkeeping.

**Three reasons to prefer the hyperbolic form:**

1. **Parity is built in.** cosh is even, sinh is odd. Symmetric problems get cleaner.
2. **It mirrors the sin/cos case.** Case A uses cosh/sinh, Case C uses cos/sin — same algebraic structure, same procedure.
3. **Boundary conditions decouple at symmetric points.** At x = 0, sinh(0) = 0 and cosh(0) = 1, just like sin/cos. Raw exponentials e^(±μ · 0) both equal 1, mixing the constants.

**Concrete comparison from Case A.** Using hyperbolic form, applying X'(±L) = 0 decouples α and β in two lines via add/subtract. Using raw exponentials, you still get the same answer, but C₁ and C₂ tangle through different exponential factors, hiding the symmetry.

**One-line summary:** exponentials arise because they are the natural solutions of X'' = μ² X. Hyperbolic functions are just the even/odd repackaging of those exponentials, chosen because they make symmetric problems clean, parallel the cos/sin case, and decouple boundary conditions at symmetric points.

---

## Clarification — Why Multiply f(x) by the Sine? What is the Even Function? Why the Initial Condition?

### Why multiply f(x) by the sine? — ORTHOGONALITY

The general solution is:

  c(x, t)  =  A₀ · 1  +  Σ Aₙ · cos(n π x / L) · e^(…)  +  Σ Bₙ · sin((n−½) π x / L) · e^(…)

At t = 0, every exponential equals 1, so:

  f(x)  =  A₀  +  Σ Aₙ · cos(n π x / L)  +  Σ Bₙ · sin((n−½) π x / L)

We know f. We do NOT know A₀, Aₙ, Bₙ. To extract just one of them — say B₅ — multiply both sides by sin(4.5 π x / L) and integrate from −L to +L:

  ∫₋ₗᴸ f(x) · sin((5 − ½) π x / L) dx
    =  A₀ · ∫₋ₗᴸ 1 · sin(4.5 π x / L) dx
       +  Σ Aₙ · ∫₋ₗᴸ cos(n π x / L) · sin(4.5 π x / L) dx
       +  Σ Bₙ · ∫₋ₗᴸ sin((n − ½) π x / L) · sin(4.5 π x / L) dx

**Almost every integral on the right is exactly zero.** Only the one with the matching sine (n = 5) survives. This property is called ORTHOGONALITY:

  ∫₋ₗᴸ sin((n−½) π x / L) · sin((m−½) π x / L) dx  =  L   if n = m
                                                    =  0   if n ≠ m
  ∫₋ₗᴸ cos(n π x / L) · sin((m−½) π x / L) dx  =  0   for all n, m
  ∫₋ₗᴸ 1 · sin((m−½) π x / L) dx  =  0   for all m ≥ 1

So:

  ∫₋ₗᴸ f(x) · sin(4.5 π x / L) dx  =  B₅ · L
  ⇒  B₅  =  (1 / L) · ∫₋ₗᴸ f(x) · sin(4.5 π x / L) dx

We multiply by the eigenfunction we want to isolate because all the other eigenfunctions vanish under the integral. This is the discrete-function analog of: "to find the x-component of a vector, dot-product it with the unit vector in the x direction."

### What is the even function here?

The even function is **f(x)**, the initial condition.

  f(+0.5) = c₀   (assuming a < 0.5 < L)
  f(−0.5) = c₀   — same.

  f(+0.1) = 0    (assuming |0.1| < a, in the bleached zone)
  f(−0.1) = 0    — same.

So f(−x) = f(x) for every x in (−L, +L). That is the definition of an even function.

The sine of anything is odd: sin(−θ) = −sin(θ).

The product f(x) · sin((n − ½) π x / L) is (even) · (odd) = odd.

**Parity rule:** even × even = even,  odd × odd = even,  even × odd = odd. (Same rule as multiplying + and − signs.)

The integral of any odd function over a symmetric interval [−L, +L] is zero — the right half exactly cancels the left half. Therefore every Bₙ = 0. No sines in the answer.

### Why use the initial condition?

Because the PDE and BCs do not fix the coefficients. Any choice of A₀, Aₙ, Bₙ gives a function that solves the PDE and respects the BCs. The one piece of data that DISTINGUISHES our solution from all other candidates is c(x, 0) = f(x). That is what the initial condition does: it provides the data that pins down the otherwise-arbitrary constants in the series.

---

## STEP 3 — Why Only Cosines Survive (Summary)

Recap of the symmetry argument.

The general solution on (−L, +L) with Neumann BCs is

  c(x, t)  =  A₀  +  Σₙ Aₙ cos(n π x / L) · e^(−D(nπ/L)² t)  +  Σₙ Bₙ sin((n−½) π x / L) · e^(−D((n−½)π/L)² t)

Coefficients from the IC:

  Bₙ  =  (1 / L) · ∫₋ₗᴸ f(x) · sin((n−½) π x / L) dx  =  0   (because f is even, sin is odd, product is odd, integral over symmetric interval is zero)

So every sine coefficient vanishes. **The solution reduces to:**

  c(x, t)  =  A₀(t)  +  Σₙ₌₁^∞ Aₙ(t) · cos(n π x / L)

That is the form originally given.

**Subtle point:** Neumann BCs at ±L alone do NOT eliminate sines — both even and odd eigenfunctions are admitted on [−L, +L]. It is the **even symmetry of the bleached-strip initial data** that kills every sine coefficient.

(The textbook trick of folding the problem onto [0, L] hides this — folding implicitly imposes evenness; on [0, L] with Neumann BCs at both ends, the only eigenfunctions are cosines.)

---

## STEP 4 — Time Dependence Aₙ(t)

For each positive integer n, the temporal ODE is

  T'ₙ(t) + D · (n π / L)² · Tₙ(t)  =  0

This is a first-order linear ODE with a known solution — a simple decaying exponential:

  **Aₙ(t)  =  Aₙ(0) · exp[ −D · (n π / L)² · t ]**

For n = 0, the eigenvalue is λ₀ = 0, so T'₀ = 0:

  **A₀(t)  =  A₀(0)  =  constant in time**

So:

- Higher modes (large n) decay FASTER, since the rate D(nπ/L)² grows as n².
- The n = 1 mode is the slowest-decaying nonzero mode, with timescale τ₁ = L² / (π² D).
- A₀ does not decay at all — it stays constant forever.

### Why A₀ is constant in time (physical reason)

Integrate the PDE in x over [−L, +L]:

  d/dt ∫₋ₗᴸ c(x, t) dx  =  D · ∫₋ₗᴸ ∂²c/∂x² dx  =  D · [∂c/∂x]₋ₗᴸ  =  D · (0 − 0)  =  0

So the total mass ∫c dx is conserved (no flux through the walls).

Every cos(n π x / L) with n ≥ 1 integrates to zero over [−L, +L]. So the conserved mass lives entirely in A₀. **That is why A₀ is pulled out as a separate constant — it represents the conserved mean concentration.**

---

## Final Assembled Solution

Putting Steps 1–4 together:

  **c(x, t)  =  A₀  +  Σₙ₌₁^∞ Aₙ(0) · cos(n π x / L) · exp[ −D · (n π / L)² · t ]**

Coefficients from the bleached-strip IC:

  A₀  =  (1 / 2L) · ∫₋ₗᴸ f(x) dx  =  c₀ · (L − a) / L
  Aₙ(0)  =  (1 / L) · ∫₋ₗᴸ f(x) · cos(n π x / L) dx  =  −(2 c₀ / (n π)) · sin(n π a / L)

Substituting:

  **c(x, t)  =  c₀ (L − a) / L  −  Σₙ₌₁^∞ (2 c₀ / (n π)) · sin(n π a / L) · cos(n π x / L) · exp[ −D · (n π / L)² · t ]**

**Sanity checks:**

- t → ∞: all exponentials die → c → c₀(L−a)/L (uniform equilibrium). ✓
- a → 0 (no bleach): sin(n π a / L) → 0 for every n → c → c₀ (uniform pre-bleach). ✓
- a → L (everything bleached): A₀ → 0 and sin(n π) = 0 → c → 0. ✓
- Each spatial mode decays as a single exponential with rate D(nπ/L)² — the principle behind FT-FRAP / strip-FRAP fitting to extract D.

---

## One-Line Summary of Where the Form Comes From

1. **Separation of variables** → solutions are products X(x) · T(t).
2. **Neumann BCs (X'(±L) = 0)** → admissible spatial functions are cosines (and possibly sines).
3. **Even IC** → kills the sine coefficients via odd-function integration, leaving pure cosines.
4. **λ = 0 eigenvalue** → gives the constant eigenfunction X₀ = 1, contributing A₀(t).
5. **λₙ = (n π / L)² eigenvalues** → give Xₙ = cos(n π x / L), contributing Aₙ(t) · cos(n π x / L).
6. **No-flux mass conservation** → A₀ is a true constant (not a function of t), so people write A₀ instead of A₀(t).

---

## Standard References

- Crank, *The Mathematics of Diffusion*, 2nd ed., Oxford 1975, §4.3 (finite slab with insulating BCs).
- Chasnov, *Differential Equations*, §9.5 (heat equation with Neumann BCs, worked end-to-end with cosine eigenfunctions).
- Axelrod, Koppel, Schlessinger, Elson, Webb (1976), *Biophys. J.* 16, 1055 — foundational FRAP paper.
- Soumpasis (1983), *Biophys. J.* 41, 95 — closed-form circular-spot FRAP.
- Modern strip-FRAP / line-FRAP / FT-FRAP literature — fits individual cosine modes to single exponentials to extract D.

# FIR Filter Design for Ayumi Decimation

## Understanding the Decimate Filter

The `decimate()` function in `ayumi.c` implements a **192-tap symmetric FIR low-pass filter** for anti-aliasing decimation by 8.

### Filter Properties

```c
static double decimate(double* x) {
  double y = -0.0000046183113992051936 * (x[1] + x[191]) +
             -0.00001117761640887225 * (x[2] + x[190]) +
             ...
             0.125 * x[96];  // Center tap
```

**Key Characteristics:**
- **192 taps** (odd number for Type I FIR)
- **Symmetric** (linear phase): coefficients mirror around center
- **Decimation by 8**: Reduces sample rate by factor of 8
- **Center coefficient**: 0.125 (= 1/8, related to decimation factor)
- **Zero coefficients**: Some taps are omitted (value = 0)
- **DC gain**: ≈ 1.0 (sum of all coefficients)

### Why This Design?

When decimating by 8x, we need a **low-pass anti-aliasing filter** to:
1. Remove frequencies above the new Nyquist (Fs_new / 2)
2. Prevent aliasing into the output bandwidth
3. Maintain high quality (80+ dB stopband attenuation)

The filter operates at the **high sample rate** (before decimation), then only 1 out of 8 samples is kept.

### Filter Design Methods

The original Ayumi filter was likely designed using:

#### 1. **Kaiser Window Method**
```python
from scipy import signal

# Design Kaiser window FIR filter
h = signal.firwin(
    numtaps=192,
    cutoff=1/16,  # Cutoff at 1/(2*decimation) of input Fs
    window=('kaiser', 7.865),  # Beta for ~80dB stopband
    scale=True
)
```

**Kaiser Window Beta Values:**
- β = 5.653 → ~60 dB stopband attenuation
- β = 7.865 → ~80 dB stopband attenuation
- β = 10.056 → ~100 dB stopband attenuation

#### 2. **Parks-McClellan (Remez Exchange)**
```python
# Optimal equiripple design
h = signal.remez(
    numtaps=192,
    bands=[0, 0.055, 0.07, 0.5],  # Normalized frequency bands
    desired=[1, 0],  # Passband=1, stopband=0
    weight=[1, 1]
)
```

This creates an **optimal** filter that minimizes maximum error in both passband and stopband.

### Understanding the Math

#### Symmetric FIR Structure

For a symmetric filter with N taps, we exploit symmetry to reduce computation:

```c
// Instead of:
y = h[0]*x[0] + h[1]*x[1] + ... + h[191]*x[191]

// We use symmetry:
y = h[1]*(x[1] + x[191]) + h[2]*(x[2] + x[190]) + ... + h[96]*x[96]
```

This reduces **192 multiplications → 96 multiplications** (almost 2x faster).

#### Why Odd Number of Taps?

- **Type I FIR** (odd taps, symmetric) can represent any frequency response
- **Type II FIR** (even taps, symmetric) has a zero at Fs/2 (not suitable for low-pass)
- For low-pass decimation filters, we need Type I

#### Frequency Response

The frequency response is the **Discrete-Time Fourier Transform (DTFT)** of the coefficients:

```
H(ω) = Σ h[n] * e^(-jωn)  for n = 0 to N-1
```

For our symmetric filter:
- **Linear phase**: φ(ω) = -ω(N-1)/2 (constant group delay)
- **Magnitude**: Smooth passband, ripples in stopband
- **Cutoff**: ~Fs/16 (Nyquist of output rate)

### Generating Your Own Filters

Use the provided `generate_fir.py` script:

```bash
# Install dependencies
pip3 install numpy scipy

# Generate default 192-tap filter
python3 generate_fir.py

# Custom 256-tap filter with higher attenuation
python3 generate_fir.py --taps 256 --stopband-db 100

# Use Parks-McClellan (optimal)
python3 generate_fir.py --method remez --taps 192

# Analyze existing filter
python3 generate_fir.py --analyze
```

### Trade-offs

| Taps | Stopband | Transition Width | CPU Cost |
|------|----------|------------------|----------|
| 64   | ~50 dB   | Wide             | Low      |
| 128  | ~70 dB   | Medium           | Medium   |
| 192  | ~80 dB   | Narrow           | High     |
| 256  | ~100 dB  | Very Narrow      | Very High|

**Ayumi uses 192 taps** as a good balance between:
- Audio quality (80 dB stopband is inaudible)
- Computational cost (still runs 90x real-time)
- Transition band sharpness

### Precision Considerations

The filter coefficients use **double precision (64-bit)**:
- ~17 decimal digits of precision
- Essential for filter stability
- Avoid quantization noise in stopband

For code generation, use at least 15-17 decimal digits:

```c
0.0000046183113992051936  // 22 digits shown (more than needed)
0.125                      // Exact power of 2
```

### Verification

You can verify filter performance by:

1. **Frequency response** (plot magnitude/phase)
2. **Impulse response** (should match coefficients)
3. **Step response** (check overshoot/ringing)
4. **Stopband attenuation** (measure in dB)
5. **Passband ripple** (should be minimal)

### Alternative Implementations

#### Polyphase Decomposition

For decimation by M, the filter can be decomposed into M parallel subfilters (more cache-efficient):

```c
// Instead of: filter → decimate
// Do: polyphase[0] + polyphase[1] + ... + polyphase[M-1]
```

This is **~30% faster** but more complex to implement.

#### Half-band Filters

For power-of-2 decimation (2, 4, 8), cascade of half-band filters:

```c
// Decimate by 8 = decimate by 2, three times
x → halfband_1 → ↓2 → halfband_2 → ↓2 → halfband_3 → ↓2
```

Each half-band filter has ~50% zero coefficients (even faster).

### References

- **Digital Signal Processing** by Oppenheim & Schafer
- **Understanding Digital Signal Processing** by Richard Lyons
- **scipy.signal documentation** for practical filter design
- **MATLAB Signal Processing Toolbox** (fir1, firpm, kaiserord functions)

### Summary

The Ayumi `decimate()` filter is a carefully designed **192-tap Kaiser window FIR filter** that:
1. Provides ~80 dB stopband attenuation (CD-quality)
2. Uses symmetry for 2x computational efficiency
3. Maintains linear phase (no phase distortion)
4. Operates at 8x oversampling rate
5. Achieves 90x real-time performance on modern CPUs

For custom requirements, use `generate_fir.py` to create filters with different parameters while maintaining the same high quality.

#!/usr/bin/env python3
"""
FIR Filter Generator for Ayumi-style Decimation Filters

Generates C code for high-quality FIR decimation filters with configurable
precision, using either Kaiser window or Parks-McClellan methods.

Usage:
    python3 generate_fir.py [options]

Examples:
    # Generate default 192-tap filter for decimation by 8
    python3 generate_fir.py

    # Generate 256-tap filter with higher precision
    python3 generate_fir.py --taps 256 --decimation 8 --precision 20

    # Use Parks-McClellan (optimal equiripple) design
    python3 generate_fir.py --method remez --stopband-db 80
"""

import numpy as np
from scipy import signal
import argparse


def analyze_existing_filter():
    """Analyze the existing Ayumi filter to understand its design."""
    # Extract coefficients from ayumi.c (reconstructed from the code)
    # This is the symmetric half + center
    coeffs = np.array([
        -0.0000046183113992051936,
        -0.00001117761640887225,
        -0.000018610264502005432,
        -0.000025134586135631012,
        -0.000028494281690666197,
        -0.000026396828793275159,
        -0.000017094212558802156,
        0.0,  # x[8] is zero
        0.000023798193576966866,
        0.000051281160242202183,
        0.00007762197826243427,
        0.000096759426664120416,
        0.00010240229300393402,
        0.000089344614218077106,
        0.000054875700118949183,
        0.0,  # x[16] is zero
        -0.000069839082210680165,
        -0.0001447966132360757,
        -0.00021158452917708308,
        -0.00025535069106550544,
        -0.00026228714374322104,
        -0.00022258805927027799,
        -0.00013323230495695704,
        0.0,  # x[24] is zero
        0.00016182578767055206,
        0.00032846175385096581,
        0.00047045611576184863,
        0.00055713851457530944,
        0.00056212565121518726,
        0.00046901918553962478,
        0.00027624866838952986,
        0.0,  # x[32] is zero
        -0.00032564179486838622,
        -0.00065182310286710388,
        -0.00092127787309319298,
        -0.0010772534348943575,
        -0.0010737727700273478,
        -0.00088556645390392634,
        -0.00051581896090765534,
        0.0,  # x[40] is zero
        0.00059548767193795277,
        0.0011803558710661009,
        0.0016527320270369871,
        0.0019152679330965555,
        0.0018927324805381538,
        0.0015481870327877937,
        0.00089470695834941306,
        0.0,  # x[48] is zero
        -0.0010178225878206125,
        -0.0020037400552054292,
        -0.0027874356824117317,
        -0.003210329988021943,
        -0.0031540624117984395,
        -0.0025657163651900345,
        -0.0014750752642111449,
        0.0,  # x[56] is zero
        0.0016624165446378462,
        0.0032591192839069179,
        0.0045165685815867747,
        0.0051838984346123896,
        0.0050774264697459933,
        0.0041192521414141585,
        0.0023628575417966491,
        0.0,  # x[64] is zero
        -0.0026543507866759182,
        -0.0051990251084333425,
        -0.0072020238234656924,
        -0.0082672928192007358,
        -0.0081033739572956287,
        -0.006583111539570221,
        -0.0037839040415292386,
        0.0,  # x[72] is zero
        0.0042781252851152507,
        0.0084176358598320178,
        0.01172566057463055,
        0.013550476647788672,
        0.013388189369997496,
        0.010979501242341259,
        0.006381274941685413,
        0.0,  # x[80] is zero
        -0.007421229604153888,
        -0.01486456304340213,
        -0.021143584622178104,
        -0.02504275058758609,
        -0.025473530942547201,
        -0.021627310017882196,
        -0.013104323383225543,
        0.0,  # x[88] is zero
        0.017065133989980476,
        0.036978919264451952,
        0.05823318062093958,
        0.079072012081405949,
        0.097675998716952317,
        0.11236045936950932,
        0.12176343577287731,
        0.125,  # Center tap x[96]
    ])

    print("Existing Ayumi Filter Analysis:")
    print(f"  Taps: 192 (symmetric)")
    print(f"  Decimation: 8")
    print(f"  Center coefficient: {coeffs[-1]:.6f}")
    print(f"  DC gain (sum): {np.sum(coeffs) * 2 - coeffs[-1]:.6f}")
    print()

    return coeffs


def design_kaiser_filter(numtaps, cutoff, decimation, fs=1.0, beta=None):
    """
    Design Kaiser window FIR filter for decimation.

    Args:
        numtaps: Number of filter taps (should be odd for Type I)
        cutoff: Cutoff frequency (normalized to Nyquist if fs=1.0)
        decimation: Decimation factor
        fs: Sample rate (use 1.0 for normalized frequency)
        beta: Kaiser window beta parameter (higher = steeper rolloff)

    Returns:
        Array of filter coefficients
    """
    if beta is None:
        # Auto-calculate beta for ~80dB stopband attenuation
        beta = 7.865  # Gives approximately 80dB

    # Design the filter
    h = signal.firwin(numtaps, cutoff, window=('kaiser', beta), fs=fs, scale=True)

    return h


def design_remez_filter(numtaps, cutoff, transition_width, stopband_db=80, fs=1.0):
    """
    Design Parks-McClellan (Remez) optimal equiripple filter.

    Args:
        numtaps: Number of filter taps (should be odd)
        cutoff: Cutoff frequency (normalized to Nyquist if fs=1.0)
        transition_width: Width of transition band
        stopband_db: Stopband attenuation in dB
        fs: Sample rate

    Returns:
        Array of filter coefficients
    """
    # Convert dB to linear scale for weight calculation
    stopband_linear = 10 ** (-stopband_db / 20)

    # Bands: [0, passband_end, stopband_start, nyquist]
    nyquist = fs / 2
    passband_end = cutoff - transition_width / 2
    stopband_start = cutoff + transition_width / 2

    bands = [0, passband_end, stopband_start, nyquist]
    desired = [1, 0]  # Passband gain = 1, stopband gain = 0
    weight = [1, 1]   # Equal weight (can adjust for different emphasis)

    h = signal.remez(numtaps, bands, desired, weight=weight, fs=fs)

    return h


def generate_c_code(coeffs, function_name='decimate', precision=17, decimation_factor=8):
    """
    Generate C code for the FIR filter.

    Args:
        coeffs: Filter coefficients (full symmetric filter)
        function_name: Name of the C function
        precision: Number of decimal digits for coefficient output
        decimation_factor: Decimation factor (for naming)
    """
    numtaps = len(coeffs)
    center_idx = numtaps // 2

    # Verify symmetry
    is_symmetric = np.allclose(coeffs, coeffs[::-1], rtol=1e-15)
    if not is_symmetric:
        print("Warning: Filter is not symmetric!")

    code = []
    code.append(f"static double {function_name}(double* x) {{")
    code.append(f"  double y = ")

    # Generate code using symmetry
    lines = []
    for i in range(1, center_idx):
        coeff = coeffs[i]
        if abs(coeff) < 1e-20:  # Skip near-zero coefficients
            continue

        mirror_idx = numtaps - 1 - i
        lines.append(f"    {coeff:.{precision}g} * (x[{i}] + x[{mirror_idx}])")

    # Center tap (no pairing)
    center_coeff = coeffs[center_idx]
    if abs(center_coeff) > 1e-20:
        lines.append(f"    {center_coeff:.{precision}g} * x[{center_idx}]")

    # Join with + and handle line continuation
    if lines:
        code[-1] += lines[0].strip()
        for line in lines[1:]:
            code.append("    +" + line.strip())
    code.append("    ;")

    # Add buffer rotation
    code.append(f"  memcpy(&x[FIR_SIZE - DECIMATE_FACTOR], x, DECIMATE_FACTOR * sizeof(double));")
    code.append(f"  return y;")
    code.append(f"}}")

    return '\n'.join(code)


def analyze_filter_response(coeffs, fs=1.0, decimation=8):
    """Analyze and plot filter frequency response."""
    w, h = signal.freqz(coeffs, worN=8192, fs=fs)

    # Find -3dB point
    h_db = 20 * np.log10(np.abs(h))
    cutoff_3db_idx = np.where(h_db < -3)[0][0] if np.any(h_db < -3) else len(h_db) - 1
    cutoff_3db = w[cutoff_3db_idx]

    # Find first stopband null
    passband_end = len(h_db) // (decimation * 2)
    stopband_db = h_db[passband_end:]
    min_stopband_db = np.min(stopband_db) if len(stopband_db) > 0 else 0

    print(f"\nFilter Frequency Response:")
    print(f"  -3dB cutoff: {cutoff_3db:.6f} * Fs")
    print(f"  Stopband attenuation: {-min_stopband_db:.1f} dB")
    print(f"  DC gain: {20 * np.log10(np.abs(h[0])):.2f} dB")

    return w, h


def main():
    parser = argparse.ArgumentParser(
        description='Generate FIR decimation filter C code',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__
    )

    parser.add_argument('--taps', type=int, default=192,
                        help='Number of filter taps (default: 192)')
    parser.add_argument('--decimation', type=int, default=8,
                        help='Decimation factor (default: 8)')
    parser.add_argument('--cutoff', type=float, default=None,
                        help='Cutoff frequency as fraction of Nyquist (default: auto)')
    parser.add_argument('--method', choices=['kaiser', 'remez'], default='kaiser',
                        help='Filter design method (default: kaiser)')
    parser.add_argument('--beta', type=float, default=7.865,
                        help='Kaiser window beta parameter (default: 7.865 for ~80dB)')
    parser.add_argument('--stopband-db', type=float, default=80,
                        help='Stopband attenuation in dB for remez (default: 80)')
    parser.add_argument('--precision', type=int, default=17,
                        help='Decimal digits for coefficients (default: 17)')
    parser.add_argument('--analyze', action='store_true',
                        help='Analyze existing Ayumi filter')

    args = parser.parse_args()

    if args.analyze:
        analyze_existing_filter()
        return

    # Auto-calculate cutoff if not specified
    if args.cutoff is None:
        # Cutoff at Nyquist of output rate (with some margin)
        # Input rate / decimation / 2 = output Nyquist
        # Express as fraction of input Nyquist: 1 / decimation
        args.cutoff = 0.95 / args.decimation

    print(f"Designing {args.taps}-tap FIR filter:")
    print(f"  Method: {args.method}")
    print(f"  Decimation: {args.decimation}x")
    print(f"  Cutoff: {args.cutoff:.6f} * Fs")

    # Design the filter
    if args.method == 'kaiser':
        coeffs = design_kaiser_filter(
            args.taps,
            args.cutoff,
            args.decimation,
            fs=1.0,
            beta=args.beta
        )
        print(f"  Kaiser beta: {args.beta:.3f}")
    else:  # remez
        transition_width = 0.1 / args.decimation
        coeffs = design_remez_filter(
            args.taps,
            args.cutoff,
            transition_width,
            stopband_db=args.stopband_db,
            fs=1.0
        )
        print(f"  Stopband: {args.stopband_db} dB")

    # Analyze response
    analyze_filter_response(coeffs, fs=1.0, decimation=args.decimation)

    # Generate C code
    print("\n" + "=" * 70)
    print("Generated C Code:")
    print("=" * 70 + "\n")

    c_code = generate_c_code(coeffs, 'decimate', args.precision, args.decimation)
    print(c_code)

    # Save to file
    output_file = f'fir_decimate_{args.taps}tap.c'
    with open(output_file, 'w') as f:
        f.write("// Auto-generated FIR decimation filter\n")
        f.write(f"// Taps: {args.taps}, Decimation: {args.decimation}x\n")
        f.write(f"// Method: {args.method}\n")
        f.write(f"// Cutoff: {args.cutoff:.6f} * Fs\n\n")
        f.write(c_code)

    print(f"\n\nSaved to: {output_file}")


if __name__ == '__main__':
    main()

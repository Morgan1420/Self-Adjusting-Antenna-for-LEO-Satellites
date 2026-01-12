import numpy as np
import matplotlib.pyplot as plt
from scipy import signal
import argparse

def plot_from_disk(filename):
    fs = 4_000_000 
    fc = 2_491_500_000

    # 1. Memory Map the file (This doesn't use RAM!)
    # 'i2' means 16-bit integers (int16)
    mm = np.memmap(filename, dtype='i2', mode='r')
    
    # Each sample is 2 integers (I and Q), so total samples is:
    total_samples = len(mm) // 2
    print(f"📂 File contains {total_samples} samples.")

    # 2. Pick a "Slice" to look at (e.g., the first 0.1 seconds)
    # This prevents the plotting math from freezing your CPU
    plot_seconds = 0.1 
    num_to_plot = int(plot_seconds * fs)
    
    # Grab just the slice we want
    raw_slice = mm[:num_to_plot * 2] 
    
    # 3. Convert only the slice to Complex IQ
    i = raw_slice[0::2].astype(np.float32)
    q = raw_slice[1::2].astype(np.float32)
    iq = (i + 1j * q) / 2048.0

    # 4. Generate Visuals
    print("📊 Plotting...")
    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(10, 10))

    # Power Spectral Density
    f, Pxx = signal.welch(iq, fs, nperseg=2048)
    f = np.fft.fftshift(f) + fc
    Pxx = np.fft.fftshift(Pxx)
    ax1.semilogy(f/1e9, Pxx)
    ax1.set_title("Frequency Spectrum")
    ax1.set_xlabel("GHz")
    ax1.grid(True)

    # Spectrogram (Waterfall)
    f_s, t_s, Sxx = signal.spectrogram(iq, fs, nperseg=1024)
    f_s = np.fft.fftshift(f_s) + fc
    Sxx = np.fft.fftshift(Sxx, axes=0)
    ax2.pcolormesh(t_s, f_s/1e9, 10 * np.log10(Sxx), shading='gouraud', cmap='viridis')
    ax2.set_title("Waterfall (Time vs Frequency)")
    ax2.set_ylabel("GHz")

    plt.tight_layout()
    plt.show()

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("file", type=str)
    args = parser.parse_args()
    plot_from_disk(args.file)
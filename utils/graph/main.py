import io
import os
import matplotlib.pyplot as plt
import re
from numpy.fft import rfft, rfftfreq
from numpy import abs
from math import tau

pwd = os.path.dirname(__file__)

samples_per_second = 25

def get_series_avg(series):
    result = list(series)
    for i in range(3, len(series) - 3):
        result[i] = sum(series[i - 3:i + 4]) / 7
    return result

def get_series_high_pass(series, cutoff_bpm):
    series = list(series)
    avg = sum(series) / len(series)
    rc = 1 / (tau * (cutoff_bpm / 60))
    alpha = rc / (rc + 1 / samples_per_second)
    result = [series[0]]
    for i in range(1, len(series)):
        result.append(alpha * (result[i - 1] - avg + series[i] - series[i - 1]) + avg)
    return result

def get_series_fft(series):
    series = list(series)
    avg = sum(series) / len(series)
    for i in range(len(series)):
        series[i] -= avg
    return list(map(abs, rfft(series)))

def get_fft_band_pass(series, cutoff_bpm_low, decay_bps_high, n_samples):
    series = list(series)
    rc_low = 1 / (tau * (cutoff_bpm_low / 60))
    freqs = rfftfreq(n_samples, 1 / samples_per_second)
    for (i, frequency) in enumerate(freqs):
        low_pass_scale = frequency * rc_low
        low_pass_scale /= 1 + low_pass_scale
        high_pass_scale = decay_bps_high ** frequency
        series[i] *= low_pass_scale * high_pass_scale
    return list(series)

def main():
    with io.open(f"{pwd}/samples.txt") as file:
        samples = list(map(lambda line: list(map(int, re.sub("^E \\(\\d+\\) \\S+: ", "", line).split(" "))), file.readlines()))
        if len(samples) == 0 or len(samples[0]) == 0:
            return

        orig_series = [[samples_[i] for samples_ in samples] for i in range(len(samples[0]))]
        avg_series = [get_series_avg(series_) for series_ in orig_series]
        high_pass_series = [get_series_high_pass(series_, 5) for series_ in orig_series]

        fft_series = [get_series_fft(series_) for series_ in orig_series]
        fft_band_pass_series = [get_fft_band_pass(series_, 25, 0.6, len(samples)) for series_ in fft_series]

        sample_series_sets = [orig_series, avg_series, high_pass_series]
        sample_series_set_names = ["Original", "Averaged (7-sample window)", "High-pass filter (cutoff freq = 1)"]
        freq_series_sets = [fft_series, fft_band_pass_series]
        freq_series_set_names = ["FFT", "FFT (band-pass)"]

        series_names = ["Red", "IR"]

        for series_set in sample_series_sets + freq_series_sets:
            print(series_set)
            series0_sum = sum(series_set[0])
            series_scales = [1] + [series0_sum / sum(series_set[i]) for i in range(1, len(series_set))]
            for series_, scale in zip(series_set, series_scales):
                for i in range(len(series_)):
                    series_[i] *= scale

        n_plots = len(sample_series_sets) + len(freq_series_sets)
        for i, (series_set, series_set_name) in enumerate(zip(sample_series_sets, sample_series_set_names)):
            plt.subplot(n_plots, 1, i + 1)
            plt.title(series_set_name)
            for (series, series_name) in zip(series_set, series_names):
                plt.plot(
                    [j / samples_per_second for j in range(len(series))],
                    series,
                    label=series_name
                )
            plt.legend()
        for i, (series_set, series_set_name) in enumerate(zip(freq_series_sets, freq_series_set_names)):
            plt.subplot(n_plots, 1, i + 1 + len(sample_series_sets))
            plt.title(series_set_name)
            for (series, series_name) in zip(series_set, series_names):
                plt.plot(
                    list(map(lambda v: v * 60, rfftfreq(len(samples), 1 / samples_per_second))),
                    series,
                    label=series_name
                )
            plt.legend()
        plt.tight_layout()
        plt.show()

main()

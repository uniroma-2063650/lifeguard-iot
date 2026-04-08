import io
import os
import matplotlib.pyplot as plt

pwd = os.path.dirname(__file__)

samples_per_second = 50

def main():
    with io.open(f"{pwd}/samples.txt") as file:
        values = list(map(lambda line: list(map(int, line.split(" "))), file.readlines()))
        if len(values) == 0 or len(values[0]) == 0:
            return

        n_series = len(values[0])
        series0_sum = sum(value[0] for value in values)
        series_scales = [series0_sum / sum(value[i] for value in values) for i in range(1, n_series)]
        for i in range(1, n_series):
            for value in values:
                value[i] *= series_scales[i - 1]

        plt.plot([i / samples_per_second for i in range(len(values))], values)
        plt.show()

main()

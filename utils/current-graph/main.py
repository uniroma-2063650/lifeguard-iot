import matplotlib.pyplot as plt
import pandas as pd

df = pd.read_csv("out.csv")
df["Time"] = pd.to_timedelta(df["Time"]).map(lambda t: t.total_seconds())
df["Time"] -= df["Time"][0]

average = df['Current'].mean()

plt.figure(figsize=(10, 5))
plt.ticklabel_format(style="plain", useOffset=False)
plt.plot(df["Time"], df["Current"])
plt.axhline(y=average, color='red', linestyle='--', label=f'Average: {average:.2f} mA')
plt.title("Current usage over time")
plt.xlabel("Time (s)")
plt.ylabel("Current (mA)")
plt.grid(True)
plt.xticks(rotation=45)
plt.tight_layout()
plt.legend()
plt.show()

import matplotlib.pyplot as plt
import pandas as pd

df = pd.read_csv("out.csv")
df["Time"] = pd.to_timedelta(df["Time"]).map(lambda t: t.total_seconds())
df["Time"] -= df["Time"][0]

average = df['Current'].mean()

fig, ax = plt.subplots(figsize=(10, 5))
ax.ticklabel_format(style="plain", useOffset=False)
ax.plot(df["Time"], df["Current"])
ax.axhline(y=average, color='red', linestyle='--', label=f'Average: {average:.2f} mA')
ax.set_title("Current usage over time")
ax.set_xlabel("Time (s)")
ax.set_ylabel("Current (mA)")
ax.grid(True)
fig.tight_layout()
fig.legend()

def on_resize(event):
    fig.tight_layout()
    fig.canvas.draw()

cid = fig.canvas.mpl_connect("resize_event", on_resize)
plt.show()

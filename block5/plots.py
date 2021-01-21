import csv
import matplotlib.pyplot as plt
import numpy as np

num = np.arange(100)
offset = np.zeros(100)
delay = np.zeros(100)
rootDisp = np.zeros(100)

with open('results.csv', newline='') as csvfile:
    spamreader = csv.reader(csvfile, delimiter=';')
    for row in spamreader:
        i = int(row[1])
        rootDisp[i] = float(row[2])
        delay[i] = float(row[4])
        offset[i] = float(row[5])

fig, axs = plt.subplots(3, 1, figsize=(9,18))
axs[0].plot(num, delay)
axs[1].plot(num, offset)
axs[2].plot(num, rootDisp)

# set label
axs[0].set_xlabel("Requests")
axs[0].set_ylabel("Delay")
axs[1].set_xlabel("Requests")
axs[1].set_ylabel("Offset")
axs[2].set_xlabel("Requests")
axs[2].set_ylabel("Root Dispersion")
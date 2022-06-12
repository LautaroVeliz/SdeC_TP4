import matplotlib.pyplot as plt
import numpy as np
from matplotlib.animation import FuncAnimation
from matplotlib.widgets import Button
import psutil
import collections

# function to update the data
def update_function(i):
    # get data
    data.popleft()
    data.append(signal_func())
    # clear axis
    ax.cla()
    # plot data
    ax.plot(data)
    ax.scatter(len(data)-1, data[-1])
    ax.text(len(data)-1, data[-1]+2, "{}%".format(data[-1]))
    ax.set_ylim(0,100)

def ram_signal (event):
    global data
    global signal_func
    data = collections.deque(np.zeros(10))
    signal_func = update_ram
    
def cpu_signal (event):
    global data
    global signal_func
    data = collections.deque(np.zeros(10))
    signal_func = psutil.cpu_percent

def update_ram():
    return psutil.virtual_memory().percent

signal_func = psutil.cpu_percent
# start collections with zeros
data = collections.deque(np.zeros(10))
# define and adjust figure
fig, ax = plt.subplots()
ax.set_facecolor('#DEDEDE')
# animate

axcpu = plt.axes([0.7, 0.05, 0.1, 0.075])
axram = plt.axes([0.81, 0.05, 0.1, 0.075])
bcpu = Button(axcpu, 'CPU')
bcpu.on_clicked(cpu_signal)
bram = Button(axram, 'RAM')
bram.on_clicked(ram_signal)

ani = FuncAnimation(fig, update_function, interval=100)
plt.show()
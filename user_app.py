import matplotlib.pyplot as plt
import numpy as np
from matplotlib.animation import FuncAnimation
from matplotlib.widgets import Button
import collections

# function to toggle signal
def toggle_signal (event):
    global data
    data = collections.deque(np.zeros(30))
    f = open(fs, "w")
    f.write("a")
    f.close()

# function to read driver
def signal_func():
    f = open(fs, "r")
    valorLeido = int(f.read(),2)
    f.close()
    return valorLeido

# function to update the data
def update_function(i):
    # get data
    data.popleft()
    data.append(signal_func())
    # clear axis
    ax.cla()
    # plot data
    ax.plot(data)
    ax.scatter(len(data), data[-1]) #Punto al final de la curva
    ax.text(len(data), data[-1]+2, "{}".format(data[-1])) #Texto porcentaje
    ax.set_ylim(0,16)
    ax.yaxis.set_ticks(np.arange(0, 16, 1.0))

# initialize variable
fs = '/dev/raspGPIODr'
data = collections.deque(np.zeros(30))

# define and adjust figure
fig, ax = plt.subplots()
ax.get_xaxis().set_visible(False)
ax.set_facecolor('#DEDEDE')

# set button
ax_sel_button = plt.axes([0.90, 0.03, 0.1, 0.075])
sel_button = Button(ax_sel_button, 'SIG')
sel_button.on_clicked(toggle_signal)

# animate
ani = FuncAnimation(fig, update_function, interval=100)
plt.show()
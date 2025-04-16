import matplotlib.pyplot as plt
from scipy.interpolate import interp1d
import numpy as np
import os

save_path = os.path.join('statistics/')

latencies = [534.761782729805, 3968.7974612481676, 1929500.9790109408, 3239273.516787392, 4556604.576472163, 5443901.91488, 5724498.182945224, 4610043.358204739, 5395825.5450422745, 5577305.426104401, 5569395.986069864, 10221744.630996037]
n_vehicles = [10, 50, 100, 200, 400, 800, 1200, 1600, 2000, 2400, 2800, 3200]

plt.figure(figsize=(8, 6))
interpolation_function = interp1d(n_vehicles, latencies, kind='cubic', bounds_error=False, fill_value=(latencies[0], latencies[-1]))
plt.scatter(n_vehicles, latencies, s=60, color='red', marker='x', label='Latencies x n_vehicles') 

x_dense = np.linspace(min(n_vehicles), max(n_vehicles), 300)
y_dense = interpolation_function(x_dense)

plt.plot(x_dense, y_dense, '-', color='blue', label=f"{'CUBIC'} Interpolation") 

plt.title("Latencies x n_vehicles")
plt.xlabel("n_vehicles")
plt.ylabel("Latencies")
plt.grid(True)
try:
    plt.savefig(
        save_path,
        dpi=300,
        bbox_inches='tight'
        )
    print(f"Plot successfully saved to: {save_path}")
except Exception as e:
    print(f"Error saving plot: {e}")

plt.close()
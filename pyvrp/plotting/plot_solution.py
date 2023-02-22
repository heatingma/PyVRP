from typing import Optional

import matplotlib.pyplot as plt
import numpy as np

from pyvrp import Individual, ProblemData


def plot_solution(
    solution: Individual,
    data: ProblemData,
    plot_customers: bool = False,
    ax: Optional[plt.Axes] = None,
):
    """
    Plots the given solution.

    Parameters
    ----------
    solution
        Solution to plot.
    data
        Data instance underlying the solution.
    plot_customers, optional
        Whether to plot customers as dots. Default False, which plots only the
        solution's routes.
    ax, optional
        Axes object to draw the plot on. One will be created if not provided.
    """
    if not ax:
        _, ax = plt.subplots()

    dim = data.num_clients + 1
    x_coords = np.array([data.client(client).x for client in range(dim)])
    y_coords = np.array([data.client(client).y for client in range(dim)])

    # This is the depot
    kwargs = dict(c="tab:red", marker="*", zorder=3, s=500)
    ax.scatter(x_coords[0], y_coords[0], label="Depot", **kwargs)

    for idx, route in enumerate(solution.get_routes(), 1):
        if route:
            x = x_coords[route]
            y = y_coords[route]

            # Coordinates of customers served by this route.
            if len(route) == 1 or plot_customers:
                ax.scatter(x, y, label=f"Route {idx}", zorder=3, s=75)
            ax.plot(x, y)

            # Edges from and to the depot, very thinly dashed.
            kwargs = dict(ls=(0, (5, 15)), linewidth=0.25, color="grey")
            ax.plot([x_coords[0], x[0]], [y_coords[0], y[1]], **kwargs)
            ax.plot([x[-1], x_coords[0]], [y[-1], y_coords[0]], **kwargs)

    ax.grid(color="grey", linestyle="solid", linewidth=0.2)

    ax.set_title("Solution")
    ax.set_aspect("equal", "datalim")
    ax.legend(frameon=False, ncol=2)
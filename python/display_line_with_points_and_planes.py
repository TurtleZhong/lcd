import numpy as np
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d import Axes3D
from mpl_toolkits.mplot3d.art3d import Poly3DCollection, Line3DCollection
import yaml
import os

file_dir = os.path.dirname(os.path.abspath(__file__))


def get_plane(hessian, x, y):
    a, b, c, d = normalize_hessian(hessian)
    print("a={0}, b={1}, c={2}, d={3}".format(a, b, c, d))
    z = (-a * x - b * y - d) * 1. / c
    return z


def normalize_hessian(hessian):
    norm = np.linalg.norm(np.array(hessian[:3]))
    return hessian / norm


def get_mean_point(points):
    mean_point = np.zeros(3)
    for point in points:
        mean_point += point
    return mean_point / len(points)


def project_point_on_plane(hessian, point):
    a, b, c, d = normalize_hessian(hessian)
    normal = np.array([a, b, c])
    for non_zero in range(3):
        if abs(normal[non_zero]) > 0.1:
            break
    x_0 = np.empty(3)
    for i in range(3):
        if i == non_zero:
            x_0[i] = -d / hessian[non_zero]
        else:
            x_0[i] = 0
    return point - np.dot(point - x_0, normal) * normal


def plot():
    scale_factor = 0.1

    # Read data from YAML file
    with open(os.path.join(file_dir,
                           '../line_with_points_and_planes.yaml')) as f:
        data = yaml.load(f)

    # a*x+b*y+c*z+d = 0 => a*(scale_factor*x)+b*(scale_factor*y)+
    #                      c*(scale_factor*z)+d*(scale_factor) = 0
    # => d must be scaled by scaled_factor
    hessian_1 = normalize_hessian(np.array(data['hessians'][0]))
    hessian_1[-1] *= scale_factor
    hessian_1 = normalize_hessian(hessian_1)
    hessian_2 = normalize_hessian(np.array(data['hessians'][1]))
    hessian_2[-1] *= scale_factor
    hessian_2 = normalize_hessian(hessian_2)
    inliers_1 = np.array(data['inliers'][0]) * scale_factor
    inliers_2 = np.array(data['inliers'][1]) * scale_factor

    mean_point_1 = get_mean_point(inliers_1)
    print("Mean point 1 is {}".format(mean_point_1))
    mean_point_1_proj = project_point_on_plane(hessian_1, mean_point_1)

    mean_point_2 = get_mean_point(inliers_2)
    print("Mean point 2 is {}".format(mean_point_2))
    mean_point_2_proj = project_point_on_plane(hessian_2, mean_point_2)

    mean_of_means = get_mean_point([mean_point_1_proj, mean_point_2_proj])

    # Create grid for x and y
    min_x = int(np.floor(min(min(inliers_1[:, 0]), min(inliers_2[:, 0]))))
    #min_x = min(0, min_x)
    min_y = int(np.floor(min(min(inliers_1[:, 1]), min(inliers_2[:, 1]))))
    #min_y = min(0, min_y)
    max_x = int(np.ceil(max(max(inliers_1[:, 0]), max(inliers_2[:, 0]))))
    #max_x = max(0, max_x)
    max_y = int(np.ceil(max(max(inliers_1[:, 1]), max(inliers_2[:, 1]))))
    #max_y = max(0, max_y)
    x, y = np.meshgrid(np.linspace(min_x, max_x, 100), np.linspace(min_y, max_y, 100))

    plot_ = plt.figure().gca(projection='3d')

    # Plot line
    start = np.array(data['start']) * scale_factor
    end = np.array(data['end']) * scale_factor
    plot_.plot([start[0], end[0]], [start[1], end[1]], [start[2], end[2]], 'r',
               label='Line after adjustment through inliers.')
    start_guess = np.array(data['start_guess']) * scale_factor
    end_guess = np.array(data['end_guess']) * scale_factor
    plot_.plot([start_guess[0], end_guess[0]], [start_guess[1], end_guess[1]],
               [start_guess[2], end_guess[2]], 'b',
               label='Guess before adjustment through inliers.')

    # Plot first plane
    #plot_.plot_surface(
    #    x, y, get_plane(hessian_1, x, y), alpha=0.9, rcount=1, ccount=1)
    # Plot second plane
    #plot_.plot_surface(
    #    x, y, get_plane(hessian_2, x, y), alpha=0.9, rcount=1, ccount=1)

    # Plot inliers for first plane
    plot_.scatter(inliers_1[:, 0], inliers_1[:, 1], inliers_1[:, 2])
    # Plot inliers for second plane
    plot_.scatter(inliers_2[:, 0], inliers_2[:, 1], inliers_2[:, 2])
    # Plot first projected mean
    plot_.scatter(
        mean_point_1_proj[0],
        mean_point_1_proj[1],
        mean_point_1_proj[2],
        c='red')
    # Plot second projected mean
    plot_.scatter(
        mean_point_2_proj[0],
        mean_point_2_proj[1],
        mean_point_2_proj[2],
        c='red')
    # Plot origin
    #plot_.scatter(0,0,0,c='blue')
    # Plot mean of mean points
    plot_.scatter(
        mean_of_means[0], mean_of_means[1], mean_of_means[2], c='green')
    # Plot lines to origin
    plot_.quiver(
        mean_point_1_proj[0],
        mean_point_1_proj[1],
        mean_point_1_proj[2],
        -mean_point_1_proj[0],
        -mean_point_1_proj[1],
        -mean_point_1_proj[2],
        colors='green')
    plot_.quiver(
        mean_point_2_proj[0],
        mean_point_2_proj[1],
        mean_point_2_proj[2],
        -mean_point_2_proj[0],
        -mean_point_2_proj[1],
        -mean_point_2_proj[2],
        colors='green')
    # Add legend
    plot_.legend()
    # Plot everything
    plt.show()

    return data


if __name__ == '__main__':
    data = plot()
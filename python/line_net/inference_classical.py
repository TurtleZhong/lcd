import numpy as np
import datagenerator_framewise
import losses_and_metrics

from numba import jit, njit

from sklearn.cluster import AgglomerativeClustering


def closest_distance_between_lines(a0, a1, b0, b1,
                                   clampAll=True, clampA0=False, clampA1=False, clampB0=False, clampB1=False):

    ''' Given two lines defined by numpy.array pairs (a0,a1,b0,b1)
        Return the closest points on each segment and their distance
        Taken from https://stackoverflow.com/questions/2824478/shortest-distance-between-two-line-segments
    '''

    # If clampAll=True, set all clamps to True
    if clampAll:
        clampA0=True
        clampA1=True
        clampB0=True
        clampB1=True

    # Calculate denomitator
    A = a1 - a0
    B = b1 - b0
    magA = np.linalg.norm(A)
    magB = np.linalg.norm(B)

    _A = A / magA
    _B = B / magB

    cross = np.cross(_A, _B);
    denom = np.linalg.norm(cross)**2

    # If lines are parallel (denom=0) test if lines overlap.
    # If they don't overlap then there is a closest point solution.
    # If they do overlap, there are infinite closest positions, but there is a closest distance
    if not denom:
        d0 = np.dot(_A,(b0-a0))

        # Overlap only possible with clamping
        if clampA0 or clampA1 or clampB0 or clampB1:
            d1 = np.dot(_A,(b1-a0))

            # Is segment B before A?
            if d0 <= 0 >= d1:
                if clampA0 and clampB1:
                    if np.absolute(d0) < np.absolute(d1):
                        return a0,b0,np.linalg.norm(a0-b0)
                    return a0,b1,np.linalg.norm(a0-b1)


            # Is segment B after A?
            elif d0 >= magA <= d1:
                if clampA1 and clampB0:
                    if np.absolute(d0) < np.absolute(d1):
                        return a1,b0,np.linalg.norm(a1-b0)
                    return a1,b1,np.linalg.norm(a1-b1)

        # Segments overlap, return distance between parallel segments
        return None,None,np.linalg.norm(((d0*_A)+a0)-b0)

    # Lines criss-cross: Calculate the projected closest points
    t = (b0 - a0);
    detA = np.linalg.det([t, _B, cross])
    detB = np.linalg.det([t, _A, cross])

    t0 = detA/denom;
    t1 = detB/denom;

    pA = a0 + (_A * t0) # Projected closest point on segment A
    pB = b0 + (_B * t1) # Projected closest point on segment B

    # Clamp projections
    if clampA0 or clampA1 or clampB0 or clampB1:
        if clampA0 and t0 < 0:
            pA = a0
        elif clampA1 and t0 > magA:
            pA = a1

        if clampB0 and t1 < 0:
            pB = b0
        elif clampB1 and t1 > magB:
            pB = b1

        # Clamp projection A
        if (clampA0 and t0 < 0) or (clampA1 and t0 > magA):
            dot = np.dot(_B,(pA-b0))
            if clampB0 and dot < 0:
                dot = 0
            elif clampB1 and dot > magB:
                dot = magB
            pB = b0 + (_B * dot)

        # Clamp projection B
        if (clampB0 and t1 < 0) or (clampB1 and t1 > magB):
            dot = np.dot(_A,(pB-a0))
            if clampA0 and dot < 0:
                dot = 0
            elif clampA1 and dot > magA:
                dot = magA
            pA = a0 + (_A * dot)

    return pA,pB,np.linalg.norm(pA-pB)





def compute_distance_matrix(lines, valid_mask):
    lines = lines[valid_mask, :]
    distance_matrix = np.zeros((lines.shape[0], lines.shape[0]))

    for i in range(lines.shape[0]):
        for j in range(lines.shape[0]):
            _, _, distance_matrix[i, j] = closest_distance_between_lines(lines[i, :3], lines[i, 3:6],
                                                                   lines[j, :3], lines[j, 3:6])

    return distance_matrix


if __name__ == '__main__':
    import tensorflow as tf
    tf.config.set_visible_devices([], 'GPU')

    # Paths to line files.
    val_files = "/nvme/line_ws/val"

    # The length of the geometry vector of a line.
    line_num_attr = 15
    img_shape = (64, 96, 3)
    max_line_count = 150
    batch_size = 1
    num_epochs = 40
    # Do not forget to delete pickle files when this config is changed.
    max_clusters = 15
    # TODO: Check if 0 is background or naw.
    bg_classes = [0, 1, 2, 20, 22]

    val_data_generator = datagenerator_framewise.LineDataSequence(val_files,
                                                                  batch_size,
                                                                  bg_classes,
                                                                  fuse=False,
                                                                  img_shape=img_shape,
                                                                  min_line_count=30,
                                                                  max_line_count=max_line_count,
                                                                  max_cluster_count=max_clusters)

    data = val_data_generator.__getitem__(0)[0]
    geometries = data['lines'][0]
    labels = data['labels']
    valid_mask = data['valid_input_mask']
    bg_mask = data['background_mask']
    cluster_count = data['cluster_count']
    unique_labels = data['unique_labels']

    dist_matrix = compute_distance_matrix(geometries, valid_mask[0])
    print(dist_matrix)
    num_clusters = 3
    agglo = AgglomerativeClustering(n_clusters=num_clusters, affinity='precomputed', linkage='single')
    agglo.fit(dist_matrix)
    computed_labels = agglo.labels_

    cluster_output = np.zeros((max_line_count, max_clusters + 1))
    cluster_output[valid_mask[0], computed_labels + 1] = 1.
    print(unique_labels[0])
    print(np.vstack([computed_labels, labels[0][valid_mask[0]]]).T)

    iou = losses_and_metrics.iou_metric(labels,
                                        unique_labels,
                                        cluster_count,
                                        bg_mask,
                                        valid_mask,
                                        max_clusters)
    print(np.array(iou(None, np.expand_dims(cluster_output, axis=0))))


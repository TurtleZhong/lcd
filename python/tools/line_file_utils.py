""" Utilities to store and read lines from .txt files.
"""


def read_line_detection_line(line):
    """
    Function used to read lines from the ROS node output.
    :param line: The vector of the line from the line files outputted by the ROS node.
    :return: A dictionary with all geometric and other data of the line: start point, end point, hessian 1, hessian 2
             colors 1, colors 2, type, label, class, normal 1, normal 2, open 1, open 2, camera origin, camera rotation
    """
    # WARNING: Changes here also need to happen in split_dataset_framewise.py

    # Lines are stored in format:
    # 0 - 2: start point
    # 3 - 5: end point
    # 6 - 9: hessian 1
    # 10-13: hessian 2
    # 14-16: colors 1
    # 17-19: colors 2
    #    20: type
    #    21: label
    #    22: class
    # 23-25: normal 1
    # 26-28: normal 2
    #    29: open 1
    #    30: open 2
    # 31-33: camera origin
    # 34-37: camera rotation
    return {
        'start_point': line[0:3],
        'end_point': line[3:6],
        'hessian_1': line[6:10],
        'hessian_2': line[10:14],
        'type': int(line[20]),
        'label': int(line[21]),
        'class': int(line[22]),
        'normal_1': line[23:26],
        'normal_2': line[26:29],
        'start_open': bool(line[29]),
        'end_open': bool(line[30]),
        'camera_origin': line[31:34],
        'camera_rotation': line[34:38]
    }


def write_line_split_line(write_file, line_dict):
    """
    Function used to write files for reading during training and inference.
    :param write_file: The file to write to.
    :param line_dict: The dictionary with all the line geometries and values. The values are following:
                      path to virtual image, start point, end point, normal 1, normal 2, open 1, open 2, label, type
                      class, frame id, camera origin, camera rotation
    """
    # WARNING: Changes here also need to happen in datagenerator_framewise.py

    # Lines are stored in format:
    #     0: path to virtual image
    # 1 - 3: start point
    # 4 - 6: end point
    # 7 - 9: normal 1
    # 10-12: normal 2
    #    13: open 1
    #    14: open 2
    #    15: label
    #    16: type
    #    17: class
    #    18: frame id
    # 19-21: camera origin
    # 22-24: camera rotation
    write_file.write(
        line_dict['image_path'] + ' ' +
        str(line_dict['start_point'][0]) + ' ' +
        str(line_dict['start_point'][1]) + ' ' +
        str(line_dict['start_point'][2]) + ' ' +
        str(line_dict['end_point'][0]) + ' ' +
        str(line_dict['end_point'][1]) + ' ' +
        str(line_dict['end_point'][2]) + ' ' +
        str(line_dict['normal_1'][0]) + ' ' +
        str(line_dict['normal_1'][1]) + ' ' +
        str(line_dict['normal_1'][2]) + ' ' +
        str(line_dict['normal_2'][0]) + ' ' +
        str(line_dict['normal_2'][1]) + ' ' +
        str(line_dict['normal_2'][2]) + ' ' +
        str(line_dict['start_open']) + ' ' +
        str(line_dict['end_open']) + ' ' +
        str(line_dict['label']) + ' ' +
        str(line_dict['type']) + ' ' +
        str(line_dict['class']) + ' ' +
        str(line_dict['frame_id']) + ' ' +
        str(line_dict['camera_origin'][0]) + ' ' +
        str(line_dict['camera_origin'][1]) + ' ' +
        str(line_dict['camera_origin'][2]) + ' ' +
        str(line_dict['camera_rotation'][0]) + ' ' +
        str(line_dict['camera_rotation'][1]) + ' ' +
        str(line_dict['camera_rotation'][2]) + ' ' +
        str(line_dict['camera_rotation'][3]) + ' ' +
        '\n'
    )



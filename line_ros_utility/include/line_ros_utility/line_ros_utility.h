#ifndef LINE_ROS_UTILITY_lINE_ROS_UTILITY_H_
#define LINE_ROS_UTILITY_lINE_ROS_UTILITY_H_

#include <iostream>
#include <string>

#include <ros/ros.h>

#include <cv_bridge/cv_bridge.h>
#include <image_geometry/pinhole_camera_model.h>
#include <dynamic_reconfigure/server.h>
#include <message_filters/subscriber.h>
#include <message_filters/sync_policies/exact_time.h>
#include <message_filters/synchronizer.h>
#include <pcl/conversions.h>
#include <pcl_ros/point_cloud.h>
#include <pcl_ros/publisher.h>
#include <std_msgs/String.h>
#include <sensor_msgs/CameraInfo.h>
#include <sensor_msgs/Image.h>
#include <sensor_msgs/PointCloud2.h>
#include <tf/transform_broadcaster.h>
#include <tf/transform_listener.h>
#include <visualization_msgs/Marker.h>

#include <line_clustering/line_clustering.h>
#include <line_detection/line_detection.h>
#include <line_detection/line_detection_inl.h>
#include <line_ros_utility/common.h>
#include <line_ros_utility/line_toolsConfig.h>
#include <line_ros_utility/RequestDecisionPath.h>
#include <line_ros_utility/TreeRequest.h>

// Factor that defines by how many times the image is enlarged when displaying a
// labelled line on an instance image.
constexpr int scale_factor_for_visualization = 4;

namespace line_ros_utility {

    typedef message_filters::sync_policies::ExactTime<
    sensor_msgs::Image, sensor_msgs::Image, sensor_msgs::Image, sensor_msgs::Image,
    sensor_msgs::CameraInfo, sensor_msgs::Image>
            MySyncPolicy;

    struct SearchTree {
        std::vector<size_t> children_right;
        std::vector<size_t> children_left;
    };

// This function returns a vector with labels for a vector of lines. It labels
// them after the classification into line_detection::LineType.
    std::vector<int> clusterLinesAfterClassification(
            const std::vector<line_detection::LineWithPlanes>& lines);

// The lines are printed in the file given by path. They are in the format that
// can be read by the random_forest.py node.
    bool printToFile(const std::vector<line_detection::LineWithPlanes>& lines3D,
                     const std::vector<int>& labels,
                     const std::vector<int>& classes,
                     const std::vector<std::vector<cv::Vec3f>>& line_normals,
                     const std::vector<std::vector<bool>>& line_opens,
                     const tf::StampedTransform& transform,
                     const std::string& path);
// Print 2D lines.
    bool printToFile(const std::vector<cv::Vec4f>& lines2D,
                     const std::string& path);
// Stores lines in marker messages.
    void storeLines3DinMarkerMsg(const std::vector<cv::Vec6f>& lines3D,
                                 visualization_msgs::Marker* disp_lines,
                                 cv::Vec3f color);
// Stores lines in marker messages with more detail.
    void storeLinesinMarkerMsg(const std::vector<line_detection::LineWithPlanes>& lines3D,
                               const std::vector<std::vector<cv::Vec3f>>& line_normals,
                               const std::vector<std::vector<bool>>& line_opens,
                               const size_t type,
                               visualization_msgs::Marker* disp_lines,
                               cv::Vec3f color);
// This functions are used to retrieve the default values for the paths and
// variables defined in the package, in case some arguments are not specified.
// Please see the script config_paths_and_variables.sh for a list of the paths
// and variables used and their meaning.
// Input: path_or_variable_name:   Name of the path or variable to retrieve.
//                                 Valid names are listed in
//                                 config_paths_and_variables.sh
//
// Output: path_or_variable_value: Value (either a string or an integer
//                                 depending on the variable) corresponding to
//                                 the input variable.
//         return:                 True if variable is successfully retrieved,
//                                 false otherwise.
    bool getDefaultPathsAndVariables(const std::string& path_or_variable_name,
                                     std::string* path_or_variable_value);
    bool getDefaultPathsAndVariables(const std::string& path_or_variable_name,
                                     int* path_or_variable_value);

// This class helps publishing several different clusters of lines in different
// colors, so that they are visualized by rviz. IMPORTANT: This function cannot
// display more clusters than there are colors defined in the constructor. If
// more clusters are given to the object, only the one with the highest labels
// are published.
    class DisplayClusters {
    public:
        DisplayClusters();
        // Frame ID of the marker message.
        void setFrameID(const std::string& frame_id);

        // It is used as input for the clusters to the class:
        // Input: lines3D: Vector of 3D lines.
        //
        //        labels:  Vector of equal size as lines3D. Every entry labels the
        //                 cluster the 3D line with the same index belongs to. The
        //                 labels should be continuous ((0, 1, 2, 3 -> good),
        //                 (0, 4, 8, 16 -> bad)), because the highest label defines
        //                 how many clusters are created (in the latter case of the
        //                 example 17 clusters will be created, but only 4 will
        //                 contain information).
        void setClusters(const std::vector<line_detection::LineWithPlanes>& lines3D,
                         const std::vector<int>& labels);

        // This functions advertises the message.
        void initPublishing(ros::NodeHandle& node_handle);
        void publish();

    private:
        bool frame_id_set_, clusters_set_, initialized_;
        std::vector<visualization_msgs::Marker> marker_lines_;
        std::vector<std::vector<cv::Vec<float, 6>>> line_clusters_;
        std::vector<ros::Publisher> pub_;
        std::string frame_id_;
        std::vector<cv::Vec3f> colors_;
    };

// This class helps publish the lines in rviz, including their normals,
// types and openness.
    class DisplayLines {
    public:
        DisplayLines();
        // Frame ID of the marker message.
        void setFrameID(const std::string& frame_id);

        // This functions advertises the message.
        void initPublishing(ros::NodeHandle& node_handle);
        void publish(const std::vector<line_detection::LineWithPlanes>& lines3D,
                     const std::vector<std::vector<cv::Vec3f>>& line_normals,
                     const std::vector<std::vector<bool>>& line_opens);

    private:
        bool frame_id_set_, initialized_;
        std::vector<visualization_msgs::Marker> marker_lines_;
        std::vector<ros::Publisher> pub_;
        std::string frame_id_;
        std::vector<cv::Vec3f> colors_;
    };

    class TreeClassifier {
    public:
        TreeClassifier();
        // Retrieves line decision paths from the random forest for specific lines.
        void getLineDecisionPath(
                const std::vector<line_detection::LineWithPlanes>& lines);
        // Retrieves the tree structures of all trees within the random forest.
        void getTrees();
        // Computes the distance between all lines. The lines are the one that were
        // given to the last call of getLineDecisionPath().
        void computeDistanceMatrix();
        // Recursive function to compute the distance between two data points.
        double computeDistance(const SearchTree& tree, const cv::SparseMat& path,
                               size_t line_idx1, size_t line_idx2, size_t idx);
        cv::Mat getDistanceMatrix();

    protected:
        size_t num_lines_;
        std::vector<SearchTree> trees_;
        ros::ServiceClient tree_client_;
        ros::ServiceClient line_client_;
        std_msgs::Header header_;
        // Decision paths are stored in a sparse matrix, because this matrix has
        // n_data_points*n_nodes_in_tree entries. If a entry (i, j) is non_zero, this
        // means that the i-th data_point went through the j-th node in the tree.
        std::vector<cv::SparseMat> decision_paths_;
        cv::Mat dist_matrix_;
    };

    class EvalData {
    public:
        EvalData(const std::vector<line_detection::LineWithPlanes>& lines3D);

        void createHeatMap(const cv::Mat& image, const cv::Mat& dist_mat,
                           const size_t idx);
        void storeHeatMaps(const cv::Mat& image, const cv::Mat& dist_mat,
                           const std::string& path);
        bool getHeatMapColor(float value, float* red, float* green, float* blue);
        void getValueBetweenTwoFixedColors(float value, int& red, int& green,
                                           int& blue);

        float dist(const cv::Mat& dist_mat, size_t i, size_t j);

        void projectLinesTo2D(const sensor_msgs::CameraInfoConstPtr& camera_info);

        void writeHeatMapColorBar(const std::string& path);

    protected:
        std::vector<cv::Vec6f> lines3D_;
        std::vector<cv::Vec4f> lines2D_;
        cv::Mat heat_map_;
    };

// Class to handle inliers points with their labels.
    class InliersWithLabels {
    public:
        InliersWithLabels();

        // Returns mean point of the set of inliers.
        //
        // Output: return: Mean point of the set of inliers.
        cv::Vec3f findMeanPoint();
        // Counts the occurrences of a label in the inliers.
        //
        // Input: label: Label of which to count the occurrences in the inliers.
        int countLabelInInliers(const unsigned short& label);
        // Sets inliers_with_labels_ to be inliers_with_labels.
        //
        // Input: inliers_with_labels: Inliers with corresponding labels.
        void setInliersWithLabels(
                const std::vector<std::pair<cv::Vec3f, unsigned short>>&
                inliers_with_labels);

        // Returns the inliers with labels on the two planes.
        //
        // Output: inliers_with_labels: Vector of pair (3D point, label) that stores
        //                              the inliers with their labels.
        void getInliersWithLabels(
                std::vector<std::pair<cv::Vec3f, unsigned short>>* inliers_with_labels);

        // Returns the instance label corresponding to the majority vote of the
        // instances of the inliers.
        //
        // Output: return: Most frequent instance label among the inlier points.
        int getLabelByMajorityVote();

    private:
        std::vector<std::pair<cv::Vec3f, unsigned short>> inliers_with_labels_;

        // True if detailed prints about the lines labelled should be displayed.
        bool verbose_mode_on_ = false;
    };


// The main class that has the full utility of line_detection, line_clustering
// and line_ros_utility implemented. Fully functional in a ros node.
    class ListenAndPublish {
    public:
        ListenAndPublish(std::string trajectory_number, std::string write_path,
                         int start_frame, int frame_step);
        ~ListenAndPublish();

        void start();

    protected:
        // Writes a mat to a pcl cloud. This is only used to publish the cloud so
        // that it can be displayed with rviz.
        void writeMatToPclCloud(const cv::Mat& cv_cloud, const cv::Mat& image,
                                pcl::PointCloud<pcl::PointXYZRGB>* pcl_cloud);
        // These functions perform the actual work. They are only here to make the
        // masterCallback more readable.
        void detectLines();
        void projectTo3D();
        void checkLines();
        void printNumberOfLines();
        void clusterKmeans();
        void clusterKmedoid();
        void initDisplay();
        void publish();
        // This is the callback that is called by the dynamic reconfigure.
        void reconfigureCallback(line_ros_utility::line_toolsConfig& config,
                                 uint32_t level);
        // This callback is called by the main subsriber sync_.
        void masterCallback(const sensor_msgs::ImageConstPtr& rosmsg_image,
                            const sensor_msgs::ImageConstPtr& rosmsg_depth,
                            const sensor_msgs::ImageConstPtr& rosmsg_instances,
                            const sensor_msgs::ImageConstPtr& rosmsg_classes,
                            const sensor_msgs::CameraInfoConstPtr& camera_info,
                            const sensor_msgs::ImageConstPtr& rosmsg_cloud);

        // (Deprecated). Old version of labelLinesWithInstances.
        // This function labels with an instances image.
        // Input: lines:       Vector with the lines in 3D.
        //
        //        instances:   Image that labels objects with a different value for
        //                     each instance. This image must be registered with the
        //                     depth image where the point cloud was extracted.
        //
        //        camera_info: This is used to reproject 3D points onto the instances
        //                     image.
        //
        // Output: labels: Labels all lines according to their backprojection onto
        //                 instances. The labeling starts at 0 and goes up for every
        //                 additional instance that was found.
        void labelLinesWithInstancesByMajorityVoting(
                const std::vector<line_detection::LineWithPlanes>& lines,
                const cv::Mat& instances, sensor_msgs::CameraInfoConstPtr camera_info,
                std::vector<int>* labels);

        // This function labels with an instances image. The labelling depends on the
        // line type associated to each line.
        // Input: lines:       Vector with the lines in 3D.
        //
        //        instances:   Image that labels objects with a different value for
        //                     each instance. This image must be registered with the
        //                     depth image where the point cloud was extracted.
        //
        //        camera_info: This is used to reproject 3D points onto the instances
        //                     image.
        //
        // Output: labels: Labels all lines according to their reprojection onto
        //                 instances. The labeling starts at 0 and goes up for every
        //                 additional instance that was found.
        void labelLinesWithInstances(
                const std::vector<line_detection::LineWithPlanes>& lines,
                const cv::Mat& instances, sensor_msgs::CameraInfoConstPtr camera_info,
                const std::map<uint16_t, uint16_t>& instance_to_class_map,
                std::vector<int>* labels);


        // Assigns the instance labels of a line to be the most frequent instance
        // label among the points of the inlier plane respectively closest or furthest
        // to the origin. Overloads assignLabelOfInlierPlaneBasedOnDistance.
        void assignLabelOfClosestInlierPlane(
                const line_detection::LineWithPlanes& line, const cv::Mat& instances,
                sensor_msgs::CameraInfoConstPtr camera_info, int* label);
        void assignLabelOfFurthestInlierPlane(
                const line_detection::LineWithPlanes& line, const cv::Mat& instances,
                sensor_msgs::CameraInfoConstPtr camera_info, int* label);
        // Assigns the instance labels of a line to be the most frequent instance
        // label among the points of the inlier plane either closest or furthest to
        // the origin, according to the value of furthest_plane. To only consider the
        // part of the inlier planes that actually contain points the distance is
        // computed not by taking the regular point-to-plane distance, but by
        // computing the mean point of both sets of inlier points and taking the
        // distance between each of these mean points and the origin.
        // Input: line:           Input line.
        //
        //        instances:      Image that labels objects with a different value for
        //                        each instance. This image must be registered with
        //                        the depth image where the point cloud was extracted.
        //
        //        camera_info:    This is used to reproject 3D points onto the
        //                        instances image.
        //
        //        furthest_plane: True if the plane from which to take the instance
        //                        label should be the one furthest away from the
        //                        origin, false otherwise.
        //
        // Output: label: Output instance label.
        void assignLabelOfInlierPlaneBasedOnDistance(
                const line_detection::LineWithPlanes& line, const cv::Mat& instances,
                sensor_msgs::CameraInfoConstPtr camera_info, bool furthest_plane,
                int* label);

        // Given one or both the inlier planes of a line, returns the set of inlier
        // points (with the instance of each inlier point) associated to the plane(s).
        // Input: line:               Input line.
        //
        //        plane:              Plane(s) inlier to the input line. NOTE: When
        //              or            first_plane_only is false, plane_1 should be the
        //        plane_1/plane_2:    right plane and plane_2 the left plane.
        //
        //        instances:          Image that labels objects with a different value
        //                            for each instance. This image must be registered
        //                            with the depth image where the point cloud was
        //                            extracted.
        //
        //        camera_info:        This is used to reproject 3D points onto the
        //                            instances image.
        //
        //        (first_plane_only): True if inliers should be obtained only for the
        //                            first of the two planes. Used for overloading
        //                            to have the single-plane version.
        //
        // Output: inliers:            Inlier points associated to the plane(s).
        //                 or
        //         inliers_right/left:
        void findInliersWithLabelsGivenPlane(
                const line_detection::LineWithPlanes& line, const cv::Vec4f& plane,
                const cv::Mat& instances, sensor_msgs::CameraInfoConstPtr camera_info,
                InliersWithLabels* inliers);
        void findInliersWithLabelsGivenPlanes(
                const line_detection::LineWithPlanes& line, const cv::Vec4f& plane_1,
                const cv::Vec4f& plane_2, const cv::Mat& instances,
                sensor_msgs::CameraInfoConstPtr camera_info,
                InliersWithLabels* inliers_right, InliersWithLabels* inliers_left,
                bool first_plane_only = false);

        // Given a 3D line and one of its two inlier planes, computes the instance of
        // the line by taking the majority vote of the instances of the its inlier
        // points that lie on that plane.
        // Input: line:        3D line, the instance of which should be computed.
        //
        //        plane:       cv::Vec4f vector representing the inlier plane from
        //                     which to extract the instance label (in Hessian form).
        //
        //        instances:   Image that labels objects with a different value for
        //                     each instance. This image must be registered with the
        //                     depth image where the point cloud was extracted.
        //
        //        camera_info: This is used to reproject 3D points onto the
        //                     instances image.
        //
        // Output: label: Instance label to be associated to the line.
        void labelLineGivenInlierPlane(const line_detection::LineWithPlanes& line,
                                       const cv::Vec4f& plane,
                                       const cv::Mat& instances,
                                       sensor_msgs::CameraInfoConstPtr camera_info,
                                       int* label);

        // Displays the original image with the 2D line and the valid inliers in the
        // two rectangles fitted around the line.
        // Input: line_2D:            2D line.
        //
        //        inliers_right/left: (Depending on the two overloads, either a vector
        //                             points or a vector of pairs point-label).
        //                            Inliers fitted on the right/left rectangle, as
        //                            3D points.
        //
        //        instances:          Image that labels objects with a different value
        //                            for each instance. This image must be registered
        //                            with the depth image where the point cloud was
        //                            extracted.
        //
        //        camera_info:        This is used to reproject 3D points onto the
        //                            instances image.
        void display2DLineWithRectangleInliers(
                const cv::Vec4f& line_2D,
                const std::vector<cv::Vec3f>& inliers_right,
                const std::vector<cv::Vec3f>& inliers_left, const cv::Mat& instances,
                sensor_msgs::CameraInfoConstPtr camera_info);
        void display2DLineWithRectangleInliers(
                const cv::Vec4f& line_2D,
                const std::vector<std::pair<cv::Vec3f, unsigned short>>& inliers_right,
                const std::vector<std::pair<cv::Vec3f, unsigned short>>& inliers_left,
                const cv::Mat& instances, sensor_msgs::CameraInfoConstPtr camera_info);

        // Displays a labelled line on top of an image in which all pixels that
        // correspond to points that have the same instance label as the line are
        // displayed in green and all the others have their original RGB color.
        // Input: line:        Labelled line.
        //
        //        label:       Instance label of the line.
        //
        //        image:       Original RGB image.
        //
        //        instances:   Instances image.
        //
        //        camera_info: This is used to reproject 3D points onto the
        //                     instances image.
        void displayLabelledLineOnInstanceImage(
                const line_detection::LineWithPlanes& line, const unsigned short& label,
                const cv::Mat& image, const cv::Mat& instances,
                sensor_msgs::CameraInfoConstPtr camera_info);

        // Extracts the normal facing the camera from the hessians and stores them.
        // Normals of non-existent planes are zero-padded.
        // in the lines.
        // Input: lines:       Lines with 3D start and endpoints and the hessian forms of the planes.
        //
        // Output: normals:    The normals for each plane of the line.
        void extractNormalsFromLines(
                const std::vector<line_detection::LineWithPlanes>& lines,
                std::vector<std::vector<cv::Vec3f>>* normals);

        // Checks if lines are open or not. Open line ends are end points that cannot
        // be fully determined as such. This can happen if the line is obscured by an
        // object or continues outside of the camera FOV.
        // Input: lines:       Lines with 3D start and endpoints.
        //
        //        depth:       Depth image.
        //
        //        camera_info: Used to reproject 3D points onto depth image.
        //
        // Output: opens:      The boolean values if the line end points are occluded or not.
        void checkLinesOpen(
                const std::vector<line_detection::LineWithPlanes>& lines,
                const cv::Mat& depth_map, sensor_msgs::CameraInfoConstPtr camera_info,
                std::vector<std::vector<bool>>* opens);

        // Helper function to check if one line is open or not at the end point.
        // Input: startpoint:  Point where the line starts in 3D.
        //
        //        endpoint:    Point where the line ends in 3D. This is where
        //                     occlusion is checked.
        //
        //        depth:       Depth image.
        //
        //        camera_info: Used to reproject 3D points onto depth image.
        bool checkLineOpen(cv::Vec3f start_point, cv::Vec3f end_point,
                           const cv::Mat& depth_map, sensor_msgs::CameraInfoConstPtr camera_info);

        // Helper function to check the class_id for each instance
        void instanceToClassIDMap(const cv::Mat& instances, const cv::Mat& classes,
                                 std::map<uint16_t, uint16_t>* instance_to_class_map);

        // Helper function to assign each line a class label. The class label is determined
        // using the precomputed instance to class map.
        // Input: instance_labels:       The instance labels of each line.
        //
        //        instance_to_class_map: The precomputed instance to class map containing a class
        //                               label for each instance present in the frame.
        //
        // Output: class_labels:         The resulting class labels.
        void labelLinesWithClasses(const std::vector<int>& instance_labels,
                const std::map<uint16_t, uint16_t>& instance_to_class_map,
                std::vector<int>* class_labels);

        // The callback function to save the path where the lines should be saved to.
        void pathCallback(const std_msgs::String::ConstPtr& path_msg);


    private:
        // True if lines should be displayed, once labelled, overlapped on the
        // instance image.
        bool labelled_line_visualization_mode_on_ = false;
        // True if the inliers found around the line should be displayed.
        bool inliers_visualization_mode_on_ = false;
        // True if detailed prints about the lines labelled should be displayed.
        bool verbose_mode_on_ = false;

        // Data storage.
        std::string output_path_;
        size_t iteration_;
        size_t frame_step_;
        cv::Mat cv_image_;
        cv::Mat cv_img_gray_;
        cv::Mat cv_cloud_;
        cv::Mat cv_depth_;
        cv::Mat cv_instances_;
        cv::Mat cv_classes_;
        // To store the color value in the instance image(1 channel). If the instance
        // image has instead 3 channels, the variable's type should be changed to
        // std::vector<cv::Vec3b> ;
        std::vector<unsigned short> known_colors_;

        pcl::PointCloud<pcl::PointXYZRGB> pcl_cloud_;
        // All the 2D lines detected in the grayscale image.
        std::vector<cv::Vec4f> lines2D_;
        // All the 2D lines kept (bijection with lines3D_).
        std::vector<cv::Vec4f> lines2D_kept_;
        // A temporary variable to store the 2D lines kept.
        std::vector<cv::Vec4f> lines2D_kept_tmp_;
        std::vector<cv::Vec<float, 6>> lines3D_;
        std::vector<line_detection::LineWithPlanes> lines3D_temp_wp_;
        std::vector<line_detection::LineWithPlanes> lines3D_with_planes_;
        std::vector<int> labels_;
        std::vector<int> class_ids_;
        std::map<uint16_t, uint16_t> instance_to_class_map_;
        std::vector<std::vector<int>> labels_left_right;
        std::vector<std::vector<cv::Vec3f>> line_normals_;
        std::vector<std::vector<bool>> line_opens_;
        std::vector<int> labels_rf_kmedoids_;
        sensor_msgs::CameraInfoConstPtr camera_info_;
        // Camera projection matrix.
        cv::Mat camera_P_;
        // Publishers and Subscribers.
        tf::TransformBroadcaster broad_caster_;
        tf::Transform transform_;
        tf::TransformListener tf_listener_;
        ros::Publisher pcl_pub_;
        ros::Subscriber path_sub_;
        message_filters::Synchronizer<MySyncPolicy>* sync_;
        message_filters::Subscriber<sensor_msgs::Image> image_sub_;
        message_filters::Subscriber<sensor_msgs::Image> depth_sub_;
        message_filters::Subscriber<sensor_msgs::Image> instances_sub_;
        message_filters::Subscriber<sensor_msgs::Image> classes_sub_;
        message_filters::Subscriber<sensor_msgs::Image> cloud_sub_;
        message_filters::Subscriber<sensor_msgs::CameraInfo> info_sub_;
        // To store parameters.
        line_detection::LineDetectionParams params_;
        size_t detector_method_;
        size_t number_of_clusters_;
        size_t show_lines_or_clusters_;
        // To have the line_detection utility.
        line_detection::LineDetector line_detector_;
        line_clustering::KMeansCluster kmeans_cluster_;
        DisplayClusters display_clusters_;
        DisplayLines display_lines_;
        // To dynamically reconfigure parameters.
        dynamic_reconfigure::Server<line_ros_utility::line_toolsConfig>
                dynamic_rcf_server_;
        dynamic_reconfigure::Server<line_ros_utility::line_toolsConfig>::CallbackType
                dynamic_rcf_callback_;
        // To measure time.
        std::chrono::time_point<std::chrono::system_clock> start_time_, end_time_;
        std::chrono::duration<double> elapsed_seconds_;
        // For random forest clustering.
        TreeClassifier tree_classifier_;
        line_clustering::KMedoidsCluster kmedoids_cluster_;
        // To handle trajectories with a general index (not necessarily 1).
        const std::string kTrajectoryNumber_;
        // Path where to write the lines files.
        const std::string kWritePath_;
    };

}  // namespace line_ros_utility

#endif  // LINE_ROS_UTILITY_lINE_ROS_UTILITY_H_
#include <ros/ros.h>

#include <cv_bridge/cv_bridge.h>
#include <image_transport/image_transport.h>
#include <message_filters/subscriber.h>
#include <message_filters/sync_policies/exact_time.h>
#include <message_filters/synchronizer.h>
#include <pcl/conversions.h>
#include <pcl_ros/point_cloud.h>
#include <pcl_ros/publisher.h>
#include <sensor_msgs/CameraInfo.h>
#include <sensor_msgs/Image.h>
#include <sensor_msgs/PointCloud2.h>
#include <tf/transform_broadcaster.h>
#include <visualization_msgs/Marker.h>

#include <line_detection/line_detection.h>
#include "line_detection/line_detection_inl.h"

typedef message_filters::sync_policies::ExactTime<
    sensor_msgs::Image, sensor_msgs::Image, sensor_msgs::Image,
    sensor_msgs::CameraInfo, sensor_msgs::PointCloud2>
    MySyncPolicy;

const std::string kRGBImageTopic = "/camera/rgb/image_raw";
const std::string kDepthImageTopic = "/camera/depth/image_raw";
const std::string kCameraInfoTopic = "/camera/rgb/camera_info";
const std::string kPointCloudTopic = "/scenenet_node/scene";
const std::string kInstancesTopic = "/camera/instances/image_raw";

class listenAndPublish {
 public:
  listenAndPublish() {
    ros::NodeHandle node_handle_;
    // The Pointcloud publisher and transformation for RVIZ.
    pcl_pub_ = node_handle_.advertise<pcl::PointCloud<pcl::PointXYZRGB> >(
        "/vis_pointcloud", 2);
    transform_.setOrigin(tf::Vector3(0, 0, 0));
    tf::Quaternion quat;
    quat.setRPY(-line_detection::kPi / 2.0, 0.0, 0.0);
    transform_.setRotation(quat);
    // To publish different types of lines as markers for rviz (this basically
    // allows different colors for the different types).
    marker_pub_ = node_handle_.advertise<visualization_msgs::Marker>(
        "/visualization_marker", 1000);
    marker_pub_1_ = node_handle_.advertise<visualization_msgs::Marker>(
        "/visualization_marker_1", 1000);
    marker_pub_2_ = node_handle_.advertise<visualization_msgs::Marker>(
        "/visualization_marker_2", 1000);
    marker_pub_3_ = node_handle_.advertise<visualization_msgs::Marker>(
        "/visualization_marker_3", 1000);

    image_sub_.subscribe(node_handle_, kRGBImageTopic, 1);
    depth_sub_.subscribe(node_handle_, kDepthImageTopic, 1);
    instances_sub_.subscribe(node_handle_, kInstancesTopic, 1);
    info_sub_.subscribe(node_handle_, kCameraInfoTopic, 1);
    pc_sub_.subscribe(node_handle_, kPointCloudTopic, 1);
  }

  ~listenAndPublish() { delete sync_; }

  void startListenAndPublishOnce() {
    // The exact time synchronizer makes it possible to have a single callback
    // that recieves messages of all three topics above synchronized. This means
    // every call of the callback function recieves three messages that have the
    // same timestamp.
    sync_ = new message_filters::Synchronizer<MySyncPolicy>(
        MySyncPolicy(10), image_sub_, depth_sub_, instances_sub_, info_sub_,
        pc_sub_);
    sync_->registerCallback(boost::bind(
        &listenAndPublish::pcCallbackPublishOnce, this, _1, _2, _3, _4, _5));
  }

  void pcCallbackPublishOnce(
      const sensor_msgs::ImageConstPtr& rosmsg_image,
      const sensor_msgs::ImageConstPtr& rosmsg_depth,
      const sensor_msgs::ImageConstPtr& rosmsg_instances,
      const sensor_msgs::CameraInfoConstPtr& camera_info,
      const sensor_msgs::PointCloud2ConstPtr& rosmsg_cloud) {
    // Extract the point cloud from the message.
    pcl::fromROSMsg(*rosmsg_cloud, pcl_cloud_);
    // Extract the image from the message.
    cv_bridge::CvImageConstPtr cv_img_ptr =
        cv_bridge::toCvShare(rosmsg_image, "rgb8");
    cv_image_ = cv_img_ptr->image;

    cv_bridge::CvImageConstPtr cv_depth_ptr =
        cv_bridge::toCvShare(rosmsg_depth);
    cv_depth_ = cv_depth_ptr->image;

    cv_bridge::CvImageConstPtr cv_instances_ptr =
        cv_bridge::toCvShare(rosmsg_instances);
    cv_instances_ = cv_instances_ptr->image;

    cvtColor(cv_image_, cv_img_gray_, CV_RGB2GRAY);

    cv::Mat depth_uchar;
    cv_depth_.convertTo(depth_uchar, CV_8UC1, 0.1);
    std::vector<cv::Vec4f> lines2D_depth;
    line_detector_.detectLines(depth_uchar, line_detection::Detector::LSD,
                               &lines2D_depth);

    line_detection::pclFromSceneNetToMat(pcl_cloud_, &cv_cloud_);

    std::chrono::time_point<std::chrono::system_clock> start, end;
    std::chrono::duration<double> elapsed_seconds;

    ROS_INFO("**** New Image ******");
    start = std::chrono::system_clock::now();
    line_detector_.detectLines(cv_img_gray_, line_detection::Detector::LSD,
                               &lines2D_);
    end = std::chrono::system_clock::now();
    elapsed_seconds = end - start;
    ROS_INFO("Detecting lines 2D: %f", elapsed_seconds.count());

    for (size_t i = 0; i < lines2D_depth.size(); ++i) {
      lines2D_.push_back(lines2D_depth[i]);
    }

    start = std::chrono::system_clock::now();
    std::vector<cv::Vec6f> lines3D_temp;
    std::vector<line_detection::LineWithPlanes> lines3D_temp_wp;

    line_detector_.project2Dto3DwithPlanes(cv_cloud_, cv_image_, lines2D_,
                                           &lines3D_temp_wp, true);
    end = std::chrono::system_clock::now();
    elapsed_seconds = end - start;
    ROS_INFO("Projecting to 3D: %f", elapsed_seconds.count());

    start = std::chrono::system_clock::now();
    line_detector_.runCheckOn3DLines(cv_cloud_, lines3D_temp_wp,
                                     &lines3D_with_planes_);
    end = std::chrono::system_clock::now();
    elapsed_seconds = end - start;
    ROS_INFO("Check for valid lines: %f", elapsed_seconds.count());

    ROS_INFO("Lines kept after projection: %d/%d", lines3D_with_planes_.size(),
             lines2D_.size());

    std::string write_to_file_path("example_data_lines.txt");
    line_detector_.printToFile(lines3D_with_planes_, write_to_file_path);

    // marker_3Dlines_.header.frame_id = pcl_cloud_.header.frame_id;
    // line_detection::storeLines3DinMarkerMsg(lines3D_, marker_3Dlines_);
    line_detection::storeLinesAfterType(lines3D_with_planes_, &lines3D_discont_,
                                        &lines3D_plane_, &lines3D_inter_);
    line_detection::storeLines3DinMarkerMsg(
        lines3D_discont_, &marker_3Dlines_discont_, {1, 0, 0});
    line_detection::storeLines3DinMarkerMsg(lines3D_plane_,
                                            &marker_3Dlines_plane_, {0, 1, 0});
    line_detection::storeLines3DinMarkerMsg(lines3D_inter_,
                                            &marker_3Dlines_inter_, {0, 0, 1});

    marker_3Dlines_discont_.header.frame_id = pcl_cloud_.header.frame_id;
    marker_3Dlines_plane_.header.frame_id = pcl_cloud_.header.frame_id;
    marker_3Dlines_inter_.header.frame_id = pcl_cloud_.header.frame_id;

    // The timestamp is set to 0 because rviz is not able to find the right
    // transformation otherwise.
    pcl_cloud_.header.stamp = 0;
    // For testing reasons only one instance is published. Otherwise new point
    // clouds would be published at 1 Hz (which makes it hard to visually
    // inspect them).
    ros::Rate rate(0.2);
    ROS_INFO("**** Started publishing ****");
    // while (!ros::isShuttingDown()) {
    broad_caster_.sendTransform(tf::StampedTransform(
        transform_, ros::Time::now(), "map", pcl_cloud_.header.frame_id));
    pcl_pub_.publish(pcl_cloud_);

    marker_pub_1_.publish(marker_3Dlines_discont_);
    marker_pub_2_.publish(marker_3Dlines_plane_);
    marker_pub_3_.publish(marker_3Dlines_inter_);

    // rate.sleep();
    // }
  }

 protected:
  // Data storage.
  cv::Mat cv_image_;
  cv::Mat cv_img_gray_;
  cv::Mat cv_cloud_;
  cv::Mat cv_depth_;
  cv::Mat cv_instances_;
  pcl::PointCloud<pcl::PointXYZRGB> pcl_cloud_;
  std::vector<cv::Vec4f> lines2D_;
  std::vector<cv::Vec6f> lines3D_, lines3D_discont_, lines3D_plane_,
      lines3D_inter_;
  std::vector<line_detection::LineWithPlanes> lines3D_with_planes_;
  visualization_msgs::Marker marker_3Dlines_, marker_3Dlines_discont_,
      marker_3Dlines_plane_, marker_3Dlines_inter_;
  // Publishers and Subscribers.
  tf::TransformBroadcaster broad_caster_;
  tf::Transform transform_;
  ros::Publisher pcl_pub_;
  ros::Publisher marker_pub_;
  ros::Publisher marker_pub_1_;
  ros::Publisher marker_pub_2_;
  ros::Publisher marker_pub_3_;
  message_filters::Synchronizer<MySyncPolicy>* sync_;
  message_filters::Subscriber<sensor_msgs::Image> image_sub_;
  message_filters::Subscriber<sensor_msgs::Image> depth_sub_;
  message_filters::Subscriber<sensor_msgs::Image> instances_sub_;
  message_filters::Subscriber<sensor_msgs::CameraInfo> info_sub_;
  message_filters::Subscriber<sensor_msgs::PointCloud2> pc_sub_;
  // To have the line_detection utility.
  line_detection::LineDetector line_detector_;
};

int main(int argc, char** argv) {
  ros::init(argc, argv, "detect_and_show_3D");
  listenAndPublish ls;
  ls.startListenAndPublishOnce();
  ros::spin();
  return 0;
}
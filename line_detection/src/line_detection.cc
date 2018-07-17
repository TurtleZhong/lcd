#include "line_detection/line_detection.h"

namespace line_detection {

cv::Vec3f projectPointOnPlane(const cv::Vec4f& hessian,
                              const cv::Vec3f& point) {
  cv::Vec3f x_0, normal;
  normal = {hessian[0], hessian[1], hessian[2]};
  size_t non_zero;
  for (non_zero = 0; non_zero < 3; ++non_zero) {
    if (fabs(hessian[non_zero]) > 0.1) break;
  }
  for (size_t i = 0; i < 3; ++i) {
    if (i == non_zero)
      x_0[i] = -hessian[3] / hessian[non_zero];
    else
      x_0[i] = 0;
  }
  return point - (point - x_0).dot(normal) * normal;
}

bool areLinesEqual2D(const cv::Vec4f line1, const cv::Vec4f line2) {
  // First compute the difference in angle. For easier computation not the
  // actual difference in angle, but cos(theta)^2 is computed.
  float vx1 = line1[0] - line1[2];
  float vx2 = line2[0] - line2[2];
  float vy1 = line1[1] - line1[3];
  float vy2 = line2[1] - line2[3];

  float angle_difference = pow((vx1 * vx2 + vy1 * vy2), 2) /
                           ((vx1 * vx1 + vy1 * vy1) * (vx2 * vx2 + vy2 * vy2));
  // Then compute the distance of the two lines. All distances between both end
  // and start points are computed and the lowest is kept.
  float dist[4], min_dist;
  dist[0] = pow(line1[0] - line2[0], 2) + pow(line1[1] - line2[1], 2);
  dist[1] = pow(line1[0] - line2[2], 2) + pow(line1[1] - line2[3], 2);
  dist[2] = pow(line1[2] - line2[0], 2) + pow(line1[3] - line2[1], 2);
  dist[3] = pow(line1[2] - line2[2], 2) + pow(line1[3] - line2[3], 2);
  min_dist = dist[0];
  for (size_t i = 1; i < 4; ++i) {
    if (dist[i] < min_dist) min_dist = dist[i];
  }
  // Set the comparison parameters. These need to be adjusted correctly.
  // angle_diffrence > 0.997261 refers is equal to effective angle < 3 deg.
  if (angle_difference > 0.95 && min_dist < 5)
    return true;
  else
    return false;
}

void findXCoordOfPixelsOnVector(const cv::Point2f& start,
                                const cv::Point2f& end, bool left_side,
                                std::vector<int>* x_coord) {
  CHECK_NOTNULL(x_coord);
  int top = floor(start.y);
  int bottom = ceil(end.y);
  int height = bottom - top;
  float x_start, width;
  x_start = floor(start.x) + 0.5;
  width = floor(end.x) - floor(start.x);
  CHECK(height > 0) << "Important: the following statement must hold: start.y "
                       "< end.y. "
                    << height;
  if (height == 1) {
    if (left_side) {
      x_coord->push_back(floor(start.x));
    } else {
      x_coord->push_back(ceil(end.x));
    }
    return;
  }
  for (int i = 0; i < height; ++i) {
    x_coord->push_back(int(x_start + i * width / (height - 1)));
  }
}

void findPointsInRectangle(std::vector<cv::Point2f> corners,
                           std::vector<cv::Point2i>* points) {
  CHECK_NOTNULL(points);
  CHECK_EQ(corners.size(), 4)
      << "The rectangle must be defined by exactly 4 corner points.";
  // Find the relative positions of the points.
  // int upper, lower, left, right, store_i;
  std::vector<int> idx{0, 1, 2, 3};
  // This part finds out if two of the points have equal x values. This may
  // not be very likely for some data, but if it happens it can produce
  // unpredictibale outcome. If this is the case, the rectangle is rotated by
  // 0.1 degree. This should not make a difference, becaues the pixels have
  // integer values anyway (so a corner point of 100.1 and 100.2 gives the
  // same result).
  bool some_points_have_equal_height = false;
  // Check all x values against all others.
  for (size_t i = 1; i < 4; ++i) {
    if (corners[0].y == corners[i].y) some_points_have_equal_height = true;
  }
  for (size_t i = 2; i < 4; ++i) {
    if (corners[1].y == corners[i].y) some_points_have_equal_height = true;
  }
  if (corners[2].y == corners[3].y) some_points_have_equal_height = true;
  // Do the rotation.
  if (some_points_have_equal_height) {
    constexpr double rot_deg = 0.1;
    const double rot_rad = degToRad(rot_deg);
    for (size_t i = 0; i < 4; ++i)
      corners[i] = {cos(rot_rad) * corners[i].x - sin(rot_rad) * corners[i].y,
                    cos(rot_rad) * corners[i].x + sin(rot_rad) * corners[i].y};
  }
  // The points are set to lowest, highest, most right and most left in this
  // order. It does work because the preprocessing done guarantees that no two
  // points have the same y coordinate.
  cv::Point2f upper, lower, left, right;
  upper = corners[0];
  int j;
  for (int i = 1; i < 4; ++i) {
    if (upper.y > corners[i].y) {
      upper = corners[i];
    }
  }
  lower.y = -1e6;
  for (int i = 0; i < 4; ++i) {
    if (lower.y < corners[i].y && corners[i] != upper) {
      lower = corners[i];
    }
  }
  left.x = 1e6;
  for (int i = 0; i < 4; ++i) {
    if (left.x > corners[i].x && corners[i] != upper && corners[i] != lower) {
      left = corners[i];
    }
  }
  for (int i = 0; i < 4; ++i) {
    if (corners[i] != left && corners[i] != upper && corners[i] != lower) {
      right = corners[i];
    }
  }
  // With the ordering given, the border pixels can be found as pixels, that
  // lie on the border vectors.
  std::vector<int> left_border;
  std::vector<int> right_border;
  findXCoordOfPixelsOnVector(upper, left, true, &left_border);
  findXCoordOfPixelsOnVector(upper, right, false, &right_border);
  // Pop_back is used because otherwise the corners[left/right] pixels would
  // be counted twice.
  left_border.pop_back();
  right_border.pop_back();
  findXCoordOfPixelsOnVector(left, lower, true, &left_border);
  findXCoordOfPixelsOnVector(right, lower, false, &right_border);
  if (left_border.size() > right_border.size()) {
    left_border.pop_back();
  } else if (left_border.size() < right_border.size()) {
    right_border.pop_back();
  }
  CHECK_EQ(left_border.size(), right_border.size());
  // Iterate over all pixels in the rectangle.
  points->clear();
  int x, y;
  for (int i = 0; i < left_border.size(); ++i) {
    y = floor(upper.y) + i;
    // y = floor(corners[upper].y) + i;
    x = left_border[i];
    do {
      points->push_back(cv::Point2i(x, y));
      ++x;
    } while (x <= right_border[i]);
  }
}

bool getPointOnPlaneIntersectionLine(const cv::Vec4f& hessian1,
                                     const cv::Vec4f& hessian2,
                                     const cv::Vec3f& direction,
                                     cv::Vec3f* x_0) {
  CHECK_NOTNULL(x_0);
  // The problem can be solved with a under determined linear system. See
  // http://mathworld.wolfram.com/Plane-PlaneIntersection.html
  cv::Mat m(2, 2, CV_32FC1);
  cv::Mat b(2, 1, CV_32FC1);
  cv::Mat x_0_mat(2, 1, CV_32FC1);
  int non_zero, count = 0;
  // Because the system is underdetemined, we can set an element of our
  // solution to zero. We just have to check that the corresponding element in
  // the direction vector is non-zero. For numerical stability we check here
  // that the element is greater than 0.1. Given that the vector is
  // normalized, at least one element always meets this condition.
  for (non_zero = 2; non_zero >= 0; --non_zero) {
    if (fabs(direction[non_zero]) > 0.1) break;
  }
  // Fill in the matrices for m*x_0 = b and solve the system.
  for (int i = 0; i < 3; ++i) {
    if (i == non_zero) continue;
    m.at<float>(0, count) = hessian1[i];
    m.at<float>(1, count) = hessian2[i];
    ++count;
  }
  b.at<float>(0, 0) = -hessian1[3];
  b.at<float>(1, 0) = -hessian2[3];
  bool success = cv::solve(m, b, x_0_mat);
  count = 0;
  // When filling in the solution we must again take into account that we
  // assumend a certain component to be zero.
  for (int i = 0; i < 3; ++i) {
    if (i == non_zero) {
      (*x_0)[i] = 0;
      continue;
    }
    (*x_0)[i] = x_0_mat.at<float>(count, 0);
    ++count;
  }
  return success;
}

LineDetector::LineDetector() {
  lsd_detector_ = cv::createLineSegmentDetector(cv::LSD_REFINE_STD);
  edl_detector_ =
      cv::line_descriptor::BinaryDescriptor::createBinaryDescriptor();
  fast_detector_ = cv::ximgproc::createFastLineDetector();
  params_ = new LineDetectionParams();
  params_is_mine_ = true;
}
LineDetector::LineDetector(LineDetectionParams* params) {
  lsd_detector_ = cv::createLineSegmentDetector(cv::LSD_REFINE_STD);
  edl_detector_ =
      cv::line_descriptor::BinaryDescriptor::createBinaryDescriptor();
  fast_detector_ = cv::ximgproc::createFastLineDetector();
  params_ = params;
  params_is_mine_ = false;
}
LineDetector::~LineDetector() {
  if (params_is_mine_) {
    delete params_;
  }
}

void LineDetector::detectLines(const cv::Mat& image, int detector,
                               std::vector<cv::Vec4f>* lines) {
  if (detector == 0)
    detectLines(image, Detector::LSD, lines);
  else if (detector == 1)
    detectLines(image, Detector::EDL, lines);
  else if (detector == 2)
    detectLines(image, Detector::FAST, lines);
  else if (detector == 3)
    detectLines(image, Detector::HOUGH, lines);
  else {
    LOG(WARNING)
        << "LineDetetor::detectLines: Detector choice not valid, LSD was "
           "chosen as default.";
    detectLines(image, Detector::LSD, lines);
  }
}
void LineDetector::detectLines(const cv::Mat& image, Detector detector,
                               std::vector<cv::Vec4f>* lines) {
  CHECK_NOTNULL(lines);
  lines->clear();
  // Check which detector is chosen by user. If an invalid number is given the
  // default (LSD) is chosen without a warning.
  if (detector == Detector::LSD) {
    lsd_detector_->detect(image, *lines);
  } else if (detector == Detector::EDL) {  // EDL_DETECTOR
    // The edl detector uses a different kind of vector to store the lines in.
    // The conversion is done later.
    std::vector<cv::line_descriptor::KeyLine> edl_lines;
    edl_detector_->detect(image, edl_lines);

    // Write lines to standard format
    for (size_t i = 0u; i < edl_lines.size(); ++i) {
      lines->push_back(cv::Vec4f(
          edl_lines[i].getStartPoint().x, edl_lines[i].getStartPoint().y,
          edl_lines[i].getEndPoint().x, edl_lines[i].getEndPoint().y));
    }

  } else if (detector == Detector::FAST) {  // FAST_DETECTOR
    fast_detector_->detect(image, *lines);
  } else if (detector == Detector::HOUGH) {  // HOUGH_DETECTOR
    cv::Mat output;
    // Parameters of the Canny should not be changed (or better: the result is
    // very likely to get worse).
    cv::Canny(image, output, params_->canny_edges_threshold1,
              params_->canny_edges_threshold2, params_->canny_edges_aperture);
    // Here parameter changes might improve the result.
    cv::HoughLinesP(output, *lines, params_->hough_detector_rho,
                    params_->hough_detector_theta,
                    params_->hough_detector_threshold,
                    params_->hough_detector_minLineLength,
                    params_->hough_detector_maxLineGap);
  }
}
void LineDetector::detectLines(const cv::Mat& image,
                               std::vector<cv::Vec4f>* lines) {
  detectLines(image, Detector::LSD, lines);
}

bool LineDetector::hessianNormalFormOfPlane(
    const std::vector<cv::Vec3f>& points, cv::Vec4f* hessian_normal_form) {
  CHECK_NOTNULL(hessian_normal_form);
  const int num_points = points.size();
  CHECK(num_points >= 3);
  if (num_points == 3) {  // In this case an exact solution can be computed.
    cv::Vec3f vec1 = points[1] - points[0];
    cv::Vec3f vec2 = points[2] - points[0];
    // This checks first if the points were too close.
    double norms = cv::norm(vec1) * cv::norm(vec2);
    if (norms < params_->min_distance_between_points_hessian) return false;
    // Then if they lie on a line. The angle between the vectors must at least
    // be 2 degrees.
    double cos_theta = fabs(vec1.dot(vec2)) / norms;
    if (cos_theta > params_->max_cos_theta_hessian_computation) return false;
    // The normal already defines the orientation of the plane (it is
    // perpendicular to both vectors, since they must lie within the plane).
    cv::Vec3f normal = vec1.cross(vec2);
    // Now bring the plane into the hessian normal form.
    *hessian_normal_form =
        cv::Vec4f(normal[0], normal[1], normal[2],
                  computeDfromPlaneNormal(normal, points[0]));
    *hessian_normal_form = (*hessian_normal_form) / cv::norm(normal);
    return true;
  } else {  // If there are more than 3 points, the solution is approximate.
    cv::Vec3f mean(0.0f, 0.0f, 0.0f);
    for (size_t i = 0; i < num_points; ++i) {
      mean += points[i] / num_points;
    }
    cv::Mat A(3, num_points, CV_64FC1);
    for (size_t i = 0; i < num_points; ++i) {
      A.at<double>(0, i) = points[i][0] - mean[0];
      A.at<double>(1, i) = points[i][1] - mean[1];
      A.at<double>(2, i) = points[i][2] - mean[2];
    }
    cv::Mat U, W, Vt;
    cv::SVD::compute(A, W, U, Vt);
    cv::Vec3f normal;
    if (U.type() == CV_64FC1) {
      normal =
          cv::Vec3f(U.at<double>(0, 2), U.at<double>(1, 2), U.at<double>(2, 2));
    } else if (U.type() == CV_32FC1) {
      normal =
          cv::Vec3f(U.at<float>(0, 2), U.at<float>(1, 2), U.at<float>(2, 2));
    }
    *hessian_normal_form = cv::Vec4f(normal[0], normal[1], normal[2],
                                     computeDfromPlaneNormal(normal, mean));
    return true;
  }
}

void LineDetector::projectLines2Dto3D(const std::vector<cv::Vec4f>& lines2D,
                                      const cv::Mat& point_cloud,
                                      std::vector<cv::Vec6f>* lines3D) {
  CHECK_NOTNULL(lines3D);
  // First check if the point_cloud mat has the right format.
  CHECK_EQ(point_cloud.type(), CV_32FC3)
      << "The input matrix point_cloud must be of type CV_32FC3.";
  lines3D->clear();
  cv::Point2i start, end;
  for (size_t i = 0; i < lines2D.size(); ++i) {
    start.x = floor(lines2D[i][0]);
    start.y = floor(lines2D[i][1]);
    end.x = floor(lines2D[i][2]);
    end.y = floor(lines2D[i][3]);
    if (!std::isnan(point_cloud.at<cv::Vec3f>(start)[0]) &&
        !std::isnan(point_cloud.at<cv::Vec3f>(end)[0])) {
      lines3D->push_back(cv::Vec6f(point_cloud.at<cv::Vec3f>(start)[0],
                                   point_cloud.at<cv::Vec3f>(start)[1],
                                   point_cloud.at<cv::Vec3f>(start)[2],
                                   point_cloud.at<cv::Vec3f>(end)[0],
                                   point_cloud.at<cv::Vec3f>(end)[1],
                                   point_cloud.at<cv::Vec3f>(end)[2]));
    }
  }
}

void LineDetector::fuseLines2D(const std::vector<cv::Vec4f>& lines_in,
                               std::vector<cv::Vec4f>* lines_out) {
  CHECK_NOTNULL(lines_out);
  lines_out->clear();
  // This list is used to remember which lines have already been assigned to a
  // cluster. Every time a line is assigned, the corresponding index is
  // deleted in this list.
  std::list<int> line_index;
  for (size_t i = 0; i < lines_in.size(); ++i) line_index.push_back(i);
  // This vector is used to store the line clusters until they are merged into
  // one line.
  std::vector<cv::Vec4f> line_cluster;
  // Iterate over all lines.
  for (size_t i = 0; i < lines_in.size(); ++i) {
    line_cluster.clear();
    // If this condition does not hold, the line lines_in[i] has already been
    // merged into a new one. If not, the algorithm tries to find lines that
    // are near this line.
    if (*(line_index.begin()) != i) {
      continue;
    } else {
      line_cluster.push_back(lines_in[i]);
      line_index.pop_front();
    }
    for (std::list<int>::iterator it = line_index.begin();
         it != line_index.end(); ++it) {
      // This loop checks if the line is near any line in the momentary
      // cluster. If yes, it assignes it to the cluster.
      for (cv::Vec4f& line : line_cluster) {
        if (areLinesEqual2D(line, lines_in[*it])) {
          line_cluster.push_back(lines_in[*it]);
          it = line_index.erase(it);
          break;
        }
      }
    }
    // If the cluster size is one, then no cluster was found.
    if (line_cluster.size() == 1) {
      lines_out->push_back(line_cluster[0]);
      continue;
    }
    // Here all the lines of a cluster are merged into one.
    int x_min = 1e4, x_max = 0, y_min = 1e4, y_max = 0, slope = 0;
    for (cv::Vec4f& line : line_cluster) {
      if (line[0] < x_min) x_min = line[0];
      if (line[0] > x_max) x_max = line[0];
      if (line[1] < y_min) y_min = line[1];
      if (line[1] > y_max) y_max = line[1];
      if (line[2] < x_min) x_min = line[2];
      if (line[2] > x_max) x_max = line[2];
      if (line[3] < y_min) y_min = line[3];
      if (line[3] > y_max) y_max = line[3];
      slope += computeSlopeOfLine(line);
    }
    if (slope > 0)
      lines_out->push_back(cv::Vec4f(x_min, y_min, x_max, y_max));
    else
      lines_out->push_back(cv::Vec4f(x_min, y_max, x_max, y_min));
  }
}

void LineDetector::paintLines(const std::vector<cv::Vec4f>& lines,
                              cv::Mat* image, cv::Vec3b color) {
  cv::Point2i p1, p2;

  for (int i = 0; i < lines.size(); i++) {
    p1.x = lines[i][0];
    p1.y = lines[i][1];
    p2.x = lines[i][2];
    p2.y = lines[i][3];

    cv::line(*image, p1, p2, color, 2);
  }
}

bool LineDetector::find3DLineStartAndEnd(const cv::Mat& point_cloud,
                                         const cv::Vec4f& line2D,
                                         cv::Vec6f* line3D) {
  CHECK_NOTNULL(line3D);
  CHECK_EQ(point_cloud.type(), CV_32FC3)
      << "The input matrix point_cloud must be of type CV_32FC3.";
  cv::Point2f start, end, line;
  // A floating point value that decribes a position in an image is always
  // within the pixel described through the floor operation.
  start.x = floor(line2D[0]);
  start.y = floor(line2D[1]);
  end.x = floor(line2D[2]);
  end.y = floor(line2D[3]);
  // Search for a non NaN value on the line. Effectivly these two while loops
  // just make unit steps (one pixel) from start to end (first loop) and then
  // from end to start (second loop) until a non NaN point is found.
  while (std::isnan(point_cloud.at<cv::Vec3f>(start)[0])) {
    line = end - start;
    start.x += floor(line.x / sqrt(line.x * line.x + line.y * line.y) + 0.5);
    start.y += floor(line.y / sqrt(line.x * line.x + line.y * line.y) + 0.5);
    if (start.x == end.x && start.y == end.y) break;
  }
  if (start.x == end.x && start.y == end.y) return false;
  while (std::isnan(point_cloud.at<cv::Vec3f>(end)[0])) {
    line = start - end;
    end.x += floor(line.x / sqrt(line.x * line.x + line.y * line.y) + 0.5);
    end.y += floor(line.y / sqrt(line.x * line.x + line.y * line.y) + 0.5);
    if (start.x == end.x && start.y == end.y) break;
  }
  if (start.x == end.x && start.y == end.y) return false;
  // If a non NaN point was found before end was reached, the line is stored.
  *line3D = cv::Vec6f(
      point_cloud.at<cv::Vec3f>(start)[0], point_cloud.at<cv::Vec3f>(start)[1],
      point_cloud.at<cv::Vec3f>(start)[2], point_cloud.at<cv::Vec3f>(end)[0],
      point_cloud.at<cv::Vec3f>(end)[1], point_cloud.at<cv::Vec3f>(end)[2]);
  return true;
}

double LineDetector::findAndRate3DLine(const cv::Mat& point_cloud,
                                       const cv::Vec4f& line2D,
                                       cv::Vec6f* line3D, int* num_inliers) {
  CHECK_NOTNULL(line3D);
  CHECK_NOTNULL(num_inliers);
  CHECK_EQ(point_cloud.type(), CV_32FC3)
      << "The input matrix point_cloud must be of type CV_32FC3.";
  cv::Point2f start, end, line, rate_it;
  // A floating point value that decribes a position in an image is always
  // within the pixel described through the floor operation.
  start.x = floor(line2D[0]);
  start.y = floor(line2D[1]);
  end.x = floor(line2D[2]);
  end.y = floor(line2D[3]);
  // Search for a non NaN value on the line.
  while (std::isnan(point_cloud.at<cv::Vec3f>(start)[0])) {
    line = end - start;
    start.x += floor(line.x / sqrt(line.x * line.x + line.y * line.y) + 0.5);
    start.y += floor(line.y / sqrt(line.x * line.x + line.y * line.y) + 0.5);
    if (start.x == end.x && start.y == end.y) break;
  }
  if (start.x == end.x && start.y == end.y) return 1e9;
  while (std::isnan(point_cloud.at<cv::Vec3f>(end)[0])) {
    line = start - end;
    end.x += floor(line.x / sqrt(line.x * line.x + line.y * line.y) + 0.5);
    end.y += floor(line.y / sqrt(line.x * line.x + line.y * line.y) + 0.5);
    if (start.x == end.x && start.y == end.y) break;
  }
  if (start.x == end.x && start.y == end.y) return 1e9;
  *line3D = cv::Vec6f(
      point_cloud.at<cv::Vec3f>(start)[0], point_cloud.at<cv::Vec3f>(start)[1],
      point_cloud.at<cv::Vec3f>(start)[2], point_cloud.at<cv::Vec3f>(end)[0],
      point_cloud.at<cv::Vec3f>(end)[1], point_cloud.at<cv::Vec3f>(end)[2]);

  // In addition to find3DLineStartAndEnd, this function also rates the line.
  // The rating can only be seen as relative to other similar lines, but not in
  // a global perspective. It bases on the mean error of points that should
  // (coming from the 2D image) lie on the line.
  double rating = 0.0;
  double rating_temp;
  double inlier_threshold = 0.01;
  int min_inliers = 5;
  *num_inliers = 0;
  rate_it = start;
  while (!(rate_it.x == end.x && rate_it.y == end.y)) {
    line = end - rate_it;
    rate_it.x += floor(line.x / sqrt(line.x * line.x + line.y * line.y) + 0.5);
    rate_it.y += floor(line.y / sqrt(line.x * line.x + line.y * line.y) + 0.5);
    if (std::isnan(point_cloud.at<cv::Vec3f>(rate_it)[0])) continue;
    rating_temp = distPointToLine(point_cloud.at<cv::Vec3f>(start),
                                  point_cloud.at<cv::Vec3f>(end),
                                  point_cloud.at<cv::Vec3f>(rate_it));
    if (rating_temp < inlier_threshold) {
      rating += rating_temp;
      ++(*num_inliers);
    }
  }
  if ((*num_inliers) < min_inliers)
    return 1e9;
  else
    return rating / (*num_inliers);
}
double LineDetector::findAndRate3DLine(const cv::Mat& point_cloud,
                                       const cv::Vec4f& line2D,
                                       cv::Vec6f* line3D) {
  int num_inliers;
  return findAndRate3DLine(point_cloud, line2D, line3D, &num_inliers);
}

std::vector<cv::Vec4f> LineDetector::checkLinesInBounds(
    const std::vector<cv::Vec4f>& lines2D, size_t x_max, size_t y_max) {
  CHECK(x_max > 0);
  CHECK(y_max > 0);
  std::vector<cv::Vec4f> new_lines;
  new_lines.reserve(lines2D.size());
  double x_bound = static_cast<double>(x_max) - 1e-9;
  double y_bound = static_cast<double>(y_max) - 1e-9;
  for (size_t i = 0; i < lines2D.size(); ++i) {
    new_lines.push_back({checkInBoundary(lines2D[i][0], 0, x_bound),
                         checkInBoundary(lines2D[i][1], 0, y_bound),
                         checkInBoundary(lines2D[i][2], 0, x_bound),
                         checkInBoundary(lines2D[i][3], 0, y_bound)});
  }
  return new_lines;
}

bool LineDetector::getRectanglesFromLine(const cv::Vec4f& line,
                                         std::vector<cv::Point2f>* rect_left,
                                         std::vector<cv::Point2f>* rect_right) {
  CHECK_NOTNULL(rect_right);
  CHECK_NOTNULL(rect_left);
  // The offset defines how far away from the line the nearest corner points
  // are.
  double offset = params_->rectangle_offset_pixels;
  double relative_rect_size = params_->max_relative_rect_size;
  // Defines the length of the side perpendicular to the line.
  // Exactly as above, but defines a numerical maximum.
  double max_rect_size = params_->max_absolute_rect_size;
  double eff_rect_size = max_rect_size;
  cv::Point2f start(line[0], line[1]);
  cv::Point2f end(line[2], line[3]);
  cv::Point2f line_dir = end - start;
  cv::Point2f go_left(-line_dir.y, line_dir.x);
  cv::Point2f go_right(line_dir.y, -line_dir.x);
  double norm = sqrt(pow(line_dir.x, 2) + pow(line_dir.y, 2));
  if (eff_rect_size > norm * relative_rect_size)
    eff_rect_size = norm * relative_rect_size;
  rect_left->resize(4);
  (*rect_left)[0] = start + offset / norm * go_left;
  (*rect_left)[1] = start + eff_rect_size / norm * go_left;
  (*rect_left)[2] = end + offset / norm * go_left;
  (*rect_left)[3] = end + eff_rect_size / norm * go_left;
  rect_right->resize(4);
  (*rect_right)[0] = start + offset / norm * go_right;
  (*rect_right)[1] = start + eff_rect_size / norm * go_right;
  (*rect_right)[2] = end + offset / norm * go_right;
  (*rect_right)[3] = end + eff_rect_size / norm * go_right;
}

void LineDetector::assignColorToLines(const cv::Mat& image,
                                      const std::vector<cv::Point2i> points,
                                      LineWithPlanes* line3D) {
  CHECK_NOTNULL(line3D);
  CHECK_EQ(image.type(), CV_8UC3);
  long long x1 = 0, x2 = 0, x3 = 0;
  int num_points = points.size();
  for (size_t i = 0; i < points.size(); ++i) {
    if (points[i].x < 0 || points[i].x >= image.cols || points[i].y < 0 ||
        points[i].y >= image.rows) {
      continue;
    }
    x1 += image.at<cv::Vec3b>(points[i])[0];
    x2 += image.at<cv::Vec3b>(points[i])[1];
    x3 += image.at<cv::Vec3b>(points[i])[2];
  }
  line3D->colors.push_back({x1 / num_points, x2 / num_points, x3 / num_points});
}

bool LineDetector::find3DlineOnPlanes(const std::vector<cv::Vec3f>& points1,
                                      const std::vector<cv::Vec3f>& points2,
                                      const cv::Vec6f& line_guess,
                                      cv::Vec6f* line) {
  LineWithPlanes line_with_planes;
  if (find3DlineOnPlanes(points1, points2, line_guess, &line_with_planes)) {
    *line = line_with_planes.line;
    return true;
  } else {
    return false;
  }
}
bool LineDetector::find3DlineOnPlanes(const std::vector<cv::Vec3f>& points1,
                                      const std::vector<cv::Vec3f>& points2,
                                      const cv::Vec6f& line_guess,
                                      LineWithPlanes* line) {
  CHECK_NOTNULL(line);
  size_t N1 = points1.size();
  size_t N2 = points2.size();
  if (N1 < 3 || N2 < 3) return false;
  cv::Vec3f mean1, mean2, normal1, normal2;
  line->hessians.resize(2);
  // cv::Vec4f hessian1, hessian2;
  // Fit a plane model to the two sets of points individually.
  if (!hessianNormalFormOfPlane(points1, &(line->hessians[0])))
    LOG(WARNING) << "find3DlineOnPlanes: search for hessian failed.";
  if (!hessianNormalFormOfPlane(points2, &(line->hessians[1])))
    LOG(WARNING) << "find3DlineOnPlanes: search for hessian failed.";
  // Extract the two plane normals.
  normal1 = {line->hessians[0][0], line->hessians[0][1], line->hessians[0][2]};
  normal2 = {line->hessians[1][0], line->hessians[1][1], line->hessians[1][2]};
  // This parameter defines at which point 2 lines are concerned to be near.
  // This distance is computed from the means of the two set of points. If the
  // distance is higher than this value, it is assumed that the line is not
  // the intersection of the two planes, but just lies on the one that is in
  // the foreground.
  constexpr double angle_difference = 0.995;
  mean1 = computeMean(points1);
  mean2 = computeMean(points2);
  if (cv::norm(mean1 - mean2) < params_->max_dist_between_planes) {
    // Checks if the planes are parallel.
    if (fabs(normal1.dot(normal2)) > angle_difference) {
      line->line = line_guess;
      line->type = LineType::PLANE;
      return true;
    } else {
      // The line lying on both planes must be perpendicular to both normals,
      // so it can be computed with the cross product.
      cv::Vec3f direction = normal1.cross(normal2);
      normalizeVector3D(&direction);
      // Now a point on the intersection line is searched.
      cv::Vec3f x_0;
      getPointOnPlaneIntersectionLine(line->hessians[0], line->hessians[1],
                                      direction, &x_0);
      // This part searches for start and end point, because so far we only
      // have a line from and to inifinity. The procedure used here projects
      // all points in both sets onto the line and then chooses the pair of
      // points that maximizes the distance of the line.
      double dist;
      double dist_min = 1e9;
      double dist_max = -1e9;
      for (size_t i = 0; i < N1; ++i) {
        dist = direction.dot(points1[i] - x_0);
        if (dist < dist_min) dist_min = dist;
        if (dist > dist_max) dist_max = dist;
      }
      for (size_t i = 0; i < N2; ++i) {
        dist = direction.dot(points2[i] - x_0);
        if (dist < dist_min) dist_min = dist;
        if (dist > dist_max) dist_max = dist;
      }
      cv::Vec3f start, end;
      start = x_0 + direction * dist_min;
      end = x_0 + direction * dist_max;
      line->line = {start[0], start[1], start[2], end[0], end[1], end[2]};
      line->type = LineType::INTER;
      return true;
    }
  } else {
    // If we reach this point, we have a discontinouty. We then try to fit a
    // line to the set of points that lies closer to the origin (and therefore
    // closer to the camera). This is in most cases a reasonable assumption,
    // since the line most of the time belongs to the object that obscures the
    // background (which causes the discontinouty).
    // The fitting is done in 3 steps:
    //  1.  Project the line_guess onto the plane fitted to the point set.
    //  2.  Find the line parallel to the projected one that goes through the
    //      point in the set of the points that is nearest to the line.
    //  3.  From all points in the set: Project them on to the line and choose
    //      the combination of start/end point that maximizes the line distance.
    cv::Vec3f start(line_guess[0], line_guess[1], line_guess[2]);
    cv::Vec3f end(line_guess[3], line_guess[4], line_guess[5]);
    cv::Vec3f direction = end - start;
    normalizeVector3D(&direction);

    const std::vector<cv::Vec3f>* points_new;
    int idx;
    if (cv::norm(mean1) < cv::norm(mean2)) {
      idx = 0;
      points_new = &points1;
    } else {
      idx = 1;
      points_new = &points2;
    }
    line->hessians[abs(idx - 1)] = {0.0f, 0.0f, 0.0f, 0.0f};
    start = projectPointOnPlane(line->hessians[idx], start);
    end = projectPointOnPlane(line->hessians[idx], end);
    cv::Vec3f nearest_point;
    double min_dist = 1e9;
    double dist, dist_dir, dist_dir_min, dist_dir_max;
    dist_dir_min = 1e9;
    dist_dir_max = -1e9;
    for (size_t i = 0; i < points_new->size(); ++i) {
      // dist is used to find the nearest point to the line.
      dist =
          cv::norm(start - (*points_new)[i]) + cv::norm(end - (*points_new)[i]);
      if (dist < min_dist) {
        min_dist = dist;
        nearest_point = (*points_new)[i];
      }
      // dist_dir is used to find the points that maximize the line.
      dist_dir = direction.dot((*points_new)[i] - start);
      if (dist_dir < dist_dir_min) dist_dir_min = dist_dir;
      if (dist_dir > dist_dir_max) dist_dir_max = dist_dir;
    }
    cv::Vec3f x_0 = projectPointOnLine(nearest_point, direction, start);
    start = x_0 + direction * dist_dir_min;
    end = x_0 + direction * dist_dir_max;
    line->line = {start[0], start[1], start[2], end[0], end[1], end[2]};
    line->type = LineType::DISCONT;
    return true;
  }
}

bool LineDetector::planeRANSAC(const std::vector<cv::Vec3f>& points,
                               cv::Vec4f* hessian_normal_form) {
  const size_t N = points.size();
  double inlier_fraction_min = params_->min_inlier_ransac;
  std::vector<cv::Vec3f> inliers;
  planeRANSAC(points, &inliers);
  // If we found not enough inlier, return false. This is important because
  // there might not be a solution (and we dont want to propose one if there
  // is none).
  if (inliers.size() <= inlier_fraction_min * N) return false;
  // Now we compute the final model parameters with all the inliers.
  return hessianNormalFormOfPlane(inliers, hessian_normal_form);
}
void LineDetector::planeRANSAC(const std::vector<cv::Vec3f>& points,
                               std::vector<cv::Vec3f>* inliers) {
  CHECK_NOTNULL(inliers);
  // Set parameters and do a sanity check.
  const int N = points.size();
  inliers->clear();
  // ROS_INFO("N = %d", N);
  const int max_it = params_->num_iter_ransac;
  constexpr int number_of_model_params = 3;
  double max_deviation = params_->max_error_inlier_ransac;
  double inlier_fraction_max = params_->inlier_max_ransac;
  CHECK(N > number_of_model_params) << "Not enough points to use RANSAC.";
  // Declare variables that are used for the RANSAC.
  std::vector<cv::Vec3f> random_points, inlier_candidates;
  cv::Vec3f normal;
  cv::Vec4f hessian_normal_form;
  // Set a random seed.
  unsigned seed =
      std::chrono::high_resolution_clock::now().time_since_epoch().count();
  std::default_random_engine generator(seed);
  // Start RANSAC.
  for (int iter = 0; iter < max_it; ++iter) {
    // Get number_of_model_params unique elements from poitns.
    getNUniqueRandomElements(points, number_of_model_params, &generator,
                             &random_points);
    // It might happen that the randomly chosen points lie on a line. In this
    // case, hessianNormalFormOfPlane would return false.
    if (!hessianNormalFormOfPlane(random_points, &hessian_normal_form))
      continue;
    normal = cv::Vec3f(hessian_normal_form[0], hessian_normal_form[1],
                       hessian_normal_form[2]);
    // Check which of the points are inlier with the current plane model.
    inlier_candidates.clear();
    for (int j = 0; j < N; ++j) {
      if (errorPointToPlane(hessian_normal_form, points[j]) < max_deviation) {
        inlier_candidates.push_back(points[j]);
      }
    }
    // If we found more inliers than in any previous run, we store them
    // as global inliers.
    if (inlier_candidates.size() > inliers->size()) {
      *inliers = inlier_candidates;
    }

    // Usual not part of RANSAC: stop early if we have enough inliers.
    // This feature is here because it might be that we have a very
    // high inlier percentage. In this case RANSAC finds the right
    // model within the first few iterations and all later iterations
    // are just wasted run time.
    if (inliers->size() > inlier_fraction_max * N) break;
  }
}

void LineDetector::project2Dto3DwithPlanes(
    const cv::Mat& cloud, const std::vector<cv::Vec4f>& lines2D,
    std::vector<cv::Vec6f>* lines3D) {
  CHECK_NOTNULL(lines3D);
  std::vector<LineWithPlanes> lines_with_planes;
  lines3D->clear();
  project2Dto3DwithPlanes(cloud, lines2D, &lines_with_planes);
  for (size_t i = 0; i < lines_with_planes.size(); ++i)
    lines3D->push_back(lines_with_planes[i].line);
}
void LineDetector::project2Dto3DwithPlanes(
    const cv::Mat& cloud, const std::vector<cv::Vec4f>& lines2D,
    std::vector<LineWithPlanes>* lines3D) {
  cv::Mat image;
  project2Dto3DwithPlanes(cloud, image, lines2D, lines3D, false);
}
void LineDetector::project2Dto3DwithPlanes(
    const cv::Mat& cloud, const cv::Mat& image,
    const std::vector<cv::Vec4f>& lines2D_in,
    std::vector<LineWithPlanes>* lines3D, bool set_colors) {
  CHECK_NOTNULL(lines3D);
  CHECK_EQ(cloud.type(), CV_32FC3);
  // Declare all variables before the main loop.
  std::vector<cv::Point2f> rect_left, rect_right;
  std::vector<cv::Point2i> points_in_rect;
  std::vector<cv::Vec3f> plane_point_cand, inliers_left, inliers_right;
  std::vector<cv::Vec6f> lines3D_cand;
  std::vector<double> rating;
  cv::Point2i start, end;
  LineWithPlanes line3D_true;
  // Parameter: Fraction of inlier that must be found for the plane model to
  // be valid.
  double min_inliers = params_->min_inlier_ransac;
  double max_rating = params_->max_rating_valid_line;
  bool right_found, left_found;
  constexpr size_t min_points_for_ransac = 3;
  // This is a first guess of the 3D lines. They are used in some cases, where
  // the lines cannot be found by intersecting planes.
  std::vector<cv::Vec4f> lines2D =
      checkLinesInBounds(lines2D_in, cloud.cols, cloud.rows);
  find3DlinesRated(cloud, lines2D, &lines3D_cand, &rating);
  // Loop over all 2D lines.
  for (size_t i = 0; i < lines2D.size(); ++i) {
    // If the rating is so high, no valid 3d line was found by the
    // find3DlinesRated function.
    if (rating[i] > max_rating) continue;
    // For both the left and the right side of the line: Find a rectangle
    // defining a patch, find all points within the patch and try to fit a
    // plane to these points.
    getRectanglesFromLine(lines2D[i], &rect_left, &rect_right);
    // Find lines for the left side.
    findPointsInRectangle(rect_left, &points_in_rect);
    if (set_colors) {
      assignColorToLines(image, points_in_rect, &line3D_true);
    }
    plane_point_cand.clear();
    for (size_t j = 0; j < points_in_rect.size(); ++j) {
      if (points_in_rect[j].x < 0 || points_in_rect[j].x >= cloud.cols ||
          points_in_rect[j].y < 0 || points_in_rect[j].y >= cloud.rows) {
        continue;
      }
      if (std::isnan(cloud.at<cv::Vec3f>(points_in_rect[j])[0])) continue;
      plane_point_cand.push_back(cloud.at<cv::Vec3f>(points_in_rect[j]));
    }
    left_found = false;
    if (plane_point_cand.size() > min_points_for_ransac) {
      planeRANSAC(plane_point_cand, &inliers_left);
      if (inliers_left.size() >= min_inliers * plane_point_cand.size()) {
        left_found = true;
      }
    }
    // Find lines for the right side.
    findPointsInRectangle(rect_right, &points_in_rect);
    if (set_colors) {
      assignColorToLines(image, points_in_rect, &line3D_true);
    }
    plane_point_cand.clear();
    for (size_t j = 0; j < points_in_rect.size(); ++j) {
      if (points_in_rect[j].x < 0 || points_in_rect[j].x >= cloud.cols ||
          points_in_rect[j].y < 0 || points_in_rect[j].y >= cloud.rows) {
        continue;
      }
      if (std::isnan(cloud.at<cv::Vec3f>(points_in_rect[j])[0])) continue;
      plane_point_cand.push_back(cloud.at<cv::Vec3f>(points_in_rect[j]));
    }
    right_found = false;
    if (plane_point_cand.size() > min_points_for_ransac) {
      planeRANSAC(plane_point_cand, &inliers_right);
      if (inliers_right.size() >= min_inliers * plane_point_cand.size()) {
        right_found = true;
      }
    }
    // If any of planes were not found, the line is found at a discontinouty.
    // This is a workaround, more efficiently this would be implemented in the
    // function find3DlineOnPlanes.
    bool is_discont = true;
    if ((!right_found) && (!left_found)) {
      continue;
    } else if (!right_found) {
      inliers_right = inliers_left;
    } else if (!left_found) {
      inliers_left = inliers_right;
    } else {
      is_discont = false;
    }
    // If both planes were found, the inliers are handled to the
    // find3DlineOnPlanes function, which takes care of different special
    // cases.
    if (find3DlineOnPlanes(inliers_right, inliers_left, lines3D_cand[i],
                           &line3D_true)) {
      // Only push back the reliably found lines.
      if (is_discont) {
        line3D_true.type = LineType::DISCONT;
        if (right_found) {
          line3D_true.hessians[1] = {0.0f, 0.0f, 0.0f, 0.0f};
        } else {
          line3D_true.hessians[0] = {0.0f, 0.0f, 0.0f, 0.0f};
        }
      }
      lines3D->push_back(line3D_true);
    }
  }
}

void LineDetector::find3DlinesByShortest(const cv::Mat& cloud,
                                         const std::vector<cv::Vec4f>& lines2D,
                                         std::vector<cv::Vec6f>* lines3D) {
  std::vector<int> correspondeces;
  find3DlinesByShortest(cloud, lines2D, lines3D, &correspondeces);
}
void LineDetector::find3DlinesByShortest(const cv::Mat& cloud,
                                         const std::vector<cv::Vec4f>& lines2D,
                                         std::vector<cv::Vec6f>* lines3D,
                                         std::vector<int>* correspondeces) {
  CHECK_NOTNULL(lines3D);
  CHECK_NOTNULL(correspondeces);
  CHECK_EQ(cloud.type(), CV_32FC3);
  int cols = cloud.cols;
  int rows = cloud.rows;
  // The actual patch size wil be bigger. The number of pixels within a patch
  // is euqal to (2*patch_size + 1)^2. And because for every pixel in the
  // start patch the distance to every pixel in the end patch is computed, the
  // complexity is proportional to (2*patch_size + 1)^4.
  int patch_size = 1;
  int x_opt_start, y_opt_start, x_min_start, x_max_start, y_min_start,
      y_max_start, x_opt_end, y_opt_end, x_min_end, x_max_end, y_min_end,
      y_max_end;
  float dist, dist_opt;
  cv::Vec3f start, end;
  correspondeces->clear();
  lines3D->clear();
  for (size_t i; i < lines2D.size(); ++i) {
    dist_opt = 1e20;
    x_opt_start = lines2D[i][0];
    y_opt_start = lines2D[i][1];
    x_opt_end = lines2D[i][2];
    y_opt_end = lines2D[i][3];
    // This checks are used to make sure, that we do not try to access a
    // point not within the image.
    x_min_start = checkInBoundaryInt(x_opt_start - patch_size, 0, rows - 1);
    x_max_start = checkInBoundaryInt(x_opt_start + patch_size, 0, rows - 1);
    y_min_start = checkInBoundaryInt(y_opt_start - patch_size, 0, cols - 1);
    y_max_start = checkInBoundaryInt(y_opt_start + patch_size, 0, cols - 1);
    x_min_end = checkInBoundaryInt(x_opt_end - patch_size, 0, rows - 1);
    x_max_end = checkInBoundaryInt(x_opt_end + patch_size, 0, rows - 1);
    y_min_end = checkInBoundaryInt(y_opt_end - patch_size, 0, cols - 1);
    y_max_end = checkInBoundaryInt(y_opt_end + patch_size, 0, cols - 1);
    // For every pixel in start patch.
    for (int x_start = x_min_start; x_start <= x_max_start; ++x_start) {
      for (int y_start = y_min_start; y_start <= y_max_start; ++y_start) {
        // For every pixel in end patch.
        for (int x_end = x_min_end; x_end <= x_max_end; ++x_end) {
          for (int y_end = y_min_end; y_end <= y_max_end; ++y_end) {
            // Check that the corresponding 3D point is not NaN.
            if (std::isnan(cloud.at<cv::Vec3f>(y_start, x_start)[0]) ||
                std::isnan(cloud.at<cv::Vec3f>(y_end, x_end)[0])) {
              continue;
            }
            // Compute distance and compare it to the optimal distance found
            // so far.
            start = cloud.at<cv::Vec3f>(y_start, x_start);
            end = cloud.at<cv::Vec3f>(y_end, x_end);
            dist = pow(start[0] - end[0], 2) + pow(start[1] - end[1], 2) +
                   pow(start[2] - end[2], 2);
            if (dist < dist_opt) {
              dist_opt = dist;
              x_opt_end = x_end;
              x_opt_start = x_start;
              y_opt_end = y_end;
              y_opt_start = y_start;
            }
          }
        }
      }
    }
    // Assuming that distances are in meters, we can safely assume that if our
    // optimal distance is still 1e20, no non-NaN points were found.
    if (dist_opt == 1e20) continue;
    // Otherwise, a line was found.
    start = cloud.at<cv::Vec3f>(y_opt_start, x_opt_start);
    end = cloud.at<cv::Vec3f>(y_opt_end, x_opt_end);
    lines3D->push_back(
        cv::Vec6f(start[0], start[1], start[2], end[0], end[1], end[2]));
    correspondeces->push_back(i);
  }
}

void LineDetector::find3DlinesRated(const cv::Mat& cloud,
                                    const std::vector<cv::Vec4f>& lines2D,
                                    std::vector<cv::Vec6f>* lines3D,
                                    std::vector<double>* rating) {
  CHECK_NOTNULL(lines3D);
  CHECK_NOTNULL(rating);
  CHECK_EQ(cloud.type(), CV_32FC3);
  int cols = cloud.cols;
  int rows = cloud.rows;
  cv::Vec4f upper_line2D, lower_line2D;
  cv::Point2f line;
  cv::Vec6f lower_line3D, line3D, upper_line3D;
  double rate_low, rate_mid, rate_up;
  lines3D->clear();
  lines3D->reserve(lines2D.size());
  rating->clear();
  rating->reserve(lines2D.size());
  for (size_t i = 0; i < lines2D.size(); ++i) {
    line.x = lines2D[i][2] - lines2D[i][0];
    line.y = lines2D[i][3] - lines2D[i][1];
    double line_normalizer = sqrt(line.x * line.x + line.y * line.y);
    upper_line2D[0] = checkInBoundary(
        floor(lines2D[i][0]) + floor(line.y / line_normalizer + 0.5), 0.0,
        cols - 1);
    upper_line2D[1] = checkInBoundary(
        floor(lines2D[i][1]) + floor(-line.x / line_normalizer + 0.5), 0.0,
        rows - 1);
    upper_line2D[2] = checkInBoundary(
        floor(lines2D[i][2]) + floor(line.y / line_normalizer + 0.5), 0.0,
        cols - 1);
    upper_line2D[3] = checkInBoundary(
        floor(lines2D[i][3]) + floor(-line.x / line_normalizer + 0.5), 0.0,
        rows - 1);
    lower_line2D[0] = checkInBoundary(
        floor(lines2D[i][0]) + floor(-line.y / line_normalizer + 0.5), 0.0,
        cols - 1);
    lower_line2D[1] = checkInBoundary(
        floor(lines2D[i][1]) + floor(line.x / line_normalizer + 0.5), 0.0,
        rows - 1);
    lower_line2D[2] = checkInBoundary(
        floor(lines2D[i][2]) + floor(-line.y / line_normalizer + 0.5), 0.0,
        cols - 1);
    lower_line2D[3] = checkInBoundary(
        floor(lines2D[i][3]) + floor(line.x / line_normalizer + 0.5), 0.0,
        rows - 1);
    rate_low = findAndRate3DLine(cloud, lower_line2D, &lower_line3D);
    rate_mid = findAndRate3DLine(cloud, lines2D[i], &line3D);
    rate_up = findAndRate3DLine(cloud, upper_line2D, &upper_line3D);
    if (rate_up < rate_mid && rate_up < rate_low) {
      lines3D->push_back(upper_line3D);
      rating->push_back(rate_up);
    } else if (rate_low < rate_mid) {
      lines3D->push_back(lower_line3D);
      rating->push_back(rate_low);
    } else {
      lines3D->push_back(line3D);
      rating->push_back(rate_mid);
    }
  }
}
void LineDetector::find3DlinesRated(const cv::Mat& cloud,
                                    const std::vector<cv::Vec4f>& lines2D,
                                    std::vector<cv::Vec6f>* lines3D) {
  std::vector<double> rating;
  std::vector<cv::Vec6f> lines3D_cand;
  find3DlinesRated(cloud, lines2D, &lines3D_cand, &rating);
  for (size_t i = 0; i < lines3D_cand.size(); ++i) {
    if (rating[i] > params_->max_rating_valid_line) {
      continue;
    }
    lines3D->push_back(lines3D_cand[i]);
  }
}

void LineDetector::runCheckOn3DLines(const cv::Mat& cloud,
                                     const std::vector<cv::Vec6f>& lines3D_in,
                                     const int method,
                                     std::vector<cv::Vec6f>* lines3D_out) {
  CHECK_NOTNULL(lines3D_out);
  lines3D_out->clear();
  cv::Vec6f line_cand;
  for (size_t i = 0; i < lines3D_in.size(); ++i) {
    line_cand = lines3D_in[i];
    if (checkIfValidLineBruteForce(cloud, &line_cand)) {
      lines3D_out->push_back(line_cand);
    }
  }
}
void LineDetector::runCheckOn3DLines(
    const cv::Mat& cloud, const std::vector<LineWithPlanes>& lines3D_in,
    std::vector<LineWithPlanes>* lines3D_out) {
  lines3D_out->clear();
  LineWithPlanes line_cand;
  for (size_t i = 0; i < lines3D_in.size(); ++i) {
    line_cand = lines3D_in[i];
    if (checkIfValidLineBruteForce(cloud, &(line_cand.line))) {
      lines3D_out->push_back(line_cand);
    }
  }
}

void LineDetector::runCheckOn2DLines(const cv::Mat& cloud,
                                     const std::vector<cv::Vec4f>& lines2D_in,
                                     std::vector<cv::Vec4f>* lines2D_out) {
  CHECK_NOTNULL(lines2D_out);
  size_t N = lines2D_in.size();
  lines2D_out->clear();
  for (size_t i = 0; i < N; ++i) {
    if (checkIfValidLineDiscont(cloud, lines2D_in[i])) {
      lines2D_out->push_back(lines2D_in[i]);
    }
  }
}

bool LineDetector::checkIfValidLineBruteForce(const cv::Mat& cloud,
                                              cv::Vec6f* line) {
  CHECK_NOTNULL(line);
  CHECK_EQ(cloud.type(), CV_32FC3);
  // First check: if one of the points near exactly on the origin, get rid of
  // it.
  if ((fabs((*line)[0]) < 1e-3 && fabs((*line)[1]) < 1e-3 &&
       fabs((*line)[2]) < 1e-3) ||
      (fabs((*line)[3]) < 1e-3 && fabs((*line)[4]) < 1e-3 &&
       fabs((*line)[5]) < 1e-3)) {
    return false;
  }
  // Minimum number of inliers for the line to be valid.
  const int num_of_points_required = params_->min_points_in_line;
  // Maximum deviation for a point to count as an inlier.
  double max_deviation = params_->max_deviation_inlier_line_check;
  // This point density measures the where the points lie on the line. It is
  // used to truncate the line on the ends, if one end lies in empty space.
  int point_density[num_of_points_required] = {0};
  double dist;
  cv::Vec3f start, end, point;
  start = {(*line)[0], (*line)[1], (*line)[2]};
  end = {(*line)[3], (*line)[4], (*line)[5]};
  double length = cv::norm(start - end);
  int count_inliers = 0;
  // For every point in the cloud: This is why it is called brute force
  // approach.
  for (int i = 0; i < cloud.rows; ++i) {
    for (int j = 0; j < cloud.cols; ++j) {
      point = cloud.at<cv::Vec3f>(i, j);
      // Check if the distance to the line is below the threshold. This
      // computes the distance to the infinite line.
      if (distPointToLine(start, end, point) < max_deviation) {
        // This is the distance from the start point projected on to the line.
        // If its negative or larger the line length, the point may lie on the
        // line, but not between the start and the end point.
        dist = (end - start).dot(point - start) / length;
        if (dist < 0 || length <= dist) {
          continue;
        }
        // Now the histogramm like point_density is raised at the entry where
        // the point lies.
        point_density[(int)(dist / length * (double)num_of_points_required)] +=
            1;
        ++count_inliers;
      }
    }
  }
  // Only take lines with enough inliers.
  if (count_inliers <= num_of_points_required) {
    return false;
  }
  // Check from the front and the back of the line if the density is zero.
  int front = 0;
  int back = num_of_points_required - 1;
  while (0 == point_density[front]) ++front;
  while (0 == point_density[back]) --back;
  cv::Vec3f direction;
  direction = end - start;
  // This part will truncate the line, if the point_density was zero at either
  // the back or the front. Otherwise it has no influence.
  end = start + direction * back / (double)(num_of_points_required - 1);
  start = start + direction * front / (double)(num_of_points_required - 1);
  // Store the line and return.
  *line = {start[0], start[1], start[2], end[0], end[1], end[2]};
  return true;
}

bool LineDetector::checkIfValidLineDiscont(const cv::Mat& cloud,
                                           const cv::Vec4f& line) {
  CHECK_EQ(cloud.type(), CV_32FC3);
  cv::Point2i start, end, dir;
  start = {floor(line[0]), floor(line[1])};
  end = {floor(line[2]), floor(line[3])};
  int patch_size = 1;
  // The patch is restricted to be within the rectangle that is spawned by
  // start and end. This has two positive effects: We never try to acces a
  // pixel outside of the image and if a line starts at an discontinouty edge
  // it prevents the algorithm from early stopping.
  int x_max, y_max, x_min, y_min, x_from, x_to, y_from, y_to;
  if (line[0] < line[2]) {
    x_min = line[0];
    x_max = line[2];
  } else {
    x_min = line[2];
    x_max = line[0];
  }
  if (line[1] < line[3]) {
    y_min = line[1];
    y_max = line[3];
  } else {
    y_min = line[3];
    y_max = line[1];
  }
  cv::Vec3f current_mean(0, 0, 0);
  cv::Vec3f last_mean(0, 0, 0);
  double max_mean_diff = 0.1;
  int count;
  bool first_time = true;
  while (start != end) {
    current_mean = {0, 0, 0};
    // This procedure always makes a 1 pixel step towards the end point. It is
    // guaranteed to land on the end point eventually, so the loop will
    // terminate.
    dir = end - start;
    start.x += floor(dir.x / sqrt(dir.x * dir.x + dir.y * dir.y) + 0.5);
    start.y += floor(dir.y / sqrt(dir.x * dir.x + dir.y * dir.y) + 0.5);
    // We need to be within the boundaries defined previously.
    x_from = checkInBoundary(start.x - patch_size, x_min, x_max);
    x_to = checkInBoundary(start.x + patch_size, x_min, x_max);
    y_from = checkInBoundary(start.y - patch_size, y_min, y_max);
    y_to = checkInBoundary(start.y + patch_size, y_min, y_max);
    // Count is used to count the number of pixels that were added to the mean
    // so that we can effectively build the mean from the sum.
    count = 0;
    for (int i = x_from; i <= x_to; ++i) {
      for (int j = y_from; j <= y_to; ++j) {
        current_mean += cloud.at<cv::Vec3f>(j, i);
        ++count;
      }
    }
    current_mean = current_mean / count;
    if (first_time) {
      last_mean = current_mean;
      first_time = false;
      continue;
    }
    if (cv::norm(current_mean - last_mean) > max_mean_diff) return false;
    last_mean = current_mean;
  }
  return true;
}

void getCroppedImageForLines2D(const std::vector<cv::Vec4f>& lines2D, const cv::Mat& image){
  cv::Mat padded_image;
  int border_height = image.rows / 2;
  int border_width = image.cols / 2;

  cv::copyMakeBorder(image, padded_image, border_height, border_height,
    border_width, border_width, cv::BORDER_CONSTANT, cv::Scalar(0,0,0));

  float min_length = 1000;

  for(size_t i = 0; i < (int)lines2D.size(); ++i){
    float rect_height = sqrt((lines2D[i][0] - lines2D[i][2]) * (lines2D[i][0] - lines2D[i][2])
     + (lines2D[i][1] - lines2D[i][3]) * (lines2D[i][1] - lines2D[i][3]));
    float rect_width = rect_height;
    float rect_angle = -atan((lines2D[i][1] - lines2D[i][3]) / (lines2D[i][0] - lines2D[i][2])) * 180 / CV_PI;

    if(rect_height < min_length){ min_length = rect_height; }

    cv::Point2f center_point((lines2D[i][0] + lines2D[i][2])/2 + border_width,
     (lines2D[i][1] + lines2D[i][3])/2 + border_height);

    cv::RotatedRect rect(center_point, cv::Size2f(rect_width, rect_height), rect_angle);

    // if (rect.angle < -45.) {
    //     rect_angle += 90.0;
    //     std::swap(rect.size.width, rect.size.height);
    // }
    cv::Mat rotation_mat = cv::getRotationMatrix2D(rect.center, 180.0 - rect_angle, 1);

    // visualization
    // cv::Point2f p1, p2;
    //
    // p1.x = lines2D[i][0] + border_width;
    // p1.y = lines2D[i][1] + border_height;
    // p2.x = lines2D[i][2] + border_width;
    // p2.y = lines2D[i][3] + border_height;

    // if(i == 0 or i == 8 or i == 10){
    //   //cv::line(padded_image, p1, p2, cv::Vec3b(0, 255, 0), 2);
    //   cv::imshow("original lines", padded_image);
    //   //cv::imshow("cropped image", cropped_image);
    //   cv::waitKey();
    // }
    cv::Mat rotated_padded_image, cropped_image;
    cv::warpAffine(padded_image, rotated_padded_image, rotation_mat, padded_image.size(), cv::INTER_CUBIC);
    cv::getRectSubPix(rotated_padded_image, rect.size, rect.center, cropped_image);
    cv::imwrite("/home/chengkun/InternASL/catkin_ws/src/line_tools/data/lines_cropped_images/line_"
     + std::to_string(i) + ".jpg", cropped_image);

    // if(i == 0 or i == 8 or i == 10){
    //   cv::imshow("original lines", rotated_padded_image);
    //   //cv::imshow("cropped image", cropped_image);
    //   cv::waitKey();
    // }


  }
  std::cout << "min_length: " << min_length << '\n';

}

}  // namespace line_detection

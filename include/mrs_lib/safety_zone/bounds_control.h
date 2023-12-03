#ifndef BOUNDS_CONTROL_H
#define BOUNDS_CONTROL_H

#include "prism.h"

#include <interactive_markers/interactive_marker_server.h>
#include <interactive_markers/menu_handler.h>
#include <visualization_msgs/InteractiveMarkerFeedback.h>
#include <visualization_msgs/Marker.h>
#include <string>
#include <ros/ros.h>

namespace mrs_lib
{
class BoundsControl : public Subscriber{
public:
  BoundsControl(Prism* prism, std::string frame_id, ros::NodeHandle nh);
  void update();

private:
  void addBoundIntMarker(bool is_upper);
  visualization_msgs::Marker makeBox(visualization_msgs::InteractiveMarker &msg);
  void boundMoveCallback(const visualization_msgs::InteractiveMarkerFeedbackConstPtr &feedback);

  // It is required for generating interactive marker names, 
  // because their names must be unique on topic 
  static int id_generator;

  const int id;

  Prism* prism_;
  std::string frame_id_;
  ros::NodeHandle nh_;

  interactive_markers::InteractiveMarkerServer* server_ = nullptr;
  std::string upper_name_;
  std::string lower_name_;
  
}; // class BoundsControl
} // namespace mrs_lib

#endif
#include <QMainWindow>
#include <kinematics_reachability/kinematics_reachability.h>
#include <ros/ros.h>
//#include "mainwindow.h"
//#include "ui_mainwindow.h"
#include <tf/transform_datatypes.h>
#include <iostream>
#include "angles/angles.h"
#include "kinematics_thread.h"

namespace kinematics_thread
{

KinematicsThread::KinematicsThread()
{
}

void KinematicsThread::initialise()
{
  ros::NodeHandle node_handle("~");
  std::string group_name, frame_id;
  node_handle.param<std::string>("group", group_name, std::string());
  node_handle.param<std::string>("frame_id", frame_id, std::string());

  ROS_INFO("Group name: %s",group_name.c_str());
  ROS_INFO("Frame id: %s",frame_id.c_str());
  
  QString q_name = QString::fromUtf8(group_name.data(), group_name.size());
  QString q_frame_id = QString::fromUtf8(frame_id.data(), frame_id.size());

  Q_EMIT setFrameIdLabel(q_frame_id);
  Q_EMIT setNameLabel(q_name);
  
  if(!reachability_solver_.initialize())
    ROS_ERROR("Could not initialize reachability solver");
  workspace_.group_name = group_name;
  workspace_.header.frame_id = frame_id;    

  workspace_subscriber_ = node_handle.subscribe("workspace_recorded", 1, &KinematicsThread::bagCallback, this);
  progress_subscriber_ = node_handle.subscribe("planner_progress", 1, &KinematicsThread::updateProgressBar, this);

  Q_EMIT sendWorkspace(workspace_);

  ros::spinOnce();
}

KinematicsThread::~KinematicsThread()
{
}

void KinematicsThread::addOrientation(QString roll, QString pitch, QString yaw)
{
  geometry_msgs::Quaternion quaternion;
  quaternion = tf::createQuaternionMsgFromRollPitchYaw(angles::from_degrees(roll.toDouble()),angles::from_degrees(pitch.toDouble()),angles::from_degrees(yaw.toDouble()));
  workspace_.orientations.push_back(quaternion);
  Q_EMIT sendWorkspace(workspace_);
}

void KinematicsThread::computeKinematics(const kinematics_reachability::WorkspacePoints& workspace)
{
  workspace_.points.clear();  
  workspace_ = workspace;
  while(!reachability_solver_.isActive())
  {
    sleep(1.0);
    ROS_INFO("Waiting for planning scene to be set");
  }          
  reachability_solver_.computeWorkspace(workspace_, true);
  reachability_solver_.visualize(workspace_,"solutions");
  reachability_solver_.animateWorkspace(workspace_);
  reachability_solver_.publishWorkspace(workspace_);
  ROS_INFO("Success");  
  Q_EMIT doneComputing();
}

void KinematicsThread::visualise(const kinematics_reachability::WorkspacePoints& workspace)
{
  workspace_ = workspace;
  reachability_solver_.visualizeWorkspaceSamples(workspace_);
  ROS_INFO("Samples visualised.");
}

void KinematicsThread::setBoundaries(kinematics_reachability::WorkspacePoints &workspace, double min_x, double min_y, double min_z, double max_x, double max_y, double max_z, double resolution, double offset_roll, double offset_pitch, double offset_yaw, double offset_x, double offset_y, double offset_z)
{
  /*double origin_x = ui_->edit_text_origin_x->text().toDouble();
  double origin_y = ui_->edit_text_origin_y->text().toDouble();
  double origin_z = ui_->edit_text_origin_z->text().toDouble();

  double min_corner_x = origin_x - (ui_->edit_text_size_x->text().toDouble()/2.0);
  double min_corner_y = origin_y - (ui_->edit_text_size_y->text().toDouble()/2.0);
  double min_corner_z = origin_z - (ui_->edit_text_size_z->text().toDouble()/2.0);

  double max_corner_x = origin_x + (ui_->edit_text_size_x->text().toDouble()/2.0);
  double max_corner_y = origin_y + (ui_->edit_text_size_y->text().toDouble()/2.0);
  double max_corner_z = origin_z + (ui_->edit_text_size_z->text().toDouble()/2.0);
  */

  
  

  workspace.parameters.min_corner.x = min_x;
  workspace.parameters.min_corner.y = min_y;
  workspace.parameters.min_corner.z = min_z;

  workspace.parameters.max_corner.x = max_x;
  workspace.parameters.max_corner.y = max_y;
  workspace.parameters.max_corner.z = max_z;

  //double resolution = ui_->edit_text_resolution->text().toDouble();
  workspace.position_resolution = resolution;

  geometry_msgs::Pose tool_frame_offset;
  //double offset_roll_rad = 0;
  //double offset_pitch_rad = 0;
  //double offset_yaw_rad = 0;

  /*double offset_x = 0; 
  double offset_y = 0;
  double offset_z = 0;
  */
  //bool checked = ui_->tool_offset_enabled->isChecked();
  //if (checked)
  //{
  double offset_roll_rad = angles::from_degrees(offset_roll);
  double offset_pitch_rad = angles::from_degrees(offset_pitch);
  double offset_yaw_rad = angles::from_degrees(offset_yaw);
  //double offset_x = ui_->edit_text_offset_x->text().toDouble(); 
  //double offset_y = ui_->edit_text_offset_y->text().toDouble();
  //double offset_z = ui_->edit_text_offset_z->text().toDouble();
  //}
  tool_frame_offset.orientation = tf::createQuaternionMsgFromRollPitchYaw(offset_roll_rad,offset_pitch_rad,offset_yaw_rad);
  tool_frame_offset.position.x = offset_x;
  tool_frame_offset.position.y = offset_y;
  tool_frame_offset.position.z = offset_z;
  workspace.tool_frame_offset = tool_frame_offset;    
}

void KinematicsThread::setUI(const kinematics_reachability::WorkspacePoints &workspace)
{
  Q_EMIT setUISignal(workspace);
}

void KinematicsThread::bagCallback(const kinematics_reachability::WorkspacePointsConstPtr &msg)
{
  workspace_ = *msg;
  setUI(workspace_); 
  ROS_INFO("Orientations: %d",(int) workspace_.orientations.size());
  ROS_INFO("Points: %d",(int) workspace_.points.size());
  
  reachability_solver_.visualizeWorkspaceSamples(workspace_);
  reachability_solver_.visualize(workspace_,"bag");
  reachability_solver_.animateWorkspace(workspace_);
  ROS_INFO("Samples visualised.");
}

void KinematicsThread::updateProgressBar(const kinematics_reachability::ProgressConstPtr &msg)
{

  ROS_INFO("Progress received.");

  Q_EMIT maxProgressSignal(msg->total);
  Q_EMIT currentProgressSignal(msg->current);

}

void KinematicsThread::computeFK(const kinematics_reachability::WorkspacePoints& workspace, double timeout)
{
  workspace_.points.clear();
  workspace_.orientations.clear();


  while(!reachability_solver_.isActive())
  {
    sleep(1.0);
    ROS_INFO("Waiting for planning scene to be set");
  }
  reachability_solver_.computeWorkspaceFK(workspace_, timeout);
  ROS_INFO("Workspace has %d points",(int) workspace_.points.size());
  
  reachability_solver_.visualize(workspace_,"solutions");
  reachability_solver_.animateWorkspace(workspace_);
  reachability_solver_.publishWorkspace(workspace_);
  ROS_INFO("Success");


}

void KinematicsThread::stopSolver()
{
  reachability_solver_.cancelFindIKSolutions(true);
}

void KinematicsThread::enableSolver()
{
  reachability_solver_.cancelFindIKSolutions(false);  
}

} //namespace


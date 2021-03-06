/*********************************************************************
* Software License Agreement (BSD License)
*
*  Copyright (c) 2011, Willow Garage, Inc.
*  All rights reserved.
*
*  Redistribution and use in source and binary forms, with or without
*  modification, are permitted provided that the following conditions
*  are met:
*
*   * Redistributions of source code must retain the above copyright
*     notice, this list of conditions and the following disclaimer.
*   * Redistributions in binary form must reproduce the above
*     copyright notice, this list of conditions and the following
*     disclaimer in the documentation and/or other materials provided
*     with the distribution.
*   * Neither the name of the Willow Garage nor the names of its
*     contributors may be used to endorse or promote products derived
*     from this software without specific prior written permission.
*
*  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
*  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
*  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
*  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
*  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
*  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
*  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
*  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
*  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
*  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
*  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
*  POSSIBILITY OF SUCH DAMAGE.
*********************************************************************/

/* Author: Ioan Sucan */

#include "planning_scene_monitor/current_state_monitor.h"
#include <tf_conversions/tf_eigen.h>
#include <limits>

planning_scene_monitor::CurrentStateMonitor::CurrentStateMonitor(const planning_models::KinematicModelConstPtr &kmodel, const boost::shared_ptr<tf::Transformer> &tf) :
  tf_(tf), kmodel_(kmodel), kstate_(kmodel), root_(kstate_.getJointState(kmodel->getRoot()->getName())), state_monitor_started_(false), error_(std::numeric_limits<double>::epsilon())
{
}

planning_models::KinematicStatePtr planning_scene_monitor::CurrentStateMonitor::getCurrentState(void) const
{
  boost::mutex::scoped_lock slock(state_update_lock_);
  planning_models::KinematicState *result = new planning_models::KinematicState(kstate_);
  return planning_models::KinematicStatePtr(result);
}

ros::Time planning_scene_monitor::CurrentStateMonitor::getCurrentStateTime(void) const
{
  boost::mutex::scoped_lock slock(state_update_lock_);
  return current_state_time_;
}

std::pair<planning_models::KinematicStatePtr, ros::Time> planning_scene_monitor::CurrentStateMonitor::getCurrentStateAndTime(void) const
{  
  boost::mutex::scoped_lock slock(state_update_lock_);
  planning_models::KinematicState *result = new planning_models::KinematicState(kstate_);
  return std::make_pair(planning_models::KinematicStatePtr(result), current_state_time_);
}

std::map<std::string, double> planning_scene_monitor::CurrentStateMonitor::getCurrentStateValues(void) const
{
  std::map<std::string, double> m;
  boost::mutex::scoped_lock slock(state_update_lock_);
  kstate_.getStateValues(m);
  return m;
}

void planning_scene_monitor::CurrentStateMonitor::setOnStateUpdateCallback(const JointStateUpdateCallback &callback)
{
  on_state_update_callback_ = callback;
}

void planning_scene_monitor::CurrentStateMonitor::startStateMonitor(const std::string &joint_states_topic)
{
  if (!state_monitor_started_ && kmodel_)
  {
    joint_time_.clear();
    if (joint_states_topic.empty())
      ROS_ERROR("The joint states topic cannot be an empty string");
    else
      joint_state_subscriber_ = nh_.subscribe(joint_states_topic, 25, &CurrentStateMonitor::jointStateCallback, this);
    state_monitor_started_ = true;
    ROS_DEBUG("Listening to joint states on topic '%s'", joint_states_topic.c_str());
  }
}

bool planning_scene_monitor::CurrentStateMonitor::isActive(void) const
{
  return state_monitor_started_;
}

void planning_scene_monitor::CurrentStateMonitor::stopStateMonitor(void)
{
  if (state_monitor_started_)
  {
    joint_state_subscriber_.shutdown();
    ROS_DEBUG("No longer listening o joint states");
    state_monitor_started_ = false;
  }
}

std::string planning_scene_monitor::CurrentStateMonitor::getMonitoredTopic(void) const
{
  if (joint_state_subscriber_)
    return joint_state_subscriber_.getTopic();
  else
    return "";
}

bool planning_scene_monitor::CurrentStateMonitor::haveCompleteState(void) const
{
  bool result = true;
  const std::vector<std::string> &dof = kmodel_->getActiveDOFNames();
  boost::mutex::scoped_lock slock(state_update_lock_);
  for (std::size_t i = 0 ; i < dof.size() ; ++i)
    if (joint_time_.find(dof[i]) == joint_time_.end())
    {
      ROS_DEBUG("Joint variable '%s' has never been updated", dof[i].c_str());
      result = false;
    }
  return result;
}

bool planning_scene_monitor::CurrentStateMonitor::haveCompleteState(std::vector<std::string> &missing_states) const
{
  bool result = true;
  const std::vector<std::string> &dof = kmodel_->getActiveDOFNames();
  boost::mutex::scoped_lock slock(state_update_lock_);
  for (std::size_t i = 0 ; i < dof.size() ; ++i)
    if (joint_time_.find(dof[i]) == joint_time_.end())
    {
      ROS_DEBUG("Joint variable '%s' has never been updated", dof[i].c_str());
      missing_states.push_back(dof[i]);
      result = false;
    }
  return result;
}

bool planning_scene_monitor::CurrentStateMonitor::haveCompleteState(const ros::Duration &age) const
{
  bool result = true;
  const std::vector<std::string> &dof = kmodel_->getActiveDOFNames();
  ros::Time now = ros::Time::now();
  ros::Time old = now - age;
  boost::mutex::scoped_lock slock(state_update_lock_);
  for (std::size_t i = 0 ; i < dof.size() ; ++i)
  {
    std::map<std::string, ros::Time>::const_iterator it = joint_time_.find(dof[i]);
    if (it == joint_time_.end())
    {
      ROS_DEBUG("Joint variable '%s' has never been updated", dof[i].c_str());
      result = false;
    }
    else
      if (it->second < old)
      {
        ROS_DEBUG("Joint variable '%s' was last updated %0.3lf seconds ago (older than the allowed %0.3lf seconds)",
                  dof[i].c_str(), (now - it->second).toSec(), age.toSec());
        result = false;
      }
  }
  return result;
}

bool planning_scene_monitor::CurrentStateMonitor::haveCompleteState(const ros::Duration &age,
                                                                    std::vector<std::string> &missing_states) const
{
  bool result = true;
  const std::vector<std::string> &dof = kmodel_->getActiveDOFNames();
  ros::Time now = ros::Time::now();
  ros::Time old = now - age;
  boost::mutex::scoped_lock slock(state_update_lock_);
  for (std::size_t i = 0 ; i < dof.size() ; ++i)
  {
    std::map<std::string, ros::Time>::const_iterator it = joint_time_.find(dof[i]);
    if (it == joint_time_.end())
    {
      ROS_DEBUG("Joint variable '%s' has never been updated", dof[i].c_str());
      missing_states.push_back(dof[i]);
      result = false;
    }
    else
      if (it->second < old)
      {
        ROS_DEBUG("Joint variable '%s' was last updated %0.3lf seconds ago (older than the allowed %0.3lf seconds)",
                  dof[i].c_str(), (now - it->second).toSec(), age.toSec());
        missing_states.push_back(dof[i]);
        result = false;
      }
  }
  return result;
}

void planning_scene_monitor::CurrentStateMonitor::jointStateCallback(const sensor_msgs::JointStateConstPtr &joint_state)
{
  if (joint_state->name.size() != joint_state->position.size())
  {
    ROS_ERROR("State monitor received invalid joint state");
    return;
  }
  
  // read the received values, and update their time stamps
  std::size_t n = joint_state->name.size();
  std::map<std::string, double> joint_state_map;
  const std::map<std::string, std::pair<double, double> > &bounds = kmodel_->getAllVariableBounds();
  for (std::size_t i = 0 ; i < n ; ++i)
  {    
    joint_state_map[joint_state->name[i]] = joint_state->position[i];
    joint_time_[joint_state->name[i]] = joint_state->header.stamp;
    
    // continuous joints wrap, so we don't modify them (even if they are outside bounds!)
    const planning_models::KinematicModel::JointModel* jm = kmodel_->getJointModel(joint_state->name[i]);
    if (jm && jm->getType() == planning_models::KinematicModel::JointModel::REVOLUTE)
      if (static_cast<const planning_models::KinematicModel::RevoluteJointModel*>(jm)->isContinuous())
        continue;
    
    std::map<std::string, std::pair<double, double> >::const_iterator bi = bounds.find(joint_state->name[i]);
    // if the read variable is 'almost' within bounds (up to error_ difference), then consider it to be within bounds
    if (bi != bounds.end())
    {
      if (joint_state->position[i] < bi->second.first && joint_state->position[i] >= bi->second.first - error_)
        joint_state_map[joint_state->name[i]] = bi->second.first;
      else
        if (joint_state->position[i] > bi->second.second && joint_state->position[i] <= bi->second.second + error_)
          joint_state_map[joint_state->name[i]] = bi->second.second;
    }
  }
  bool set_map_values = true;
  
  // read root transform, if needed
  if (tf_ && (root_->getType() == planning_models::KinematicModel::JointModel::PLANAR ||
              root_->getType() == planning_models::KinematicModel::JointModel::FLOATING))
  {
    const std::string &child_frame = root_->getJointModel()->getChildLinkModel()->getName();
    const std::string &parent_frame = kmodel_->getModelFrame();
    
    std::string err;
    ros::Time tm;
    tf::StampedTransform transf;
    bool ok = false;
    if (tf_->getLatestCommonTime(parent_frame, child_frame, tm, &err) == tf::NO_ERROR)
    {
      try
      {
        tf_->lookupTransform(parent_frame, child_frame, tm, transf);
        ok = true;
      }
      catch(tf::TransformException& ex)
      {
        ROS_ERROR("Unable to lookup transform from %s to %s.  Exception: %s", parent_frame.c_str(), child_frame.c_str(), ex.what());
      }
    }
    else
      ROS_DEBUG("Unable to lookup transform from %s to %s: no common time.", parent_frame.c_str(), child_frame.c_str());
    if (ok)
    {
      const std::vector<std::string> &vars = root_->getJointModel()->getVariableNames();
      for (std::size_t j = 0; j < vars.size() ; ++j)
        joint_time_[vars[j]] = tm;
      set_map_values = false;
      Eigen::Affine3d eigen_transf;
      tf::TransformTFToEigen(transf, eigen_transf);
      boost::mutex::scoped_lock slock(state_update_lock_);
      root_->setVariableValues(eigen_transf);
      kstate_.setStateValues(joint_state_map); 
      current_state_time_ = joint_state->header.stamp;
    }
  }
  
  if (set_map_values)
  {
    boost::mutex::scoped_lock slock(state_update_lock_);
    kstate_.setStateValues(joint_state_map);
    current_state_time_ = joint_state->header.stamp;
  }
  
  // callback, if needed
  if (on_state_update_callback_)
    on_state_update_callback_(joint_state);
}

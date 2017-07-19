#include "ros/ros.h"
#include "nav_msgs/Path.h"
#include <mav_trajectory_generation/polynomial_optimization_linear.h>
#include "mav_trajectory_generation_ros/minSnapStamped.h"
#include <mav_trajectory_generation_ros/ros_visualization.h>
#include "HelperFunctions/helper.h"
#include "HelperFunctions/QuatRotEuler.h"
#include "minSnapFunctions/minSnapFcns.h"
#include "mav_trajectory_generation_ros/PVAJS.h"
#include "mav_trajectory_generation_ros/PVAJS_array.h"
#include <mav_trajectory_generation/polynomial_optimization_nonlinear.h>


#include <sstream>

bool minSnapNloptService(mav_trajectory_generation_ros::minSnapStamped::Request  &req,
                           mav_trajectory_generation_ros::minSnapStamped::Response &res)
{
  //Get initial time to calculate solving time
  ros::Time t0 = ros::Time::now();

  //Declare initial variables
  const int n_w = req.Waypoints.poses.size(); //Number of waypoints
  const int dimension = 3;
  const int derivative_to_optimize = mav_trajectory_generation::derivative_order::SNAP;
  const double dt = 0.1; //Output sampling period
  double cost = 0;

  //Get waypoints into vertex
  mav_trajectory_generation::Vertex::Vector vertices;
  waypoint2vertex_minSnap(req.Waypoints, dimension,
                          derivative_to_optimize, &vertices);


  //Get segment times from client request
  Eigen::VectorXd segment_times = Eigen::MatrixXd::Zero(n_w-1,1);
  double diff_time;
  for (int i = 1; i < n_w; i++){
    diff_time = (req.Waypoints.poses[i].header.stamp - req.Waypoints.poses[i-1].header.stamp).toSec();
    segment_times(i-1) = diff_time;
  }
  std::cout << "final time: " << segment_times.sum() << std::endl;

  //Convert segment times to the appropriate type
  std::vector<double> stdSegment_times;
  eigenVectorXd2stdVector(segment_times, &stdSegment_times);

  //Get solution for minimum snap problem with optimal segment times
  mav_trajectory_generation::Trajectory trajectory;
  mav_trajectory_generation::NonlinearOptimizationParameters parameters;
  parameters.max_iterations = 1000;
  parameters.f_rel = 0.05;
  // parameters.x_rel = 0.1;
  parameters.time_penalty = 500.0;
  parameters.initial_stepsize_rel = 0.1;
  parameters.inequality_constraint_tolerance = 0.1;
  const int N = 10;
  const double v_max = 2.0;
  const double a_max = 2.0;
  mav_trajectory_generation::PolynomialOptimizationNonLinear<N> opt(dimension, parameters, true);
  opt.setupFromVertices(vertices, stdSegment_times, derivative_to_optimize);
  // opt.addMaximumMagnitudeConstraint(mav_trajectory_generation::derivative_order::VELOCITY, v_max);                                
  // opt.addMaximumMagnitudeConstraint(mav_trajectory_generation::derivative_order::ACCELERATION, a_max);
  opt.optimize();

  mav_trajectory_generation::OptimizationInfo optimization_info_ = opt.getOptimizationInfo();
  std::cout << optimization_info_.cost_trajectory << std::endl;
  std::cout << optimization_info_.cost_time << std::endl;
  

  mav_trajectory_generation::Segment::Vector segments;
  opt.getPolynomialOptimizationRef().getSegments(&segments);
  opt.getTrajectory(&trajectory);
  std::vector<double> stdSegment_timesFinal;
  opt.getPolynomialOptimizationRef().getSegmentTimes(&stdSegment_timesFinal);

  std::cout << "final time: " << trajectory.getMaxTime() << std::endl;

  //Calculate solution time
  ros::Duration SolverTime = ros::Time::now() - t0;
  ROS_INFO("Number of points: %d\tCalculation time: %f\tCost: %f",
            n_w, SolverTime.toSec(), cost);


  //Sample trajectory
  mav_trajectory_generation_ros::PVAJS_array flatStates;
  trajectory2waypoint(trajectory, dt, &flatStates);

  //Output
  res.flatStates = flatStates;
  
  return true;
}

bool minSnapOptTimeService(mav_trajectory_generation_ros::minSnapStamped::Request  &req,
                           mav_trajectory_generation_ros::minSnapStamped::Response &res)
{
  //Get initial time to calculate solving time
  ros::Time t0 = ros::Time::now();

  // //Declare initial variables
  const int n_w = req.Waypoints.poses.size(); //Number of waypoints
  const int dimension = 3;
  const int derivative_to_optimize = mav_trajectory_generation::derivative_order::SNAP;
  const double dt = 0.01; //Output sampling period
  double cost;

  //Get waypoints into vertex
  mav_trajectory_generation::Vertex::Vector vertices;
  waypoint2vertex_minSnap(req.Waypoints, dimension,
                          derivative_to_optimize, &vertices);


  //Get segment times from client request
  Eigen::VectorXd segment_times = Eigen::MatrixXd::Zero(n_w-1,1);
  double diff_time;
  for (int i = 1; i < n_w; i++){
    diff_time = (req.Waypoints.poses[i].header.stamp - req.Waypoints.poses[i-1].header.stamp).toSec();
    segment_times(i-1) = diff_time;
  }

  //Get solution for minimum snap problem with optimal segment times
  mav_trajectory_generation::Trajectory trajectory;
  cost = solveMinSnapGradDescent(vertices, dimension, segment_times, &trajectory);

  //Calculate solution time
  ros::Duration SolverTime = ros::Time::now() - t0;
  ROS_INFO("Number of points: %d\tCalculation time: %f\tCost: %f",
            n_w, SolverTime.toSec(), cost);


  //Sample trajectory
  mav_trajectory_generation_ros::PVAJS_array flatStates;
  trajectory2waypoint(trajectory, dt, &flatStates);

  //Output
  res.flatStates = flatStates;


  return true;
}

bool minSnapService(mav_trajectory_generation_ros::minSnapStamped::Request  &req,
                    mav_trajectory_generation_ros::minSnapStamped::Response &res)
{

  //Get initial time to calculate solving time
  ros::Time t0 = ros::Time::now();
  ros::Duration SolverTime;

  // //Declare initial variables
  const int n_w = req.Waypoints.poses.size(); //Number of waypoints
  const int dimension = 3;
  const int derivative_to_optimize = mav_trajectory_generation::derivative_order::SNAP;
  const double dt = 0.01; //Output sampling period
  double cost;

  //Get waypoints into vertex
  mav_trajectory_generation::Vertex::Vector vertices;
  waypoint2vertex_minSnap(req.Waypoints, dimension,
                          derivative_to_optimize, &vertices);


  //Get segment times from client request
  Eigen::VectorXd segment_times = Eigen::MatrixXd::Zero(n_w-1,1);
  double diff_time;
  for (int i = 1; i < n_w; i++){
    diff_time = (req.Waypoints.poses[i].header.stamp - req.Waypoints.poses[i-1].header.stamp).toSec();
    segment_times(i-1) = diff_time;
  }

  //Calculate solution time
  SolverTime = ros::Time::now() - t0;
  ROS_INFO("Number of points: %d\tTime to get time_vector: %f",
            n_w, SolverTime.toSec());

  //Get solution for minimum snap problem
  mav_trajectory_generation::Trajectory trajectory;
  // cost = solveMinSnap(vertices, segment_times, dimension, &trajectory);

  //Convert segment times to the appropriate type
  std::vector<double> stdSegment_times;
  eigenVectorXd2stdVector(segment_times, &stdSegment_times);

//Calculate solution time
  SolverTime = ros::Time::now() - t0;
  ROS_INFO("Number of points: %d\tTime to get segment times: %f",
            n_w, SolverTime.toSec());

  //Solve optimization problem
  const int N = 10;
  mav_trajectory_generation::PolynomialOptimization<N> opt(dimension);
  opt.setupFromVertices(vertices, stdSegment_times, derivative_to_optimize);
 //Calculate solution time
  SolverTime = ros::Time::now() - t0;
  ROS_INFO("Number of points: %d\tTime to get setup: %f",
            n_w, SolverTime.toSec());

  opt.solveLinear();
//Calculate solution time
  SolverTime = ros::Time::now() - t0;
  ROS_INFO("Number of points: %d\tTime to solve optimization: %f",
            n_w, SolverTime.toSec());
  //Get segments
  mav_trajectory_generation::Segment::Vector segments;
  opt.getSegments(&segments);

  //Get trajectory
  // mav_trajectory_generation::Trajectory trajectory;
  opt.getTrajectory(&trajectory);

  cost = opt.computeCost();

  //Calculate solution time
  SolverTime = ros::Time::now() - t0;
  ROS_INFO("Number of points: %d\tCalculation time: %f\tCost: %f",
            n_w, SolverTime.toSec(), cost);

  //Sample trajectory
  mav_trajectory_generation_ros::PVAJS_array flatStates;
  trajectory2waypoint(trajectory, dt, &flatStates);

  //Output
  res.flatStates = flatStates;

  return true;
}

/**
 * This tutorial demonstrates simple sending of messages over the ROS system.
 */
int main(int argc, char **argv)
{
  /**
   * The ros::init() function needs to see argc and argv so that it can perform
   * any ROS arguments and name remapping that were provided at the command line.
   * For programmatic remappings you can use a different version of init() which takes
   * remappings directly, but for most command-line programs, passing argc and argv is
   * the easiest way to do it.  The third argument to init() is the name of the node.
   *
   * You must call one of the versions of ros::init() before using any other
   * part of the ROS system.
   */
  ros::init(argc, argv, "talker");

  //ROS initialization
  ros::NodeHandle n;

  //Publishers
  ros::Publisher Waypoint_pub = n.advertise<nav_msgs::Path>("Waypoints", 1000);

  //Services
  ros::ServiceServer minSnap_Srv = n.advertiseService("/minSnap", minSnapService);
  ros::ServiceServer minSnap_Srv2 = n.advertiseService("/minSnapOptTime", minSnapOptTimeService);
  ros::ServiceServer minSnap_Srv3 = n.advertiseService("/minSnapNlopt", minSnapNloptService);

  ros::Rate loop_rate(50);

  /**
   * A count of how many messages we have sent. This is used to create
   * a unique string for each message.
   */
  while (ros::ok())
  {

    // Waypoint_pub.publish(Waypoints);

    ros::spinOnce();

    loop_rate.sleep();
  }


  return 0;
}
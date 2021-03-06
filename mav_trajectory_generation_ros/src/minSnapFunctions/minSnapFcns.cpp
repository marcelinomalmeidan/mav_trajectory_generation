
#include "minSnapFcns.h"

void waypoint2vertex_minSnap(const nav_msgs::Path &Waypoints,
	const int &dimension,
	mav_trajectory_generation::Vertex::Vector *vertex){

  //Declare initial variables
  int n_w = Waypoints.poses.size();
  mav_trajectory_generation::Vertex start_end(dimension), middle(dimension);
  const int derivative_to_optimize = mav_trajectory_generation::derivative_order::SNAP;

  //Populate waypoints
  Eigen::Vector3d Point;
  for(int i = 0; i < n_w; i++){

    Point << Waypoints.poses[i].pose.position.x,
             Waypoints.poses[i].pose.position.y,
             Waypoints.poses[i].pose.position.z;

    if ((i == 0) || (i == n_w -1)){
      start_end.makeStartOrEnd(Point, derivative_to_optimize);
      vertex->push_back(start_end);
    }
    else{
      middle.addConstraint(mav_trajectory_generation::derivative_order::POSITION, Point);
      vertex->push_back(middle);
    }

  }
}

void yaw2vertex_minAcc(const nav_msgs::Path &Waypoints,
  mav_trajectory_generation::Vertex::Vector *vertex){

  //Declare initial variables
  int n_w = Waypoints.poses.size();
  const int dimension = 1;
  mav_trajectory_generation::Vertex start_end(dimension), middle(dimension);
  const int derivative_to_optimize = mav_trajectory_generation::derivative_order::ACCELERATION;

  //Populate yaw waypoints
  double yaw, prev_yaw;
  for(int i = 0; i < n_w; i++){

    yaw = getHeadingFromQuat(Waypoints.poses[i].pose.orientation);

    if (i > 0) {
      if (prev_yaw - yaw > M_PI) {
        yaw = yaw - 2*M_PI;
      } else if( prev_yaw - yaw < -M_PI) {
        yaw = yaw + 2*M_PI;
      }
    }

    if ((i == 0) || (i == n_w -1)){
      start_end.makeStartOrEnd(yaw, derivative_to_optimize);
      vertex->push_back(start_end);
    }
    else{
      middle.addConstraint(mav_trajectory_generation::derivative_order::POSITION, yaw);
      vertex->push_back(middle);
    }

    prev_yaw = yaw;
  }
}

void trajectory2waypoint(
  const mav_trajectory_generation::Trajectory &wp_trajectory,
  const double &dt,
  mav_trajectory_generation_ros::PVAJS_array *flatStates){

  //Get whole trajectory
  mav_msgs::EigenTrajectoryPoint::Vector states;
  bool success = mav_trajectory_generation::sampleWholeTrajectory(wp_trajectory, dt, &states);
  if (!success) {
    ROS_INFO("Error while sampling trajectory!");
    return;
  }

  //Populate waypoints to send back
  geometry_msgs::PoseStamped Pos;
  mav_trajectory_generation_ros::PVAJS flatState;

  mav_trajectory_generation::EigenTrajectoryPoint2PVAJS_array(states, flatStates);
}

void trajectory2waypoint(
	const mav_trajectory_generation::Trajectory &wp_trajectory,
  const mav_trajectory_generation::Trajectory &yaw_trajectory,
	const double &dt,
  mav_trajectory_generation_ros::PVAJS_array *flatStates){

  //Get whole trajectory
  mav_msgs::EigenTrajectoryPoint::Vector states;
  bool success = mav_trajectory_generation::sampleWholeTrajectory(wp_trajectory, dt, &states);
  if (!success) {
    ROS_INFO("Error while sampling trajectory!");
    return;
  }

  //Populate waypoints to send back
  geometry_msgs::PoseStamped Pos;
  mav_trajectory_generation_ros::PVAJS flatState;

  mav_trajectory_generation::EigenTrajectoryPoint2PVAJS_array(states, flatStates);

  // Get yaw trajectory
  yawTrajectory2waypoint(yaw_trajectory, dt, flatStates);
}

void yawTrajectory2waypoint(
  const mav_trajectory_generation::Trajectory &yaw_trajectory,
  const double &dt,
  mav_trajectory_generation_ros::PVAJS_array *flatStates) {
  int yaw_pos = mav_trajectory_generation::derivative_order::POSITION;
  int yaw_vel = mav_trajectory_generation::derivative_order::VELOCITY;
  int yaw_acc = mav_trajectory_generation::derivative_order::ACCELERATION;
  double sample_time;
  std::vector<Eigen::VectorXd> yaw_vec, yawdot_vec, yawddot_vec;

  const double min_time = yaw_trajectory.getMinTime();
  const double max_time = yaw_trajectory.getMaxTime();

  yaw_trajectory.evaluateRange(min_time, max_time, dt, yaw_pos, 
                               &yaw_vec);
  yaw_trajectory.evaluateRange(min_time, max_time, dt, yaw_vel, 
                               &yawdot_vec);
  yaw_trajectory.evaluateRange(min_time, max_time, dt, yaw_acc, 
                               &yawddot_vec);

  for (int i = 0; i < yaw_vec.size(); i++){
    flatStates->PVAJS_array[i].yaw = yaw_vec[i][0];
    flatStates->PVAJS_array[i].yaw_dot = yawdot_vec[i][0];
    flatStates->PVAJS_array[i].yaw_ddot = yawddot_vec[i][0];
   }
}


double solveMinSnap(
	const mav_trajectory_generation::Vertex::Vector &vertices,
	const Eigen::VectorXd &segment_times,
	const int &dimension, 
	mav_trajectory_generation::Trajectory *trajectory){

  //Convert segment times to the appropriate type
  std::vector<double> stdSegment_times;
  eigenVectorXd2stdVector(segment_times, &stdSegment_times);

  //Solve optimization problem
  const int N = 10;
  const int derivative_to_optimize = mav_trajectory_generation::derivative_order::SNAP;
  mav_trajectory_generation::PolynomialOptimization<N> opt(dimension);
  opt.setupFromVertices(vertices, stdSegment_times, derivative_to_optimize);
  opt.solveLinear();

  //Get segments
  mav_trajectory_generation::Segment::Vector segments;
  opt.getSegments(&segments);

  //Get trajectory
  // mav_trajectory_generation::Trajectory trajectory;
  opt.getTrajectory(trajectory);

  return opt.computeCost();
}

double solveMinAcceleration(
  const mav_trajectory_generation::Vertex::Vector &vertices,
  const Eigen::VectorXd &segment_times,
  const int &dimension, 
  mav_trajectory_generation::Trajectory *trajectory){

  //Convert segment times to the appropriate type
  std::vector<double> stdSegment_times;
  eigenVectorXd2stdVector(segment_times, &stdSegment_times);

  return solveMinAcceleration(vertices, stdSegment_times, dimension, trajectory);
}

double solveMinAcceleration(
  const mav_trajectory_generation::Vertex::Vector &vertices,
  const std::vector<double> &segment_times,
  const int &dimension, 
  mav_trajectory_generation::Trajectory *trajectory){

  //Solve optimization problem
  const int N = 10;
  const int derivative_to_optimize = mav_trajectory_generation::derivative_order::ACCELERATION;
  mav_trajectory_generation::PolynomialOptimization<N> opt(dimension);
  opt.setupFromVertices(vertices, segment_times, derivative_to_optimize);
  opt.solveLinear();

  //Get segments
  mav_trajectory_generation::Segment::Vector segments;
  opt.getSegments(&segments);

  //Get trajectory
  // mav_trajectory_generation::Trajectory trajectory;
  opt.getTrajectory(trajectory);

  return opt.computeCost();
}

//Compute minimum snap trajectory with optimal segments and return minSnap cost
double solveMinSnapGradDescent(
  const mav_trajectory_generation::Vertex::Vector &vertices,
  const int &dimension, 
  Eigen::VectorXd &segment_times,
  mav_trajectory_generation::Trajectory *trajectory,
  Eigen::VectorXd *segment_times_out){

  //Get initial solution for minimum snap problem
  double cost;
  mav_trajectory_generation::Trajectory curTrajectory;
  cost = solveMinSnap(vertices, segment_times, dimension, &curTrajectory);

  //Declare variables for gradient descent
  const double FinalTime = curTrajectory.getMaxTime();
  const int m = segment_times.size();
  const double h = 0.00001;     //Small step for time gradient
  const double epsilon = 0.01;  //Condition for terminating gradient descent
  double step = std::numeric_limits<float>::infinity();
  double costNew;
  Eigen::VectorXd g = (-1.0/(m-1.0))*Eigen::MatrixXd::Ones(m,1);
  Eigen::VectorXd gradF = Eigen::MatrixXd::Zero(m,1);
  double init_cost = cost;
  Eigen::VectorXd best_segment_times;
  *trajectory = curTrajectory;

  //Gradient descent loop
  while(step > epsilon){

    //Calculate gradient
    Eigen::VectorXd g_i = g;
    Eigen::VectorXd segment_times_new = segment_times;
    for(int i = 0; i < m; i++){
      g_i = g;
      g_i[i] = 1;
      segment_times_new = segment_times + h*g_i;
      costNew = solveMinSnap(vertices, segment_times_new, dimension, &curTrajectory);
      gradF(i) = (costNew - cost)/h;
    }

    //Normalize gradient vector (its bigger value will be as big as the bigger segment time)
    gradF = segment_times_new.cwiseAbs().minCoeff()*gradF/gradF.cwiseAbs().maxCoeff();

    //Perform gradient descent
    double alpha = 0.9;           //Step size
    double curCost = std::numeric_limits<float>::infinity();
    double prevCost = std::numeric_limits<float>::infinity();
    best_segment_times = segment_times;
    double bestCost = cost;
    for (int j = 0; j < 6; j++){  //Here we only iterate 6 times before alpha becomes too small
      segment_times_new = segment_times - alpha*gradF;

      //Renormalize segment times to preserve final time
      segment_times_new = FinalTime*segment_times_new/segment_times_new.sum();

      curCost = solveMinSnap(vertices, segment_times_new, dimension, &curTrajectory);

      // if(curCost > prevCost) {
      //     break;
      // }

      if(curCost < bestCost){
        bestCost = curCost;
        best_segment_times = segment_times_new;
      }

      alpha = alpha*0.5;
      prevCost = curCost;
    }

    //Check if there was any improvement. Otherwise, stop iterations
    if(bestCost < cost){
      segment_times = best_segment_times;
      step = (cost - bestCost)/cost;
      cost = bestCost;
      *trajectory = curTrajectory;
    }
    else{
      break;
    }

  }
  ROS_INFO("improvement: %f", (init_cost-cost)/cost);

  // std::cout  << segment_times.transpose() << std::endl;

  *segment_times_out = best_segment_times;
  return cost;
}
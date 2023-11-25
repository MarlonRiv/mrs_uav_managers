#include <ros/ros.h>

#include <mrs_lib/subscribe_handler.h>
#include <mrs_lib/service_client_handler.h>
#include <mrs_lib/attitude_converter.h>
#include <mrs_lib/publisher_handler.h>

#include <mrs_msgs/ControlManagerDiagnostics.h>
#include <mrs_msgs/EstimationDiagnostics.h>
#include <mrs_msgs/Vec4.h>

#include <std_srvs/SetBool.h>
#include <std_srvs/Trigger.h>

#include <nav_msgs/Odometry.h>

#include <gtest/gtest.h>
#include <ros/console.h>
#include <log4cxx/logger.h>

#define POS_JUMP_SIZE 3.0

/* class Tester //{ */

class Tester {

public:
  Tester();

  bool test();

private:
  ros::NodeHandle                    nh_;
  std::shared_ptr<ros::AsyncSpinner> spinner_;

  mrs_lib::SubscribeHandler<mrs_msgs::ControlManagerDiagnostics> sh_control_manager_diag_;
  mrs_lib::SubscribeHandler<mrs_msgs::EstimationDiagnostics>     sh_estim_manager_diag_;

  mrs_lib::ServiceClientHandler<std_srvs::SetBool> sch_arming_;
  mrs_lib::ServiceClientHandler<std_srvs::Trigger> sch_midair_;
  mrs_lib::ServiceClientHandler<std_srvs::Trigger> sch_eland_;
};

//}

/* Tester() //{ */

Tester::Tester() {

  // | ------------------ initialize test node ------------------ |

  nh_ = ros::NodeHandle("~");

  ROS_INFO("[%s]: ROS node initialized", ros::this_node::getName().c_str());

  ros::Time::waitForValid();

  spinner_ = std::make_shared<ros::AsyncSpinner>(2);
  spinner_->start();

  std::string uav_name = "uav1";

  // | ----------------------- subscribers ---------------------- |

  mrs_lib::SubscribeHandlerOptions shopts;
  shopts.nh                 = nh_;
  shopts.node_name          = "Test";
  shopts.no_message_timeout = mrs_lib::no_timeout;
  shopts.threadsafe         = true;
  shopts.autostart          = true;
  shopts.queue_size         = 10;
  shopts.transport_hints    = ros::TransportHints().tcpNoDelay();

  sh_control_manager_diag_ = mrs_lib::SubscribeHandler<mrs_msgs::ControlManagerDiagnostics>(shopts, "/" + uav_name + "/control_manager/diagnostics");
  sh_estim_manager_diag_   = mrs_lib::SubscribeHandler<mrs_msgs::EstimationDiagnostics>(shopts, "/" + uav_name + "/estimation_manager/diagnostics");

  ROS_INFO("[%s]: subscribers initialized", ros::this_node::getName().c_str());

  // | --------------------- service clients -------------------- |

  sch_arming_ = mrs_lib::ServiceClientHandler<std_srvs::SetBool>(nh_, "/" + uav_name + "/hw_api/arming");
  sch_midair_ = mrs_lib::ServiceClientHandler<std_srvs::Trigger>(nh_, "/" + uav_name + "/uav_manager/midair_activation");
  sch_eland_  = mrs_lib::ServiceClientHandler<std_srvs::Trigger>(nh_, "/" + uav_name + "/control_manager/eland");

  ROS_INFO("[%s]: service client initialized", ros::this_node::getName().c_str());
}

//}

/* test() //{ */

bool Tester::test() {

  // | ---------------- wait for ready to takeoff --------------- |

  while (ros::ok()) {

    ROS_INFO_THROTTLE(1.0, "[%s]: waiting for the MRS UAV System", ros::this_node::getName().c_str());

    if (sh_control_manager_diag_.hasMsg() && sh_estim_manager_diag_.hasMsg()) {
      break;
    }

    ros::Duration(0.01).sleep();
  }

  ROS_INFO("[%s]: MRS UAV System is ready", ros::this_node::getName().c_str());

  ros::Duration(1.0).sleep();

  // | ---------------------- arm the drone --------------------- |

  ROS_INFO("[%s]: arming the edrone", ros::this_node::getName().c_str());

  std_srvs::SetBool arming;
  arming.request.data = true;

  {
    bool service_call = sch_arming_.call(arming);

    if (!service_call || !arming.response.success) {
      ROS_ERROR("[%s]: arming service call failed", ros::this_node::getName().c_str());
      return false;
    }
  }

  // | -------------------------- wait -------------------------- |

  ros::Duration(0.2).sleep();

  // | -------------------- midair activation ------------------- |

  ROS_INFO("[%s]: activating the drone 'in mid air'", ros::this_node::getName().c_str());

  std_srvs::Trigger midair;

  {
    bool service_call = sch_midair_.call(midair);

    if (!service_call || !midair.response.success) {
      ROS_ERROR("[%s]: midair activation service call failed", ros::this_node::getName().c_str());
      return false;
    }
  }

  // | ----------------- sleep before the action ---------------- |

  ros::Duration(1.0).sleep();

  // | ----------------- call the eland service ----------------- |

  std_srvs::Trigger eland;

  {
    bool service_call = sch_eland_.call(eland);

    if (!service_call || !midair.response.success) {
      ROS_ERROR("[%s]: eland service call failed", ros::this_node::getName().c_str());
      return false;
    }
  }

  // | -------------- wait for emergency controller ------------- |

  while (ros::ok()) {

    ROS_INFO_THROTTLE(1.0, "[%s]: waiting for EmergencyController", ros::this_node::getName().c_str());

    if (sh_control_manager_diag_.getMsg()->active_controller == "EmergencyController") {
      break;
    }
  }

  // | -------------------- wait for landing -------------------- |

  while (ros::ok()) {

    ROS_INFO_THROTTLE(1.0, "[%s]: waiting for lainding", ros::this_node::getName().c_str());

    if (sh_control_manager_diag_.getMsg()->output_enabled == false) {
      break;
    }
  }

  return true;
}

//}

std::shared_ptr<Tester> tester_;

TEST(TESTSuite, eland_service) {

  bool result = tester_->test();

  if (result) {
    GTEST_SUCCEED();
  } else {
    GTEST_FAIL();
  }
}

int main([[maybe_unused]] int argc, [[maybe_unused]] char** argv) {

  ros::init(argc, argv, "eland_service_test");

  tester_ = std::make_shared<Tester>();

  testing::InitGoogleTest(&argc, argv);

  Tester tester;

  return RUN_ALL_TESTS();
}

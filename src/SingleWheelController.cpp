//
// Created by james on 4/3/19.
//

#include <sys/mman.h>

#include <bobble_controllers/SingleWheelController.h>
#include <pluginlib/class_list_macros.h>

namespace bobble_controllers {

    void SingleWheelController::runSubscriber() {
        ros::NodeHandle n;
        ros::Subscriber sub = n.subscribe("/bobble/wheel_controller/wheel_transition", 1,
                                          &SingleWheelController::transitionCallback, this);
        ros::Subscriber sub_cmd_vel = n.subscribe("/bobble/wheel_controller/wheel_cmd", 1,
                                                  &SingleWheelController::commandCallback, this);
        ros::Rate loop_rate(20);
        while(ros::ok() && runThread)
        {
            control_command_mutex.lock();
            commandStruct.IdleCmd = commandStructTmp.IdleCmd;
            commandStruct.VoltageCmd = commandStructTmp.VoltageCmd;
            commandStruct.VelocityCmd = commandStructTmp.VelocityCmd;
            commandStruct.PositionCmd = commandStructTmp.PositionCmd;
            commandStruct.DesiredVoltage = commandStructTmp.DesiredVoltage;
            commandStruct.DesiredVelocity = commandStructTmp.DesiredVelocity;
            commandStruct.DesiredPosition = commandStructTmp.DesiredPosition;
            control_command_mutex.unlock();

            loop_rate.sleep();
        }
        sub.shutdown();
        sub_cmd_vel.shutdown();
    }

    void SingleWheelController::transitionCallback(const bobble_controllers::WheelControlCommands::ConstPtr &cmd) {
        commandStructTmp.IdleCmd = cmd->IdleCmd;
        commandStructTmp.VoltageCmd = cmd->VoltageCmd;
        commandStructTmp.VelocityCmd = cmd->VelocityCmd;
        commandStructTmp.PositionCmd = cmd->PositionCmd;
    }

    void SingleWheelController::commandCallback(const bobble_controllers::WheelCommands::ConstPtr &cmd) {
        commandStructTmp.DesiredVoltage = cmd->DesiredVoltage;
        commandStructTmp.DesiredVelocity = cmd->DesiredVelocity;
        commandStructTmp.DesiredPosition = cmd->DesiredPosition;
    }

    SingleWheelController::SingleWheelController(void)
            :
            VelocityControlPID(0.0, 0.0, 0.0),
            PositionControlPID(0.0, 0.0, 0.0) {
    }

    SingleWheelController::~SingleWheelController(void) {
        runThread = false;
        subscriberThread->join();
    }

    bool SingleWheelController::initRequest(hardware_interface::RobotHW* robot_hw,
                                              ros::NodeHandle&             root_nh,
                                              ros::NodeHandle&             controller_nh,
                                              ClaimedResources&            claimed_resources)
    {
        // check if construction finished cleanly
        if (state_ != CONSTRUCTED){
            ROS_ERROR("Cannot initialize this controller because it failed to be constructed");
            return false;
        }

        // get a pointer to the effort interface
        hardware_interface::EffortJointInterface* effort_hw = robot_hw->get<hardware_interface::EffortJointInterface>();
        if (!effort_hw)
        {
            ROS_ERROR("This controller requires a hardware interface of type hardware_interface::EffortJointInterface.");
            return false;
        }

        node_ = controller_nh;

        unpackParameter("MotorVoltageCommand", MaxVoltageCmd, 0.4);

        XmlRpc::XmlRpcValue joint_names;
        if (!node_.getParam("joints", joint_names)) {
            ROS_ERROR("No 'joints' in controller. (namespace: %s)",
                      node_.getNamespace().c_str());
            return false;
        }

        if (joint_names.getType() != XmlRpc::XmlRpcValue::TypeArray) {
            ROS_ERROR("'joints' is not a struct. (namespace: %s)",
                      node_.getNamespace().c_str());
            return false;
        }

        for (int i = 0; i < joint_names.size(); i++) {
            XmlRpc::XmlRpcValue &name_value = joint_names[i];
            if (name_value.getType() != XmlRpc::XmlRpcValue::TypeString) {
                ROS_ERROR("joints are not strings. (namespace: %s)",
                          node_.getNamespace().c_str());
                return false;
            }

            hardware_interface::JointHandle j = robot_hw->get<hardware_interface::EffortJointInterface>()->getHandle((std::string) name_value);
            joints_.push_back(j);
        }

        // Setup publishers and subscribers
        pub_wheel_status = new realtime_tools::RealtimePublisher<bobble_controllers::WheelStatus>(root_nh, "wheel_controller/wheel_status", 1);

        runThread = true;
        subscriberThread = new std::thread(&SingleWheelController::runSubscriber, this);

        // Setup PID Controllers
        //VelocityControlPID.setPID(VelocityControlKp, VelocityControlKi, 0.0);
        //VelocityControlPID.setOutputFilter(VelocityControlAlphaFilter);
        //VelocityControlPID.setMaxIOutput(VelocityControlMaxIntegralOutput);
        //VelocityControlPID.setOutputLimits(-VelocityControlOutputLimitDegrees * (M_PI / 180.0),
        //                                   VelocityControlOutputLimitDegrees * (M_PI / 180.0));
        //VelocityControlPID.setDirection(true);

        state_ = INITIALIZED;
        return true;
    }


    void SingleWheelController::starting(const ros::Time &time) {
        //! Initialize variables
        ActiveControlMode = ControlModes::IDLE;

        // Setup the Real-Time thread
        struct sched_param param;
        // set the priority high, but not so high it overrides the comm
        param.sched_priority = 95;
        if (sched_setscheduler(0, SCHED_FIFO, &param) == -1) {
            ROS_WARN("Failed to set real-time scheduler.");
            return;
        }
        if (mlockall(MCL_CURRENT | MCL_FUTURE) == -1) {
            ROS_WARN("Failed to lock memory.");
        }
    }

    void SingleWheelController::populateCommands()
    {
        /// Get commands
        /// Lock mutex to prevent subscriber from writing to the commands
        control_command_mutex.lock();
        /// TODO: update commands
        control_command_mutex.unlock();
    }

    void SingleWheelController::update(const ros::Time &time, const ros::Duration &duration) {
        /// Populate Command Variables
        populateCommands();

        /////////////////////////////////////////////////////////////////////////////////////////
        /// Perform the desired control depending on controller state
        /////////////////////////////////////////////////////////////////////////////////////////
        if (ActiveControlMode == ControlModes::IDLE) {
        } else if (ActiveControlMode == ControlModes::VOLTAGE) {
        } else if (ActiveControlMode == ControlModes::VELOCITY) {
        } else if (ActiveControlMode == ControlModes::POSITION) {
        } else {
        }

        /////////////////////////////////////////////////////////////////////////////////////////
        /// Report our status out using RT safe publisher
        /////////////////////////////////////////////////////////////////////////////////////////
        write_controller_status_msg();
    }

    void SingleWheelController::write_controller_status_msg() {
        if(pub_wheel_status->trylock()) {
            pub_wheel_status->msg_.ControlMode = ActiveControlMode;
            pub_wheel_status->msg_.MotorVelocity = MeasuredMotorPosition;
            pub_wheel_status->msg_.MotorPosition = MeasuredMotorVelocity;
        }
    }

}

PLUGINLIB_EXPORT_CLASS(bobble_controllers::SingleWheelController, controller_interface::ControllerBase
)
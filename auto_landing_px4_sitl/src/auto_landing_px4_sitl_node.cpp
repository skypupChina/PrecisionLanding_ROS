//visual-flow-landing
#include <ros/ros.h>
#include <geometry_msgs/PoseStamped.h>
#include <geometry_msgs/TwistStamped.h>
#include <mavros_msgs/CommandBool.h>
#include <mavros_msgs/State.h>
#include <mavros_msgs/SetMode.h>
#include <apriltags_ros/AprilTagDetectionArray.h>
#include <tf/tf.h>
#include <tf/transform_datatypes.h>

using namespace std;

geometry_msgs::TwistStamped vs_body_axis;
geometry_msgs::PoseStamped uavPose;
apriltags_ros::AprilTagDetectionArray apriltag_pos_msg;

double uavRollENU, uavPitchENU, uavYawENU;

double err_x, err_y, err_z;
double last_err_x, last_err_y, last_err_z;
double last_timestamp;

double xyP, xyI, xyD, zP, zI, zD, yawP, yawD;
double dt;

float uav_init_altitude = 0.0;
float uav_altitude = 0.0;     
float uav_x_distance = 0.0;
float uav_y_distance = 0.0;


bool flag_offboard_mode = false;
bool flag_enter_position_hold = false;

// 判断飞行模式
mavros_msgs::State current_state;
void stateReceived(const mavros_msgs::State::ConstPtr& msg) 
{
    static bool init_flag = false;
    current_state = *msg;

    if (current_state.mode == "OFFBOARD")
    {
        if(!init_flag)
        {
            ROS_INFO_STREAM("Offboard Mode"); 
            init_flag = true;         
        }

        flag_offboard_mode = true;
    }
    else
    {
        flag_offboard_mode = false;
        init_flag = false;  
    }
}

// 无人机位置和姿态，From 内部传感器
void uavPoseReceived(const geometry_msgs::PoseStampedConstPtr& msg)
{    
/*    uavPose.pose.position.x = msg->pose.position.x;
    uavPose.pose.position.y = msg->pose.position.y;
    uavPose.pose.position.z = msg->pose.position.z;
    uavPose.pose.orientation.x = msg->pose.orientation.x; // 四元数
    uavPose.pose.orientation.y = msg->pose.orientation.y;
    uavPose.pose.orientation.z = msg->pose.orientation.z;
    uavPose.pose.orientation.w = msg->pose.orientation.w;
    
    // Using ROS tf to get RPY angle from Quaternion
    tf::Quaternion quat;
    tf::quaternionMsgToTF(uavPose.pose.orientation, quat);  // 将四元数消息msg转换为四元数
    tf::Matrix3x3(quat).getRPY(uavRollENU, uavPitchENU, uavYawENU); // 由四元数得到欧拉角
    //ROS_INFO("Current UAV angles: roll=%0.3f, pitch=%0.3f, yaw=%0.3f", uavRollENU*180/3.1415926, uavPitchENU*180/3.1415926, uavYawENU*180/3.1415926);  
*/
    uav_altitude = msg->pose.position.z;
//    cout<<"The altitude is "<<uav_altitude<<endl;

    err_z = -uav_altitude;
//    cout<<" err_z"<<err_z<<endl;
}


//obtain the apriltags pose
void TagDetectionsReceived(const apriltags_ros::AprilTagDetectionArray::ConstPtr& msg)
{   
    static int flag_not_found_mark = 0;

     // 获取无人机相对apriltag的xy距离  
    apriltags_ros::AprilTagDetection tag_msg = msg->detections[0];

    uav_x_distance = tag_msg.pose.pose.position.x;
    uav_y_distance = tag_msg.pose.pose.position.y;
    
    if (abs(uav_x_distance) > 0.01f && abs(uav_y_distance) > 0.01f) 
    {

       // 将无人机与mark在 x y z 方向的距离偏差，分别表示为err_ ,便于控制部分的理解
        // x轴反向，y,z轴重合
        err_x = uav_x_distance;
        err_y = -uav_y_distance;

        //一旦发现标志，将未发现mark的计数标志复位0
        flag_not_found_mark = 0;

        cout<<" err_x "<<err_x<<endl;

        cout<<" err_y "<<err_y<<endl;

    } 
    
    // 说明： 1.8m 只是尝试值，明显有点大
    // offboard 模式下，高度大于1.8m？，连续 10次 未发现标志，认为目标丢失，自动进入定点悬停模式
    // 高度小于1.8m？，由于 Tag 占图像大部分，飞机晃动，Tag很容易出视野，识别失败，此时保持继续降落
    else    
    {
        flag_not_found_mark++;
        cout<<"It has been "<<flag_not_found_mark<<" times"<<endl;
        if (uav_altitude > 1.8 && (flag_not_found_mark > 3))
        {           
           ROS_INFO_STREAM("Fail to found mark, enter Position Hold");
           flag_enter_position_hold = true;
        }
        else if(uav_altitude < 1.8 && (flag_not_found_mark > 3))
        {
            err_x = 0.0;
            err_y = 0.0;
            last_err_x = 0.0;
            last_err_y = 0.0;
        }else{
            ROS_INFO_STREAM("000000000000000000000000000000000000");
        }
    }

    cout<<"TagDetectionsReceived! "<<endl;
}



// 飞机降落速度控制
void landingVelocityControl()
{
    vs_body_axis.header.seq++;
    vs_body_axis.header.stamp = ros::Time::now();
    //cout<<"landingvelocitycontrol errxy: "<<err_x<<" "<<err_y<<endl;
    dt = ros::Time::now().toSec() - last_timestamp;
    //velocity_z set as a constant // PD控制(x y方向)
    vs_body_axis.twist.linear.x = err_x * xyP + (err_x - last_err_x) / dt * xyD;
    vs_body_axis.twist.linear.y = err_y * xyP + (err_y - last_err_y) / dt * xyD;

    // output the vel
    //cout<<vs_body_axis.twist.linear.x<<"  "<< vs_body_axis.twist.linear.y<<endl;
/*    vs_body_axis.twist.linear.x = err_x * xyP;
    vs_body_axis.twist.linear.y = err_y * xyP;*/

    vs_body_axis.twist.linear.z = -0.35;//should it be different?
   
    // 速度限幅
    vs_body_axis.twist.linear.x = (vs_body_axis.twist.linear.x>0.8)?0.8\
                                  :((vs_body_axis.twist.linear.x<-0.8)?-0.8:vs_body_axis.twist.linear.x);
    vs_body_axis.twist.linear.y = (vs_body_axis.twist.linear.y>0.8)?0.8\
                                  :((vs_body_axis.twist.linear.y<-0.8)?-0.8:vs_body_axis.twist.linear.y);
    vs_body_axis.twist.linear.z = (vs_body_axis.twist.linear.z>0.8)?0.8\
                                  :((vs_body_axis.twist.linear.z<-0.8)?-0.8:vs_body_axis.twist.linear.z);  

    // 更新偏差值 
    last_err_x = err_x;
    last_err_y = err_y;
    last_timestamp = ros::Time::now().toSec();
}


int main(int argc, char **argv)
{
    ros::init(argc, argv, "velocity_marker_land_node");
    ros::NodeHandle nh;

        // arm 与 disarm service 使用
    mavros_msgs::CommandBool arm_cmd;
    arm_cmd.request.value = false; // 未解锁

    int flag_low_altitude = 0;
    int flag_time = 0;

    last_timestamp = ros::Time::now().toSec();
    ros::Time time_disarm(0);

    printf("------------landing control node running successfully-------------\n");

    // pub vel_sp
    ros::Publisher bodyAxisVelocityPublisher = nh.advertise<geometry_msgs::TwistStamped>("/mavros/setpoint_velocity/cmd_vel", 10);

    // sub flight mode
    ros::Subscriber stateSubscriber = nh.subscribe("mavros/state", 10, stateReceived);

    // sub uavpose
    ros::Subscriber uavPoseSubscriber = nh.subscribe("/mavros/local_position/pose", 1000, uavPoseReceived);

    // sub tag
    ros::Subscriber TagDetectionsSubscriber = nh.subscribe("/tag_detections",5,TagDetectionsReceived);  

    //ros::Publisher initial_pos_pub = nh.advertise<geometry_msgs::PoseStamped>("mavros/setpoint_position/local", 10);


    // client arm/disarmed
    ros::ServiceClient arming_client = nh.serviceClient<mavros_msgs::CommandBool>("/mavros/cmd/arming");

    // client set_mode
    //ros::ServiceClient set_mode_client = nh.serviceClient<mavros_msgs::SetMode>("/mavros/set_mode");

    last_err_x = 0;
    last_err_y = 0;
    last_err_z = 0;
    //last_err_yaw = 0;
    // cout<<"main in errxy: "<<err_x<<endl;
    // 获取 PID 参数值
    ros::param::param("~xyP", xyP, 0.3);
    ros::param::param("~xyD", xyD, 0.2);
    cout << "got xyP = " << xyP << endl;
    cout << "got xyD = " << xyD << endl;

    ros::Rate loopRate(50.0);
    //set mode offboard
    while(ros::ok()){
 
        if(flag_offboard_mode)
        {
            // 高度大于0.35m，正常控制飞行
            if(uav_altitude >= 0.35)
            {             
                // 切换到 offboard 模式，且未进入位置保持状态
                if(!flag_enter_position_hold)
                {
                    landingVelocityControl();   
                    //ROS_INFO_STREAM("Offboard auto control");     
                }
                // 在 offboard 模式下，进入位置保持状态, 未进入手动调节模式
                else
                {
                    ROS_INFO_STREAM("Position hold control");
                    vs_body_axis.header.seq++;
                    vs_body_axis.header.stamp = ros::Time::now();
                    vs_body_axis.twist.linear.x = 0.0;
                    vs_body_axis.twist.linear.y = 0.0;
                    vs_body_axis.twist.linear.z = 0.0;
                    vs_body_axis.twist.angular.z = 0.0;
                }

                flag_low_altitude = 0;
            }

            // 高度小于 0.35m，继续降落，1s 后飞机锁定(防止超声波数据出错，要求低高度预警连续触发3次)
            else
            {   
                ROS_INFO_STREAM("Almost landed");
                flag_low_altitude++;
                if(flag_low_altitude > 2)  
                {   //cout<<"hehehehhehehehehe "<<flag_low_altitude<<endl;
                    flag_low_altitude = 3;
                    vs_body_axis.header.seq++;
                    vs_body_axis.header.stamp = ros::Time::now();
                    vs_body_axis.twist.linear.x = 0;
                    vs_body_axis.twist.linear.y = 0;
                    vs_body_axis.twist.linear.z = -0.1;

                    if (flag_time == 0)
                    {        
                        time_disarm = ros::Time::now();
                        flag_time = 1;
                         //ROS_INFO_STREAM("meishibani");
                    }

                    if ((ros::Time::now() - time_disarm) > ros::Duration(1.0))
                    {
                        ROS_INFO_STREAM("getting here");
                        if( arming_client.call(arm_cmd) && arm_cmd.response.success)
                        {
                            ROS_INFO("Vehicle disarmed");
                            return 0;
                        }else{
                            ROS_INFO_STREAM("that's should not happen");
                        }
                    }
                }
            }
        }
        else
        {
            ROS_INFO_STREAM("Offboard not running");
        }

        // 此句为测试代码，不用 arm 飞机，直接看速度控制输出量 
        //landingVelocityControl();

        //发布速度控制量
        bodyAxisVelocityPublisher.publish(vs_body_axis);
      
        ros::spinOnce();
        loopRate.sleep();
    }
    
    return 0;
}


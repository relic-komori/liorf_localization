#pragma once
#ifndef _UTILITY_LIDAR_ODOMETRY_H_
#define _UTILITY_LIDAR_ODOMETRY_H_
#define PCL_NO_PRECOMPILE 

#include <ros/ros.h>

#include <std_msgs/Header.h>
#include <common_lib.h>
#include <std_msgs/Float64MultiArray.h>
#include <sensor_msgs/Imu.h>
#include <sensor_msgs/PointCloud2.h>
#include <sensor_msgs/NavSatFix.h>
#include <nav_msgs/Odometry.h>
#include <nav_msgs/Path.h>
#include <geometry_msgs/PoseWithCovarianceStamped.h>
#include <geometry_msgs/TwistStamped.h>
#include <visualization_msgs/Marker.h>
#include <visualization_msgs/MarkerArray.h>

#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/search/impl/search.hpp>
#include <pcl/range_image/range_image.h>
#include <pcl/kdtree/kdtree_flann.h>
#include <pcl/common/common.h>
#include <pcl/common/transforms.h>
#include <pcl/registration/icp.h>
#include <pcl/io/pcd_io.h>
#include <pcl/filters/filter.h>
#include <pcl/filters/voxel_grid.h>
#include <pcl/filters/crop_box.h> 
#include <pcl_conversions/pcl_conversions.h>

#include <tf/LinearMath/Quaternion.h>
#include <tf/transform_listener.h>
#include <tf/transform_datatypes.h>
#include <tf/transform_broadcaster.h>
 
#include <vector>
#include <cmath>
#include <algorithm>
#include <queue>
#include <deque>
#include <iostream>
#include <fstream>
#include <ctime>
#include <cfloat>
#include <iterator>
#include <sstream>
#include <string>
#include <limits>
#include <iomanip>
#include <array>
#include <thread>
#include <mutex>

#include <opencv2/opencv.hpp>
// OpenCVのインクルードを修正
// エラー回避のため最後に読み込み（参考: https://stackoverflow.com/questions/42504592/flann-util-serialization-h-class-stdunordered-mapunsigned-int-stdvectorun ）

using namespace std;

typedef pcl::PointXYZI PointType;

// <!-- liorf_localization_yjz_lucky_boy -->
std::shared_ptr<CommonLib::common_lib> common_lib_;

enum class SensorType { VELODYNE, OUSTER, LIVOX, ROBOSENSE, MULRAN};

class ParamServer
{
public:

    ros::NodeHandle nh;

    std::string robot_id;

    //Topics
    string pointCloudTopic;
    string imuTopic;
    string odomTopic;
    string gpsTopic;

    //Frames
    string lidarFrame;
    string baselinkFrame;
    string odometryFrame;
    string mapFrame;

    // GPS Settings
    bool useImuHeadingInitialization;
    bool useGpsElevation;
    float gpsCovThreshold;
    float poseCovThreshold;

    // Save pcd
    bool savePCD;
    string savePCDDirectory;

    // Lidar Sensor Configuration
    SensorType sensor;
    int N_SCAN;
    int Horizon_SCAN;
    int downsampleRate;
    int point_filter_num;
    float lidarMinRange;
    float lidarMaxRange;

    // IMU
    int imuType;
    float imuRate;
    float imuAccNoise;
    float imuGyrNoise;
    float imuAccBiasN;
    float imuGyrBiasN;
    float imuGravity;
    float imuRPYWeight;
    vector<double> extRotV;
    vector<double> extRPYV;
    vector<double> extTransV;
    Eigen::Matrix3d extRot;
    Eigen::Matrix3d extRPY;
    Eigen::Vector3d extTrans;
    Eigen::Quaterniond extQRPY;

    // voxel filter paprams
    float mappingSurfLeafSize ;
    float surroundingKeyframeMapLeafSize;
    float loopClosureICPSurfLeafSize ;

    float z_tollerance; 
    float rotation_tollerance;

    // CPU Params
    int numberOfCores;
    double mappingProcessInterval;

    // Surrounding map
    float surroundingkeyframeAddingDistThreshold; 
    float surroundingkeyframeAddingAngleThreshold; 
    float surroundingKeyframeDensity;
    float surroundingKeyframeSearchRadius;
    
    // Loop closure
    bool  loopClosureEnableFlag;
    float loopClosureFrequency;
    int   surroundingKeyframeSize;
    float historyKeyframeSearchRadius;
    float historyKeyframeSearchTimeDiff;
    int   historyKeyframeSearchNum;
    float historyKeyframeFitnessScore;

    // global map visualization radius
    float globalMapVisualizationSearchRadius;
    float globalMapVisualizationPoseDensity;
    float globalMapVisualizationLeafSize;

    ParamServer()
    {
        nh.param<std::string>("/robot_id", robot_id, "roboat");

        nh.param<std::string>("liorf_localization/pointCloudTopic", pointCloudTopic, "points_raw");
        nh.param<std::string>("liorf_localization/imuTopic", imuTopic, "imu_correct");
        nh.param<std::string>("liorf_localization/odomTopic", odomTopic, "odometry/imu");
        nh.param<std::string>("liorf_localization/gpsTopic", gpsTopic, "odometry/gps");

        nh.param<std::string>("liorf_localization/lidarFrame", lidarFrame, "base_link");
        nh.param<std::string>("liorf_localization/baselinkFrame", baselinkFrame, "base_link");
        nh.param<std::string>("liorf_localization/odometryFrame", odometryFrame, "odom");
        nh.param<std::string>("liorf_localization/mapFrame", mapFrame, "map");

        nh.param<bool>("liorf_localization/useImuHeadingInitialization", useImuHeadingInitialization, false);
        nh.param<bool>("liorf_localization/useGpsElevation", useGpsElevation, false);
        nh.param<float>("liorf_localization/gpsCovThreshold", gpsCovThreshold, 2.0);
        nh.param<float>("liorf_localization/poseCovThreshold", poseCovThreshold, 25.0);

        nh.param<bool>("liorf_localization/savePCD", savePCD, false);
        nh.param<std::string>("liorf_localization/savePCDDirectory", savePCDDirectory, "/Downloads/LOAM/");

        std::string sensorStr;
        nh.param<std::string>("liorf_localization/sensor", sensorStr, "");
        if (sensorStr == "velodyne")
        {
            sensor = SensorType::VELODYNE;
        }
        else if (sensorStr == "ouster")
        {
            sensor = SensorType::OUSTER;
        }
        else if (sensorStr == "livox")
        {
            sensor = SensorType::LIVOX;
        } else if  (sensorStr == "robosense") 
        {
            sensor = SensorType::ROBOSENSE;
        } else if  (sensorStr == "mulran") 
        {
            sensor = SensorType::MULRAN;
        } else {
            ROS_ERROR_STREAM(
                "Invalid sensor type (must be either 'velodyne' or 'ouster' or 'livox' or 'robosense' or 'mulran'): " << sensorStr);
            ros::shutdown();
        }

        nh.param<int>("liorf_localization/N_SCAN", N_SCAN, 16);
        nh.param<int>("liorf_localization/Horizon_SCAN", Horizon_SCAN, 1800);
        nh.param<int>("liorf_localization/downsampleRate", downsampleRate, 1);
        nh.param<int>("liorf_localization/point_filter_num", point_filter_num, 3);
        nh.param<float>("liorf_localization/lidarMinRange", lidarMinRange, 1.0);
        nh.param<float>("liorf_localization/lidarMaxRange", lidarMaxRange, 1000.0);

        nh.param<int>("liorf_localization/imuType", imuType, 0);
        nh.param<float>("liorf_localization/imuRate", imuRate, 500.0);
        nh.param<float>("liorf_localization/imuAccNoise", imuAccNoise, 0.01);
        nh.param<float>("liorf_localization/imuGyrNoise", imuGyrNoise, 0.001);
        nh.param<float>("liorf_localization/imuAccBiasN", imuAccBiasN, 0.0002);
        nh.param<float>("liorf_localization/imuGyrBiasN", imuGyrBiasN, 0.00003);
        nh.param<float>("liorf_localization/imuGravity", imuGravity, 9.80511);
        nh.param<float>("liorf_localization/imuRPYWeight", imuRPYWeight, 0.01);
        nh.param<vector<double>>("liorf_localization/extrinsicRot", extRotV, vector<double>());
        nh.param<vector<double>>("liorf_localization/extrinsicRPY", extRPYV, vector<double>());
        nh.param<vector<double>>("liorf_localization/extrinsicTrans", extTransV, vector<double>());
        extRot = Eigen::Map<const Eigen::Matrix<double, -1, -1, Eigen::RowMajor>>(extRotV.data(), 3, 3);
        extRPY = Eigen::Map<const Eigen::Matrix<double, -1, -1, Eigen::RowMajor>>(extRPYV.data(), 3, 3);
        extTrans = Eigen::Map<const Eigen::Matrix<double, -1, -1, Eigen::RowMajor>>(extTransV.data(), 3, 1);
        extQRPY = Eigen::Quaterniond(extRPY).inverse();

        nh.param<float>("liorf_localization/mappingSurfLeafSize", mappingSurfLeafSize, 0.2);
        nh.param<float>("liorf_localization/surroundingKeyframeMapLeafSize", surroundingKeyframeMapLeafSize, 0.2);

        nh.param<float>("liorf_localization/z_tollerance", z_tollerance, FLT_MAX);
        nh.param<float>("liorf_localization/rotation_tollerance", rotation_tollerance, FLT_MAX);

        nh.param<int>("liorf_localization/numberOfCores", numberOfCores, 2);
        nh.param<double>("liorf_localization/mappingProcessInterval", mappingProcessInterval, 0.15);

        nh.param<float>("liorf_localization/surroundingkeyframeAddingDistThreshold", surroundingkeyframeAddingDistThreshold, 1.0);
        nh.param<float>("liorf_localization/surroundingkeyframeAddingAngleThreshold", surroundingkeyframeAddingAngleThreshold, 0.2);
        nh.param<float>("liorf_localization/surroundingKeyframeDensity", surroundingKeyframeDensity, 1.0);
        nh.param<float>("liorf_localization/loopClosureICPSurfLeafSize", loopClosureICPSurfLeafSize, 0.3);
        nh.param<float>("liorf_localization/surroundingKeyframeSearchRadius", surroundingKeyframeSearchRadius, 50.0);

        nh.param<bool>("liorf_localization/loopClosureEnableFlag", loopClosureEnableFlag, false);
        nh.param<float>("liorf_localization/loopClosureFrequency", loopClosureFrequency, 1.0);
        nh.param<int>("liorf_localization/surroundingKeyframeSize", surroundingKeyframeSize, 50);
        nh.param<float>("liorf_localization/historyKeyframeSearchRadius", historyKeyframeSearchRadius, 10.0);
        nh.param<float>("liorf_localization/historyKeyframeSearchTimeDiff", historyKeyframeSearchTimeDiff, 30.0);
        nh.param<int>("liorf_localization/historyKeyframeSearchNum", historyKeyframeSearchNum, 25);
        nh.param<float>("liorf_localization/historyKeyframeFitnessScore", historyKeyframeFitnessScore, 0.3);

        nh.param<float>("liorf_localization/globalMapVisualizationSearchRadius", globalMapVisualizationSearchRadius, 1e3);
        nh.param<float>("liorf_localization/globalMapVisualizationPoseDensity", globalMapVisualizationPoseDensity, 10.0);
        nh.param<float>("liorf_localization/globalMapVisualizationLeafSize", globalMapVisualizationLeafSize, 1.0);

        usleep(100);
    }

    sensor_msgs::Imu imuConverter(const sensor_msgs::Imu& imu_in)
    {
        sensor_msgs::Imu imu_out = imu_in;
        // rotate acceleration
        Eigen::Vector3d acc(imu_in.linear_acceleration.x, imu_in.linear_acceleration.y, imu_in.linear_acceleration.z);
        acc = extRot * acc;
        imu_out.linear_acceleration.x = acc.x();
        imu_out.linear_acceleration.y = acc.y();
        imu_out.linear_acceleration.z = acc.z();
        // rotate gyroscope
        Eigen::Vector3d gyr(imu_in.angular_velocity.x, imu_in.angular_velocity.y, imu_in.angular_velocity.z);
        gyr = extRot * gyr;
        imu_out.angular_velocity.x = gyr.x();
        imu_out.angular_velocity.y = gyr.y();
        imu_out.angular_velocity.z = gyr.z();

        if (imuType) {
            // rotate roll pitch yaw
            Eigen::Quaterniond q_from(imu_in.orientation.w, imu_in.orientation.x, imu_in.orientation.y, imu_in.orientation.z);
            Eigen::Quaterniond q_final = q_from * extQRPY;
            imu_out.orientation.x = q_final.x();
            imu_out.orientation.y = q_final.y();
            imu_out.orientation.z = q_final.z();
            imu_out.orientation.w = q_final.w();

            if (sqrt(q_final.x()*q_final.x() + q_final.y()*q_final.y() + q_final.z()*q_final.z() + q_final.w()*q_final.w()) < 0.1)
            {
                ROS_ERROR("Invalid quaternion, please use a 9-axis IMU!");
                ros::shutdown();
            }
        }

        return imu_out;
    }
};

template<typename T>
sensor_msgs::PointCloud2 publishCloud(const ros::Publisher& thisPub, const T& thisCloud, ros::Time thisStamp, std::string thisFrame)
{
    sensor_msgs::PointCloud2 tempCloud;
    pcl::toROSMsg(*thisCloud, tempCloud);
    tempCloud.header.stamp = thisStamp;
    tempCloud.header.frame_id = thisFrame;
    if (thisPub.getNumSubscribers() != 0)
        thisPub.publish(tempCloud);
    return tempCloud;
}

template<typename T>
double ROS_TIME(T msg)
{
    return msg->header.stamp.toSec();
}


template<typename T>
void imuAngular2rosAngular(sensor_msgs::Imu *thisImuMsg, T *angular_x, T *angular_y, T *angular_z)
{
    *angular_x = thisImuMsg->angular_velocity.x;
    *angular_y = thisImuMsg->angular_velocity.y;
    *angular_z = thisImuMsg->angular_velocity.z;
}


template<typename T>
void imuAccel2rosAccel(sensor_msgs::Imu *thisImuMsg, T *acc_x, T *acc_y, T *acc_z)
{
    *acc_x = thisImuMsg->linear_acceleration.x;
    *acc_y = thisImuMsg->linear_acceleration.y;
    *acc_z = thisImuMsg->linear_acceleration.z;
}


template<typename T>
void imuRPY2rosRPY(sensor_msgs::Imu *thisImuMsg, T *rosRoll, T *rosPitch, T *rosYaw)
{
    double imuRoll, imuPitch, imuYaw;
    tf::Quaternion orientation;
    tf::quaternionMsgToTF(thisImuMsg->orientation, orientation);
    tf::Matrix3x3(orientation).getRPY(imuRoll, imuPitch, imuYaw);

    *rosRoll = imuRoll;
    *rosPitch = imuPitch;
    *rosYaw = imuYaw;
}

#endif

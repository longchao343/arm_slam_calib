/*
 * touch_filter_sim.cpp
 *
 *  Created on: Oct 14, 2015
 *      Author: mklingen
 */

// Camera observations of landmarks (i.e. pixel coordinates) will be stored as Point2 (x, y).
#include <gtsam/geometry/Point2.h>

// Inference and optimization
#include <gtsam/inference/Symbol.h>
#include <gtsam/nonlinear/DoglegOptimizer.h>
#include <gtsam/nonlinear/Values.h>
#include <gtsam/nonlinear/NonlinearFactorGraph.h>

// SFM-specific factors
#include <gtsam/slam/PriorFactor.h>
#include <gtsam/slam/GeneralSFMFactor.h> // does calibration !
#include <gtsam/slam/ProjectionFactor.h>

#include <dart/dart.h>
#include <stdio.h>
#include <ros/ros.h>
#include <fstream>
#include <aikido/rviz/InteractiveMarkerViewer.hpp>
#include <aikido/util/CatkinResourceRetriever.hpp>

#include <gtsam/nonlinear/NonlinearFactorGraph.h>
#include <arm_slam_calib/EncoderFactor.h>
#include <arm_slam_calib/RobotProjectionFactor.h>
#include <arm_slam_calib/RobotGenerator.h>

#include <gtsam/nonlinear/LevenbergMarquardtOptimizer.h>
#include <gtsam/nonlinear/Marginals.h>

#include <visualization_msgs/Marker.h>
#include <visualization_msgs/MarkerArray.h>

#include <map>

#include <arm_slam_calib/ArmSlamCalib.h>



int main(int argc, char** argv)
{
    ros::init(argc, argv, "arm_calib_sim");
    ros::NodeHandle nh("~");
    gtsam::ArmSlamCalib::Params params;
    params.simulated = true;
    params.optimizationMode = gtsam::ArmSlamCalib::ISAM;
    gtsam::ArmSlamCalib calib(nh, std::make_shared<std::mutex>(), params);

    std::vector<std::string> joints;
    joints.push_back("mico_joint_1");
    joints.push_back("mico_joint_2");
    joints.push_back("mico_joint_3");
    joints.push_back("mico_joint_4");
    joints.push_back("mico_joint_5");
    joints.push_back("mico_joint_6");

    std::string cameraName = "mico_end_effector";
    calib.InitRobot("package://ada_description/robots/mico.urdf", joints, cameraName);
    calib.CreateSimulation("/home/mklingen/prdev/src/arm_slam_calib/data/traj.txt");

    dart::dynamics::SkeletonPtr genRobot = dart::RobotGenerator::GenerateRobot(20, 0.5, 0.7, 0.01, 0.1);

    calib.GetViewer()->addSkeleton(genRobot);

    ros::Rate hz(30);

    for (size_t i = 0; i < 100; i++)
    {
        hz.sleep();
        calib.UpdateViewer();
        ros::spinOnce();
    }

    std::ofstream errorstream("error.txt");
    size_t iters = calib.GetParams().trajectorySize;
    bool drawLandmark = true;
    bool drawObs = false;
    bool drawCamera = false;
    bool drawTraj = true;

    std::ofstream offsetFile("offsets.txt", std::ios::out);
    std::ofstream reprojectionFile("reproj_error.txt", std::ios::out);
    std::ofstream extrinsicFile("extrinsic_errors.txt", std::ios::out);

    Eigen::VectorXd genPosition = Eigen::VectorXd::Zero(genRobot->getNumDofs());
    for(size_t i = 0; i < iters; i++)
    {
        genRobot->setPositions(genPosition);
        for (size_t k = 0; k < genRobot->getNumDofs(); k++)
        {
            genPosition(k) += utils::Rand(-0.01, 0.01);
        }

        calib.SimulationStep(i);
        if (i > 1)
        {
            calib.OptimizeStep();
        }
        calib.DrawState(i, 0, calib.initialEstimate, 0.0f, 0.8f, 0.8f, 1.0f,  drawLandmark, drawTraj, drawObs, drawCamera);
        calib.DrawState(i, 1, calib.currentEstimate, 0.8f, 0.0f, 0.0f, 1.0f,  drawLandmark, drawTraj, drawObs, drawCamera);
        calib.DrawState(i, 2, calib.groundTruth, 0.0f, 0.8f, 0.0f, 1.0f,  drawLandmark, drawTraj, drawObs, drawCamera);
        calib.UpdateViewer();

        gtsam::Vector err = calib.ComputeLatestJointAngleOffsets(i);

        for(size_t j = 0; j < 6; j++)
        {
            offsetFile << err(j);

            offsetFile << " ";
        }
        gtsam::Vector err_sim = calib.ComputeLatestGroundTruthSimOffsets(i);

        for(size_t j = 0; j < 6; j++)
        {
            offsetFile << err_sim(j);

            if (j < 5)
            {
                offsetFile << " ";
            }
        }

        offsetFile << std::endl;


        gtsam::Pose3 extCur = calib.currentEstimate.at<gtsam::Pose3>(gtsam::Symbol('K', 0));
        gtsam::Quaternion extCurQ = extCur.rotation().toQuaternion();
        gtsam::Quaternion simExtQ =  calib.GetSimExtrinsic().rotation().toQuaternion();
        extrinsicFile << calib.GetSimExtrinsic().translation().x() << " "
                         <<  calib.GetSimExtrinsic().translation().y() << " "
                         <<  calib.GetSimExtrinsic().translation().z() << " "
                         << simExtQ.x() << " "
                         << simExtQ.y() << " "
                         << simExtQ.z() << " "
                         << simExtQ.w() << " ";

        extrinsicFile << extCur.translation().x() << " "
                         << extCur.translation().y() << " "
                         << extCur.translation().z() << " "
                         << extCurQ.x() << " "
                         << extCurQ.y() << " "
                         << extCurQ.z() << " "
                         << extCurQ.w();
        extrinsicFile << std::endl;


        hz.sleep();
        ros::spinOnce();

        gtsam::ArmSlamCalib::CalibrationError error = calib.ComputeError(calib.groundTruth, calib.currentEstimate);
        errorstream << error.landmarkError << " " << error.extrinsicError << " " << error.jointAngleError << std::endl;
    }

    errorstream.close();
    extrinsicFile.close();
    offsetFile.close();
    calib.PrintSimErrors(".", "");

    while(ros::ok())
    {
        calib.DrawState(params.trajectorySize, 0, calib.initialEstimate, 0, 0.8, 0.8, 1.0, drawLandmark, drawTraj, drawObs, drawCamera);
        calib.DrawState(params.trajectorySize, 1, calib.currentEstimate, 0.8, 0.0, 0.0, 1.0, drawLandmark, drawTraj, drawObs, drawCamera);
        calib.DrawState(params.trajectorySize, 2, calib.groundTruth, 0.0, 0.8, 0.0, 1.0, drawLandmark, drawTraj, drawObs, drawCamera);
        calib.UpdateViewer();
        hz.sleep();
        ros::spinOnce();
    }
}


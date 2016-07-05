# arm_slam_calib

This experimental package tracks and calibrates a robot arm using monocular visual SLAM. It was created as a proof of concept for my ([Matt Klingensmith's](github.com/mklingen) thesis. It provides the following:

* Joint angle estimates for the robot's trajectory
* The extrinsic calibration of a monocular camera with respect to the robot's body

It is designed for serial-link robot manipulators with a single monocular camera on the end effector or some other part of the robot's body. The user must provide the robot's input. The SLAM system reads the robot's joint angles and images from the camera through ROS. Visualizations are provided through ROS. Depth images (for instance from Kinect-like projective light sensors) may be used for visualization, but are not used for calibration/tracking.

#Requirements#
* A serial link robot manipulator emitting joint states through ROS.
* A monocular camera that has been intrinsically calibrated, emitting RGB images and intrinsic calibration through ROS
* [URDF file](http://wiki.ros.org/urdf) for your robot
* ROS indigo or higher (probably on Linux, preferably Ubuntu)
* catkin build system

##ROS Packages##
* geometry_msgs
* cv_bridge
* sensor_msgs
* std_msgs
* roscpp
* image_transport
* visualization_msgs
* pcl_ros
* [apriltags](https://github.com/personalrobotics/apriltags)
* [joint_state_recorder](https://github.com/personalrobotics/joint_state_recorder)

##Non-ROS Dependencies##
* [AIKIDO](https://github.com/personalrobotics/aikido)
* [DART](https://github.com/dartsim/dart)
* [BRISK](https://github.com/clemenscorny/brisk)
* [OpenCV](https://github.com/opencv/opencv)
* [OpenGV](https://github.com/laurentkneip/opengv)
* [GTSAM](https://github.com/devbharat/gtsam)
* [Eigen3](http://eigen.tuxfamily.org/index.php?title=Main_Page)
* [PCL](https://github.com/PointCloudLibrary/pcl)
* [Assimp](https://github.com/assimp/assimp)
* [TBB](https://github.com/wjakob/tbb)
* C++11

# Usage #
The tracking/calibration algorithm can be configured to run with or without fiducials. 

To use fiducials, you will have to first get an [April Tags](https://github.com/personalrobotics/apriltags) node up and running, observing april tags in the environment. The SLAM system can use the april tags messages directly. It will also subscribe to the color images (or depth images, where available) for debugging purposes. 

The system can also be run entirely without fiducials, using the texture in the environment from [BRISK](https://github.com/clemenscorny/brisk) features. Make sure that the robot is in a highly textured, static environment. [Calibrate the intrinsics](http://wiki.ros.org/openni_launch/Tutorials/IntrinsicCalibration) of your camera first. They must be output to a `CameraInfo` topic through ROS. The images can be of any format. `BGR8` and `mono8` have been tested. Your mileage may vary.

## Launch Files ##
The launch file `launch_mico_brisk.launch` is provided as an example of how to use the SLAM system to calibrate a robot arm without fiducials. Here are the contents of that file with comments explaining each parameter.

```xml
<!--
This launch file was written to calibrate a Kinova Mico robot with an Asus Xtion Pro mounted on its wrist,
without the use of fiducials. Modify it to suit your needs.
-->
<launch>
     <node name="arm_slam_calib" pkg="arm_slam_calib" type="real" output="screen">
     <!-- Set this to 'true' if you are running from a bag file or are using /clock -->
     <param name="use_sim_time"                   value="true" />
     <!-- Variance on the joint encoder readings in radians. Higher means more noise -->
     <param name="encoder_noise_level"            value="0.01"/>
     <!-- Noise on the extrinsic initial guess in meters. Higher means less certainty -->
     <param name ="extrinsic_noise_level"         value="0.2"/>
     <!-- Noise on the extrinsic initial guess in radians. Higher means less certainty -->
     <param name="extrinsic_rot_noise_level"      value="0.1"/>
     <!-- Noise on landmark locations in meters. Higher means less certainty -->
     <param name="landmark_noise_level"           value="20.0"/>
     <!-- Noise in the camera projection function in pixels. Higher means less certainty. -->
     <param name="projection_noise_level"         value="1.0"/>
     <!-- Whether or not to use RANSAC to estimate the depth of landmrks. 
          Turning this off improves speed, but landmarks will have random initial depths -->
     <param name="run_ransac"                     value="true"/>
     <!-- When true, saves debug images of landmarks. WARNING: very slow! -->
     <param name="save_images"                    value="false"/>
     <!-- When true, uses depth images to create low-rez point clouds for visualization -->
     <param name="generate_stitched_point_clouds" value="true"/>
     <!-- When true, uses depth images to create a live point cloud for visualization -->
     <param name="generate_current_point_cloud"   value="true"/>
     <!-- When true, dsplays a ghostly robot in rviz representing the estimated
          joint angles -->
     <param name="draw_estimate_robot"            value="false"/>
     <!-- When true, turns on a deadband in which encoder readings are believed
          to be inaccurate -->
     <param name="use_encoder_deadband"           value="false"/>
     <!-- Size of the encoder deadband in radians -->
     <param name="encoder_deadband"               value="0.05"/>
     <!-- SLAM method to use. Valid methods are ISAM, BatchLM, and BatchDogleg -->
     <param name="optimization_mode"              value="ISAM"/>
     <!-- Region of interest on the camera in pixels. -->
     <param name="min_x"                      value="0"/>
     <param name="min_y"                      value="0"/>
     <param name="max_x"                      value="99999"/>
     <param name="max_y"                      value="99999"/>
     <!-- When true, matches features between images. Turn off to completely
          disable visual SLAM -->
     <param name="do_feature_matching"            value="true"/>
     <!-- Threshold on hamming distance for BRISK feature matches. Lower values
          result in fewer false positives -->
     <param name="hamming_threshold"              value="60"/>
     <!-- A feature will only be matched when its score relative to the next
          best score exceeds this value. -->
     <param name="ratio_threshold"                value="1.25"/>
     <!-- When true, draws successfully matched features in RVIZ -->
     <param name="draw_good_features"             value="true"/>
     <!-- When true, draws lines representing successfull matches in RVIZ -->
     <param name="draw_good_matches"              value="true"/>
     <!-- When true, draws unsucessfully matched features in RVIZ -->
     <param name="draw_bad_features"              value="false"/>
     <!-- When true, draws matches between features that didn't meet the required
          thresholds as red lines -->
     <param name="draw_bad_matches"               value="false"/>
     <!-- When true, draws all candidate landmarks in RVIZ (not just matched
          candidates) as yellow dots. -->
     <param name="draw_candidates"                value="false"/>
     <!-- When true, draws a grid below the robot in RVIZ. -->
     <param name="draw_grid"                      value="true"/>
     <!-- These joints in the URDF will be optimized. All other joints will be 
          considered to have no uncertainty -->
     <param name="free_joints"                    value="[mico_joint_1, mico_joint_2, mico_joint_3, mico_joint_4, mico_joint_5, mico_joint_6]"/>
     <!-- Link in the URDF that the camera is mounted to. Extrinsic will be reported
          relative to this link -->
     <param name="camera_mount_link"              value="mico_end_effector"/>
     <!-- Location of the robot's URDF. package://, file:// or http:// can be used. -->
     <param name="urdf"                           value="package://ada_description/robots/mico.urdf"/>
     <!-- Topic that the camera is publishing color images to (mono also works)-->
     <param name="rgb_topic"                      value="/camera/rgb/image_rect_color"/>
     <!-- Topic containing the camera intrinsics (must be pre-calibrated) -->
     <param name="rgb_info"                       value="/camera/rgb/camera_info"/>
     <!--- (optional) The topic of a depth camera for visualization -->
     <param name="depth_topic"                    value="/camera/depth/image_rect_raw"/>
     <!-- (optional) The camera info topic of a depth camera for visualization -->
     <param name="depth_info"                     value="/camera/depth/camera_info"/>
     <!-- Topic containing the robot joint angles -->
     <param name="joint_state_topic"              value="/mico_arm_driver/out/joint_state"/>
     <!-- Initial guess of the camera extrinsic w.r.t the robot's link in meters. -->
     <rosparam param="extrinsic_initial_guess_translation">[0.0136688, 0.0486435, -0.0924387]</rosparam>
     <!-- Initial guess of the camera extrinsic Euler angles -->
     <rosparam param="extrinsic_initial_guess_rotation">[0, 0, 3.14159]</rosparam>
     <!-- Smoothness parameter in radians. Turn on when velocity measurements unavailable -->
     <param name="drift_noise"    value="0.05"/>
     <!-- Smoothness parameter. Use only when velocit measurements unavailable -->
     <param name="use_drift_noise" value="false"/>
     <!-- Robustness parameter for rejecting outliers. Higher values mean less 
          sensitivity to outliers but slower convergence. -->
     <param name="robust_cauchy_multiplier" value="3.0"/>
     <!-- When true, draws 3D landmarks to RVIZ -->
     <param name="draw_landmarks" value="true"/>
     <!-- When true, draws stitched point clouds to RVIZ -->
     <param name="draw_pointclouds" value="true"/>
     <!-- When true, draws the marginal covariance of the camera extrinsic
          to RVIZ as a yellow spheroid. -->
     <param name="draw_extrinsic_marginal" value="false"/>
     <!-- When true, draws observations to landmarks as lines in RVIZ -->
     <param name="draw_observation" value="false"/>
     <!-- When true, draws the camera's trajectory as a line in RVIZ -->
     <param name="draw_trajectory" value="false"/>
     <!-- Threshold in radians in which to accept a new keyframe for the SLAM
          system. Smaller values mean more keyframes but slower performance -->
     <param name="keyframe_threshold" value="0.05"/>
     <!-- This can be set to false to completely turn off SLAM for debugging
          purposes -->
     <param name="do_slam" value="true"/>
     <!-- When true, saves the stitched point clouds to a file on exit -->
     <param name="save_pointcloud" value="false"/>
     <!-- When true, saves some statistics about the SLAM system to ~/.ros -->
     <param name="save_stats" value="true"/>
    </node>
</launch>
```

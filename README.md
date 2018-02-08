# visual-tactile-localization-simulation[WIP]
Gazebo based simulation scenario for a visual-tactile localization algorithm.

## Requirements[WIP]
- [YARP](http://www.yarp.it/)
- [gazebo-yarp-plugins (forked)](https://github.com/xEnVrE/gazebo-yarp-plugins/) including:
  - `GazeboYarpModelPosePublisher` (publish pose of a model using [yarp::dev::FrameTransformServer](http://www.yarp.it/classyarp_1_1dev_1_1FrameTransformServer.html))
  - `FakePointCloud` (send on a port a fake point cloud given the mesh of a model and the origin of a observer)
  - `FakePointCloudViewer` (show the fake point cloud within Gazebo)
  - `EstimateViewer` (show the estimated pose as an additional transparent visual element within Gazebo)
- [Gazebo](http://gazebosim.org/) (tested with version `7.9.0`)
- [icub-gazebo](https://github.com/robotology/icub-gazebo/)
- [visual-tactile-localization](https://github.com/robotology-playground/visual-tactile-localization)

### VCG
The plugin `FakePointCloud` uses the header-only library [VCG](http://vcg.isti.cnr.it/vcglib/) to sample point clouds.It is provided within the header files of the plugin.

### Configure the environment
It is supposed that you have already installed `yarp` using two directories one for code, i.e. `$ROBOT_CODE`, and one for installed stuff, i.e. `$ROBOT-INSTALL` (this is not strictly required but it helps in setting the environment variables required for Gazebo within yarpmanager). Also it is supposed that `Gazebo 7` has already been installed.

#### Get the code[WIP]
```
cd $ROBOT_CODE
git clone https://github.com/robotology-playground/visual-tactile-localization.git
git clone https://github.com/robotology-playground/visual-tactile-localization-simulation.git
git clone https://github.com/xEnVrE/gazebo-yarp-plugins.git
git clone https://github.com/robotology/icub-gazebo.git
```
#### Install visual-tactile-localization
```
cd $ROBOT_CODE/visual-tactile-localization
git checkout feature/rf_module
mkdir build && cd build
cmake ../
make install
```
This package provides a module `upf-localizer` and a context `simVisualTactileLocalization` containg configuration file for the localizer as well as `.OFF` mesh files of the models.

#### Install visual-tactile-localization-simulation
```
cd $ROBOT_CODE/visual-tactile-localization-simulation
mkdir build && cd build
cmake ../
make install
```
This package provides a module `visual-tactile-localization-sim` and two applications description `xml`s in ICUBcontrib:
- `visual-tactile-sim_system.xml` to launch the entire simulation setup; 
- `visual-tactile-sim_app.xml` to launch the module `visual-tactile-localization-sim` once the setup is online;

#### Install gazebo-yarp-plugins
```
cd $ROBOT_CODE/gazebo-yarp-plugins
git checkout visual_tactile_loc_plugins
mkdir build && cd build
cmake ../ -DCMAKE_INSTALL_PREFIX=$ROBOT_INSTALL
make install
```

#### Install icub-gazebo
Cloning is sufficient. However there is an open [issue](https://github.com/robotology/QA/issues/115) that prevents from using Gazebo with the YARP Cartesian Interface if low level velocity control is used which is the default in the `simCartesianControl` context. To switch to low level position control, which is working, the option `PositionControl` has to be modified in the files `$ROBOT_INSTALL/share/iCub/contexts/simCartesianControl/cartesian/{left,right}_arm_cartesian.xml`
```
<devices robot="icubSim" build="0">
    <device name="left_arm_cartesian" type="cartesiancontrollerserver">
        <group name="GENERAL">
            ...
            <!-- <param name="PositionControl">off</param> -->
            <param name="PositionControl">on</param>
            ...
        </group>
        ...
```

### Notes about the application description file[WIP]
After installation an application description xml file should be available in 
```
$ROBOT_INSTALL/share/ICUBcontrib/applications/app_visual-tactile-sim.xml
```
It consists of the following modules:
#### yarpdev
`yarpdev` runs an instance of `yarp::dev::FrameTransformServer` without ROS support. This device is used by several gazebo plugins and modules within this setup as explained later.

#### gazebo
`gazebo` runs the Gazebo simulator:
  - with the simulation server `gzserver` paused;
  - with the plugin [GazeboYarpClock](http://robotology.gitlab.io/docs/gazebo-yarp-plugins/master/classgazebo_1_1GazeboYarpClock.html) required to send the __simulation clock__ over the default port  `/clock`;
  - using [models/scenario/model.sdf](models/scenario/model.sdf) as world; 
  - using `$ROBOT_CODE/visual-tactile-localization-simulation` as working directory
  - with a dependency on the port `/transformServer/transforms:o` opened by `yarpdev` (timeout=5.0s) required because plugin `GazeboYarpModelPosePublisher` uses it to publish transforms (e.g. absolute pose of the object, absolute pose of the iCub root frame attached on the waist), plugin `FakePointCloud` uses it to get the absolute pose of the object and plugin `EstimateViewer` uses it to get the estimated pose published by the module `upf-localizer`. Dependency allow plugins to find the port open when they start;
- with environment variables
  ```
  YARP_CLOCK=/clock
  ```
  required since some plugins uses time internally and need to be synchronized with the simulation clock available on port `/clock`;
  ```
  GAZEBO_MODEL_PATH=$GAZEBO_MODEL_PATH:$ROBOT_CODE/icub-gazebo:$ROBOT_CODE/visual-tactile-localization-simulation/models
  ```
  required to expose models provided within this package to Gazebo
  ```
  GAZEBO_PLUGIN_PATH=$GAZEBO_PLUGIN_PATH:$ROBOT_INSTALL/lib
  ```
  required to expose plugins provided by `gazebo-yarp-plugins` to Gazebo
  
#### upf-localizer
`upf-localizer` runs a RFModule that interfaces with the UPF:
  - with the context `simVisualTactileLocalization`
  - with a dependency on the port `/transformServer/transforms:o` opened by `yarpdev` (timeout=5.0s) required because the module uses it to publish the estimated pose;
  - with a dependency on the port `/clock` opened by module `gazebo` due to the plugin `GazeboYarpClock`;
- with environment variables
  ```
  YARP_CLOCK=/clock
  ```
  required to synchronize time with the simulation clock available on port `\clock`.
  
#### yarprobotinterface
`yarprobotinterface` starts all the devices required by the robot:
 - with the context `simCartesianControl`;
 - with the option `--config no_legs.xml` since only `torso` and `arms` are required;
 - with a dependency on the ports `/icubSim/torso/state:o`, `/icubSim/left_arm/state:o`, `/icubSim/right_arm/state:o` and `\clock`;
 - with environment variables
  ```
  YARP_CLOCK=/clock
  ```
  required to synchronize time with the simulation clock available on port `\clock`.
  
#### iKinCartesianSolver
`iKinCartesianSolver` starts the inverse kinematics online solver:
 - with the context `simCartesianControl`;
 - with the option `--part right_arm` since for now only the right arm is used;
  - with a dependency on the ports `/icubSim/torso/state:o`, `/icubSim/right_arm/state:o` and `\clock`;
 - with environment variables
  ```
  YARP_CLOCK=/clock
  ```
  required to synchronize time with the simulation clock available on port `\clock`.
  
#### Connections
Connections provided within the application description are:
- from `/mustard/fakepointcloud:o` to `/mustard/fakepointcloud_viewer:i` where `/mustard/fakepointcloud:o` is opened by the plugin `FakePointCloud` and `/mustard/fakepointcloud_viewer:i` is opened by the plugin `FakePointCloudViewer`.

### Add another object to the simulation[WIP]
To be done

### Run the simulation[WIP]
1. Run `yarpserver`
2. Run `yarpmanager`
3. Run all the application `VisualTactileLocalizationSim`
4. Connect all the ports
5. ...

To stop the simulation use `Stop all` within yarpmanager and disconnect all the ports. In case Gazebo does not close type
```
killall gzserver
killall gzclient
```

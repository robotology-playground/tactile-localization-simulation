# visual-tactile-localization-simulation
Gazebo based simulation scenario for a visual-tactile localization algorithm.

[![Visual Tactile Localization Example](http://img.youtube.com/vi/K3bbX8e4m1U/hqdefault.jpg)](https://youtu.be/K3bbX8e4m1U)

## Requirements
- [YARP](http://www.yarp.it/)
- [gazebo-yarp-plugins (forked)](https://github.com/xEnVrE/gazebo-yarp-plugins/tree/visual_tactile_loc_plugins) including:
  - `GazeboYarpModelPosePublisher` (publishing of ground truth position of the object)
  - `FakePointCloud` (fake point cloud generation and visualization on Gazebo)
  - `EstimateViewer` (estimate visualization on Gazebo)
  - `GazeboYarpSkin` (tactile sensing on the finger tips of iCub)
  - `GazeboYarpControlBoard` (joint velocity control reimplemented, [wrong joint limits of fingers](https://github.com/robotology/gazebo-yarp-plugins/issues/348) fixed and wrong implementation of `yarp::dev::RemoteControlBoard::checkMotionDone` fixed)
  - `GazeboYarpModelReset` (allows to reset the initial position of the object, useful to repeat experiments)
- [Gazebo](http://gazebosim.org/) (Gazebo >= 8 required)
- [icub-gazebo (forked)](https://github.com/xEnVrE/icub-gazebo)
- [visual-tactile-localization](https://github.com/robotology-playground/visual-tactile-localization)

### VCG
The plugin `FakePointCloud` uses the header-only library [VCG](http://vcg.isti.cnr.it/vcglib/) to sample point clouds. It is provided within the header files of the plugin.

## Configure the environment
It is supposed that you have already installed `yarp` using two directories one for code, i.e. `$ROBOT_CODE`, and one for installed stuff, i.e. `$ROBOT-INSTALL` (this is not strictly required but it helps in setting the environment variables required for Gazebo within yarpmanager). Also it is supposed that `Gazebo 8` has already been installed.

### Get the code
```
cd $ROBOT_CODE
git clone https://github.com/robotology-playground/visual-tactile-localization.git
git clone https://github.com/robotology-playground/visual-tactile-localization-simulation.git
git clone https://github.com/xEnVrE/gazebo-yarp-plugins.git
git clone https://github.com/robotology/icub-gazebo.git
```
### Install visual-tactile-localization
```
cd $ROBOT_CODE/visual-tactile-localization
git checkout master
mkdir build && cd build
cmake ../ -DCMAKE_INSTALL_PREFIX=$ROBOT_INSTALL
make install
```
This package provides a module `upf-localizer` and a context `simVisualTactileLocalization` containg configuration file for the localizer as well as `.OFF` mesh files of the models.

### Install visual-tactile-localization-simulation
```
cd $ROBOT_CODE/visual-tactile-localization-simulation
mkdir build && cd build
cmake ../ -DCMAKE_INSTALL_PREFIX=$ROBOT_INSTALL
make install
```
This package provides a module `visual-tactile-localization-sim` and two applications description `xml`s in ICUBcontrib:
- `visual-tactile-sim_system.xml` to launch the entire simulation setup; 
- `visual-tactile-sim_app.xml` to launch the module `visual-tactile-localization-sim` once the setup is online;

### Install gazebo-yarp-plugins
```
cd $ROBOT_CODE/gazebo-yarp-plugins
git checkout visual_tactile_loc_plugins
mkdir build && cd build
cmake ../ -DCMAKE_INSTALL_PREFIX=$ROBOT_INSTALL
make install
```

### Install icub-gazebo
```
cd $ROBOT_CODE/icub-gazebo
git checkout visual_tactile_loc
```
Cloning and checkout are sufficient. This forked version is equipped with Gazebo contact sensors placed at finger tips and cameras disabled (not required)

## Run the simulation
1. Run `yarpserver`
2. Run `yarpmanager`
3. `Run all` the application `VisualTactileLocalizationSim`
4. `Run all` the application `VisualTactileLocalizationSimApplication`
4. Connect all the ports

The module `visual-tactile-localization-sim` opens a RPC port `/service` where the following commands can be issued

- `home-right` restore the right arm in the starting configuration;
- `localize` perform localization using visual information;
- `approach-with-right` perform an approaching phase consisting in 
   - moving the right hand near a box taking into account the current estimate;
   - closing the fingers of the right-hand until contacts are detected between the fingers and the border of the box;
- `approach-corner-with-right` perform an approaching phase consisting in 
   - moving the right hand near the right corner of a box taking into account the current estimate;
   - closing the fingers of the right-hand until contacts are detected between the fingers and the border of the box;
- `push-with-right` perform a pushing phase. The robot tries to push the box towards himself while estimating its pose using tactile data. During this phase when contact is lost fingers are moved in order to recover it.
- `rotate-with-right` perform a rotation phase. The robot tries to rotate the box pushing on the corner of the box while estimating its pose using tactile data. During this phase when contact is lost fingers are moved in order to recover it.
- `quit` stop the module.

A transparent mesh, generated by the plugin `EstimateViewer`, is superimposed on the mesh of the object to be localized and show the current estimate produced by the UPF filter.

## How to stop the simulation
Since most of the modules in the system uses `/clock` as internal clock it is important to stop them before stopping the module `gazebo`.

Also in case Gazebo does not close type
```
killall gzserver
killall gzclient
```

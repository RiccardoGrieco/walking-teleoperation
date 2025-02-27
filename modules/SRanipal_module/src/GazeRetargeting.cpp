/**
 * @file GazeRetargeting.cpp
 * @authors Stefano Dafarra <stefano.dafarra@iit.it>
 * @copyright 2021 iCub Facility - Istituto Italiano di Tecnologia
 *            Released under the terms of the LGPLv2.1 or later, see LGPL.TXT
 * @date 2021
 */

#ifndef _USE_MATH_DEFINES //for using M_PI
#define _USE_MATH_DEFINES
#endif

#include <GazeRetargeting.hpp>
#include <yarp/os/LogStream.h>
#include <yarp/dev/IAxisInfo.h>
#include <yarp/dev/IControlLimits.h>
#include <yarp/os/Vocab.h>
#include <iDynTree/Core/Utils.h>
#include <Eigen/Dense>
#include <iDynTree/Core/EigenHelpers.h>
#include <yarp/os/Time.h>
#include <cmath>
#include <algorithm>


void GazeRetargeting::setRobotEyeControlMode(int controlMode)
{
    if (m_eyesMode)
    {
        int eyeAxis[] = {m_eyeTiltIndex, m_eyeVersIndex, m_eyeVergIndex};
        int controlModes[] = {controlMode, controlMode, controlMode};
        m_eyesMode->setControlModes(3, eyeAxis, controlModes);
    }
}

bool GazeRetargeting::homeRobotEyes()
{
    if (!m_eyesPos)
    {
        return false;
    }

    if (!updateRobotEyeEncoders())
    {
        return false;
    }

    yInfo() << "[GazeRetargeting::homeRobotEyes] Homing robot eyes..";

    int eyeAxis[] = {m_eyeTiltIndex, m_eyeVersIndex, m_eyeVergIndex};
    double speeds[] = {m_maxEyeSpeedInDegS, m_maxEyeSpeedInDegS, m_maxEyeSpeedInDegS};
    double references[] = {0.0, 0.0, 0.0};

    double maxError = std::max({m_eyeTiltInRad, m_eyeVersInRad, m_eyeVergInRad});
    double expectedTime = maxError / iDynTree::deg2rad(m_maxEyeSpeedInDegS);


    m_eyesPos->setRefSpeeds(3, eyeAxis, speeds);

    if (!m_eyesPos->positionMove(3, eyeAxis, references))
    {
        return false;
    }

    yarp::os::Time::delay(3.0 * expectedTime); //Just give some time for it to go to home

    yInfo() << "[GazeRetargeting::homeRobotEyes] Robot eyes homed!";

    return true;
}

bool GazeRetargeting::updateRobotEyeEncoders()
{
    if (!m_eyesEnc || !m_eyesEnc->getEncoders(m_encodersInDeg.data()))
    {
        return false;
    }

    m_eyeVersInRad = iDynTree::deg2rad(m_encodersInDeg[m_eyeVersIndex]);
    m_eyeVergInRad = iDynTree::deg2rad(m_encodersInDeg[m_eyeVergIndex]);
    m_eyeTiltInRad = iDynTree::deg2rad(m_encodersInDeg[m_eyeTiltIndex]);

    return true;
}

bool GazeRetargeting::setDesiredRobotEyeVelocities(double vergenceSpeedInDegS, double versionSpeedInDegS, double tiltSpeedInDegS)
{
    if (!m_eyesVel)
    {
        return false;
    }

    double velRefs[] = {tiltSpeedInDegS, versionSpeedInDegS, vergenceSpeedInDegS};
    int eyeAxis[] = {m_eyeTiltIndex, m_eyeVersIndex, m_eyeVergIndex};

    return m_eyesVel->velocityMove(3, eyeAxis, velRefs);
}

double GazeRetargeting::saturateRobotEyeVelocity(double inputVelocity, double inputPosition, double maxVelocity, double jointLowerBound, double jointUpperBound)
{
    //See https://github.com/ami-iit/element_qp-reactive-control/issues/51
    double velocityLowerLimit = std::tanh(m_tanhGain * (inputPosition - jointLowerBound)) * (-maxVelocity);
    double velocityUpperLimit = std::tanh(m_tanhGain * (jointUpperBound - inputPosition)) * maxVelocity;

    return std::max(velocityLowerLimit, std::min(inputVelocity, velocityUpperLimit));
}

GazeRetargeting::~GazeRetargeting()
{
    close();
}

bool GazeRetargeting::configure(yarp::os::ResourceFinder &rf)
{
    if (m_configured)
    {
        yError() << "[GazeRetargeting::configure] The gaze retargeting is already configured.";
        return false;
    }

    std::string name = rf.check("name", yarp::os::Value("SRanipalModule"), "The name of the module.").asString();
    std::string robot = rf.check("robot", yarp::os::Value("icub"), "The name of the robot to connect to.").asString();

    std::string eyes_version_name =  rf.check("eyesVersionName", yarp::os::Value("eyes_vers")).asString();
    std::string eyes_vergence_name = rf.check("eyesVergenceName", yarp::os::Value("eyes_verg")).asString();
    std::string eyes_tilt_name = rf.check("eyesTiltName", yarp::os::Value("eyes_tilt")).asString();

    m_maxEyeSpeedInDegS = rf.check("eyeMaxVelocity", yarp::os::Value(20.0)).asFloat64();
    double userMaxVergInDeg = rf.check("eyeMaxVergence", yarp::os::Value(10.0)).asFloat64();
    double userMaxVersInDeg = rf.check("eyeMaxVersion", yarp::os::Value(25.0)).asFloat64();
    double userMaxTiltInDeg = rf.check("eyeMaxTilt", yarp::os::Value(30.0)).asFloat64();
    m_tanhGain = rf.check("eyeKinematicSaturationGain", yarp::os::Value(10.0)).asFloat64();
    std::string headControlBoard = rf.check("headControlBoardName", yarp::os::Value("head")).asString();

    yarp::os::Property rcb_head_conf{
        {"device", yarp::os::Value("remote_controlboard")},
        {"local", yarp::os::Value("/" + name + "/head/remoteControlBoard")},
        {"remote", yarp::os::Value("/" + robot + "/" + headControlBoard)},
        {"part", yarp::os::Value(headControlBoard)}};

    if (!m_eyesDriver.open(rcb_head_conf))
    {
        yError() << "[GazeRetargeting::configure] Failed to open the head control board. Use noGaze to avoid connecting to it.";
        return false;
    }

    yarp::dev::IAxisInfo* axisInfo{nullptr};
    yarp::dev::IControlLimits* controlLimits{nullptr};

    if (!m_eyesDriver.view(axisInfo) || !axisInfo)
    {
        yError() << "[GazeRetargeting::configure] Failed to view the IAxisInfo interface. Use noGaze to avoid connecting to it.";
        return false;
    }

    if (!m_eyesDriver.view(controlLimits) || !controlLimits)
    {
        yError() << "[GazeRetargeting::configure] Failed to view the IControlLimits interface. Use noGaze to avoid connecting to it.";
        return false;
    }

    if (!m_eyesDriver.view(m_eyesPos) || !m_eyesPos)
    {
        yError() << "[GazeRetargeting::configure] Failed to view the IPositionControl interface. Use noGaze to avoid connecting to it.";
        return false;
    }

    if (!m_eyesDriver.view(m_eyesVel) || !m_eyesVel)
    {
        yError() << "[GazeRetargeting::configure] Failed to view the IVelocityControl interface. Use noGaze to avoid connecting to it.";
        return false;
    }

    if (!m_eyesDriver.view(m_eyesEnc) || !m_eyesEnc)
    {
        yError() << "[GazeRetargeting::configure] Failed to view the IEncoders interface. Use noGaze to avoid connecting to it.";
        return false;
    }

    if (!m_eyesDriver.view(m_eyesMode) || !m_eyesMode)
    {
        yError() << "[GazeRetargeting::configure] Failed to view the IControlMode interface. Use noGaze to avoid connecting to it.";
        return false;
    }

    int nAxes = 0;
    if (!m_eyesVel->getAxes(&nAxes))
    {
        yError() << "[GazeRetargeting::configure] Failed to get the number of axes. Use noGaze to avoid connecting to it.";
        return false;
    }

    m_encodersInDeg.resize(nAxes);

    m_eyeTiltIndex = -1;
    m_eyeVergIndex = -1;
    m_eyeVersIndex = -1;

    for (size_t i = 0; i < nAxes; ++i)
    {
        std::string axisName;
        if (!axisInfo->getAxisName(i, axisName))
        {
            yError() << "[GazeRetargeting::configure] Failed to get the axis name of the neck joint with index" << i <<"." ;
            return false;
        }

        if (axisName.find(eyes_version_name) != std::string::npos)
        {
            m_eyeVersIndex = i;
        }
        else if (axisName.find(eyes_vergence_name) != std::string::npos)
        {
            m_eyeVergIndex = i;
        }
        else if (axisName.find(eyes_tilt_name) != std::string::npos)
        {
            m_eyeTiltIndex = i;
        }
    }

    if (m_eyeVersIndex < 0)
    {
        yError() << "[GazeRetargeting::configure] Failed to find the version joint with the name" << eyes_version_name << "among the neck joints." ;
        return false;
    }
    if (m_eyeVergIndex < 0)
    {
        yError() << "[GazeRetargeting::configure] Failed to find the vergence joint with the name" << eyes_vergence_name << "among the neck joints." ;
        return false;
    }
    if (m_eyeTiltIndex < 0)
    {
        yError() << "[GazeRetargeting::configure] Failed to find the tilt joint with the name" << eyes_tilt_name << "among the neck joints." ;
        return false;
    }

    m_eyesVel->setRefAcceleration(m_eyeVersIndex, std::numeric_limits<double>::max());
    m_eyesVel->setRefAcceleration(m_eyeVergIndex, std::numeric_limits<double>::max());
    m_eyesVel->setRefAcceleration(m_eyeTiltIndex, std::numeric_limits<double>::max());

    double robotMinVergInDeg, robotMaxVergInDeg,
            robotMinVersInDeg, robotMaxVersInDeg,
            robotMinTiltInDeg, robotMaxTiltInDeg;

    if (!controlLimits->getLimits(m_eyeVersIndex, &robotMinVersInDeg, &robotMaxVersInDeg))
    {
        yError() << "[GazeRetargeting::configure] Failed to get the control limits of the eyes version.";
        return false;
    }
    if (!controlLimits->getLimits(m_eyeVergIndex, &robotMinVergInDeg, &robotMaxVergInDeg))
    {
        yError() << "[GazeRetargeting::configure] Failed to get the control limits of the eyes vergence.";
        return false;
    }
    if (!controlLimits->getLimits(m_eyeTiltIndex, &robotMinTiltInDeg, &robotMaxTiltInDeg))
    {
        yError() << "[GazeRetargeting::configure] Failed to get the control limits of the eyes tilt.";
        return false;
    }

    m_maxVergInDeg = std::min(userMaxVergInDeg, robotMaxVergInDeg); //The min vergence is supposed to be 0.0
    m_maxVersInDeg = std::min({userMaxVersInDeg, std::abs(robotMinVersInDeg), std::abs(robotMaxVersInDeg)});
    m_maxTiltInDeg = std::min({userMaxTiltInDeg, std::abs(robotMinTiltInDeg), std::abs(robotMaxTiltInDeg)});

    if (!m_VRInterface.configure(rf))
    {
        return false;
    }

    setRobotEyeControlMode(VOCAB_CM_POSITION);

    homeRobotEyes();

    setRobotEyeControlMode(VOCAB_CM_VELOCITY);

    m_configured = true;

    return true;
}

void GazeRetargeting::setOperatorEyeGazeAxes(const iDynTree::Axis &leftGaze, const iDynTree::Axis &rightGaze)
{
    m_leftGazeOperator.setDirection(leftGaze.getDirection());
    m_leftGazeOperator.setOrigin(leftGaze.getOrigin());
    m_rightGazeOperator.setDirection(rightGaze.getDirection());
    m_rightGazeOperator.setOrigin(rightGaze.getOrigin());
    m_gazeSet = true;
}

bool GazeRetargeting::update()
{
    if (!m_configured)
    {
        yError() << "[GazeRetargeting::update] The gaze retargeting module is not initialized.";
        return false;
    }

    if (!m_VRInterface.isActive())
    {
        return true; //do nothing
    }

    //Get the current eye encoder values
    if (!updateRobotEyeEncoders())
    {
        yError() << "[GazeRetargeting::update] Failed to get eye encoders.";
        return false;
    }

    double vergenceSpeedInRadS = 0.0, versionSpeedInRadS = 0.0, tiltSpeedInRadS = 0.0;

    //Compute the desired eye speed according to the user gaze
    if (m_gazeSet) //The desired gaze has been set at least once.
    {
        if (!m_VRInterface.computeDesiredRobotEyeVelocities(m_leftGazeOperator, m_rightGazeOperator, vergenceSpeedInRadS, versionSpeedInRadS, tiltSpeedInRadS))
        {
            yError() << "[GazeRetargeting::update] Failed to compute the desired eye velocity.";
            return false;
        }
    }

    double vergenceSpeedInDegS = iDynTree::rad2deg(vergenceSpeedInRadS);
    double versionSpeedInDegS = iDynTree::rad2deg(versionSpeedInRadS);
    double tiltSpeedInDegS = iDynTree::rad2deg(tiltSpeedInRadS);

    //We saturate the desired eye velocities according to the limits too
    vergenceSpeedInDegS = saturateRobotEyeVelocity(vergenceSpeedInDegS, m_encodersInDeg[m_eyeVergIndex], m_maxEyeSpeedInDegS, 0.0, m_maxVergInDeg);
    versionSpeedInDegS = saturateRobotEyeVelocity(versionSpeedInDegS, m_encodersInDeg[m_eyeVersIndex], m_maxEyeSpeedInDegS, -m_maxVersInDeg, m_maxVersInDeg);
    tiltSpeedInDegS = saturateRobotEyeVelocity(tiltSpeedInDegS, m_encodersInDeg[m_eyeTiltIndex], m_maxEyeSpeedInDegS, -m_maxTiltInDeg, m_maxTiltInDeg);

    //Set the desired velocitites to the robot
    if (!setDesiredRobotEyeVelocities(vergenceSpeedInDegS, versionSpeedInDegS, tiltSpeedInDegS))
    {
        yError() << "[GazeRetargeting::update] Failed to set the desired eye velocity.";
        return false;
    }

    //Update the orientation of the images in the VR view.
    m_VRInterface.setVRImagesPose(m_eyeVergInRad, m_eyeVersInRad, m_eyeTiltInRad);

    return true;
}

void GazeRetargeting::close()
{
    m_VRInterface.close();
    setRobotEyeControlMode(VOCAB_CM_POSITION);
    homeRobotEyes();
    m_eyesDriver.close();
    m_eyesVel = nullptr;
    m_eyesPos = nullptr;
    m_eyesMode = nullptr;
    m_eyesEnc = nullptr;
    m_configured = false;
}

bool GazeRetargeting::VRInterface::getValueFromRPC(const std::string &query, yarp::os::Value &value)
{
    yarp::os::Bottle cmd, reply;
    cmd.addString(query);
    bool okWrite = m_VRDeviceRPCOutputPort.write(cmd, reply);
    yDebug() << "[GazeRetargeting::VRInterface::getValueFromRPC] Sent the following command to RPC:"
             << query;

    if (!okWrite || reply.size() == 0)
    {
        yDebug()
            << "[GazeRetargeting::VRInterface::getValueFromRPC] Failed to get an answer. (okWrite ="
            << okWrite << "reply.size()" << reply.size();
        return false;
    }

    yDebug() << "[GazeRetargeting::VRInterface::getValueFromRPC] Received answer:" << reply.toString();

    value = reply.get(0);

    return true;
}

bool GazeRetargeting::VRInterface::getValueFromRPC(const std::string &query, bool &value)
{
    yarp::os::Value output;
    if (!getValueFromRPC(query, output))
    {
        return false;
    }

    if (output.isVocab32())
    {
        value = yarp::os::Vocab32::decode(output.asVocab32()).find("ok") != std::string::npos;
    }
    else if (output.isBool())
    {
        value = output.asBool();
    }
    else if (output.isString())
    {
        value = output.asString().find("ok") != std::string::npos;
    }
    else
    {
        return false;
    }

    return true;
}

bool GazeRetargeting::VRInterface::getValueFromRPC(const std::string &query, double &value)
{
    yarp::os::Value output;
    if (!getValueFromRPC(query, output) || !output.isFloat64())
    {
        return false;
    }

    value = output.asFloat64();

    return true;
}

bool GazeRetargeting::VRInterface::getValueFromRPC(const std::string &query, std::string &value)
{
    yarp::os::Value output;
    if (!getValueFromRPC(query, output) || !output.isString())
    {
        return false;
    }

    value = output.asString();

    return true;
}

iDynTree::Vector2 GazeRetargeting::VRInterface::applyDeadzone(const iDynTree::Vector2 &input)
{
    iDynTree::Vector2 output;
    output.zero();

    Eigen::Map<const Eigen::Vector2d> map = iDynTree::toEigen(input);
    Eigen::Map<Eigen::Vector2d> outputMap = iDynTree::toEigen(output);

    double inputNorm = map.norm();
    bool deadzoneNotActiveAndErrorStillHigh = !m_deadzoneActive && inputNorm > m_errorDeadzone; //True if the deadzone is not active, and the error is too high to activate it
    bool deadzoneActiveButErrorVeryHigh = m_deadzoneActive && inputNorm > m_errorDeadzoneActivation; //True if the deadzone is active, but the error is bigger than the threshold to deactivate it
    bool timeCheckNotActive = m_deadzoneMinActivationTimeInS <= 0.0; //if the parameter m_deadzoneMinActivationTimeInS is lower or equal than zero it means that we should not consider the time check when deactivating the deadzone
    bool timeCheckPerformedOnce = m_deadzoneActivationTime >= 0.0; //m_deadzoneActivationTime contains the first time instant in which deadzoneActiveButErrorVeryHigh becomes true. If it is greater or equal than zero, it means that it has been set already.
    bool enoughTimePassed =  timeCheckNotActive || (timeCheckPerformedOnce && ((yarp::os::Time::now() - m_deadzoneActivationTime) >= m_deadzoneMinActivationTimeInS)); //enoughTimePassed is true either if we don't use any time threshold, or when the time condition has triggered.

    if ((deadzoneActiveButErrorVeryHigh && enoughTimePassed) || deadzoneNotActiveAndErrorStillHigh)
    {
        m_deadzoneActive = false;
        m_deadzoneActivationTime = -1.0;
        outputMap = (1.0 - m_errorDeadzone / inputNorm) * map;
        return output;
    }

    m_deadzoneActive = true;

    if (deadzoneActiveButErrorVeryHigh && !timeCheckPerformedOnce) //It is the the first time that the gaze exits the deadzone
    {
        m_deadzoneActivationTime = yarp::os::Time::now();
    }

    if (!deadzoneActiveButErrorVeryHigh) //The gaze is still in the deadzone
    {
        m_deadzoneActivationTime = -1.0;
    }

    return output;
}

double GazeRetargeting::VRInterface::applyQuantization(double input, double quantization)
{
    int level = static_cast<int>(std::round(input / quantization));
    return level * quantization;
}

bool GazeRetargeting::VRInterface::configure(yarp::os::ResourceFinder &rf)
{
    m_name = rf.check("name", yarp::os::Value("SRanipalModule"), "The name of the module.").asString();
    std::string VRDeviceRPCOutputPort = rf.check("VRDeviceRPCOutputPortName", yarp::os::Value("/VR/rpc:o")).asString();

    if (!m_VRDeviceRPCOutputPort.open("/" + m_name + VRDeviceRPCOutputPort))
    {
        yError() << "[GazeRetargeting::configure] Failed to open /" + m_name + VRDeviceRPCOutputPort + " port.";
        return false;
    }

    m_velocityGain = rf.check("gazeVelocityGain", yarp::os::Value(2.0)).asFloat64();
    m_errorDeadzone = rf.check("gazeDeadzone", yarp::os::Value(0.02)).asFloat64();
    double activationOffset = rf.check("gazeDeadzoneActivationOffset", yarp::os::Value(0.1)).asFloat64();

    if (activationOffset < 0)
    {
        yError() << "[GazeRetargeting::configure] gazeDeadzoneActivationOffset is supposed to be non-negative.";
        return false;
    }

    m_errorDeadzoneActivation = m_errorDeadzone + activationOffset;

    m_deadzoneMinActivationTimeInS = rf.check("gazeDeadzoneMinActivationTime", yarp::os::Value(0.5)).asFloat64();

    double gazeAccuracyInDeg = rf.check("gazeMovementAccuracyInDeg", yarp::os::Value(0.1)).asFloat64();

    if (gazeAccuracyInDeg <= 0.0)
    {
        yError() << "[GazeRetargeting::configure] gazeAccuracyInDeg is supposed to be strictly positive.";
        return false;
    }

    m_eyeMovementAccuracyInRad = iDynTree::deg2rad(gazeAccuracyInDeg);

    return true;
}

void GazeRetargeting::VRInterface::setVRImagesPose(double vergenceInRad, double versionInRad, double tiltInRad)
{
    m_leftEye.elevation = tiltInRad;
    m_rightEye.elevation = tiltInRad;

    // See https://icub-tech-iit.github.io/documentation/icub_kinematics/icub-vergence-version/icub-vergence-version/#converting-vergenceversion-to-decoupled-lr
    // In the documentation above, the angle is positive clockwise, while the angles we send are positive anticlockwise
    m_leftEye.azimuth = -(versionInRad + vergenceInRad/2.0);
    m_rightEye.azimuth = -(versionInRad - vergenceInRad/2.0);

    m_leftEye.elevation  = applyQuantization(m_leftEye.elevation,  m_eyeMovementAccuracyInRad);
    m_leftEye.azimuth    = applyQuantization(m_leftEye.azimuth,    m_eyeMovementAccuracyInRad);
    m_rightEye.elevation = applyQuantization(m_rightEye.elevation, m_eyeMovementAccuracyInRad);
    m_rightEye.azimuth   = applyQuantization(m_rightEye.azimuth,   m_eyeMovementAccuracyInRad);

    m_leftEye.sendAngles();
    m_rightEye.sendAngles();

}

bool GazeRetargeting::VRInterface::computeDesiredRobotEyeVelocities(const iDynTree::Axis &operatorLeftEyeGaze, const iDynTree::Axis &operatorRightEyeGaze,
                                                               double &vergenceSpeedInRadS, double &versionSpeedInRadS, double &tiltSpeedInRadS)
{
    //compute the intersection between the gaze ray and the xy plane of the image
    iDynTree::Vector2 leftImageIntersection, rightImageIntersection;
    bool okL = m_leftEye.intersectionInImage(operatorLeftEyeGaze, leftImageIntersection);
    bool okR = m_rightEye.intersectionInImage(operatorRightEyeGaze, rightImageIntersection);
    if (!okL || !okR)
    {
        yError() << "[GazeRetargeting::VRInterface::computeDesiredRobotEyeVelocities] Failed to compute the intersection between the gaze and the images.";
        return false;
    }

    //First, apply a deadzone on the intersection with the image to avoid unwanted motions
    leftImageIntersection = applyDeadzone(leftImageIntersection);
    rightImageIntersection = applyDeadzone(rightImageIntersection);

    double leftEyeDistance = iDynTree::toEigen(m_leftEye.imageRelativePosition).norm();
    double rightEyeDistance = iDynTree::toEigen(m_rightEye.imageRelativePosition).norm();
    //Compute the desired single eye velocity
    double leftElevationVelocity, rightElevationVelocity, leftAzimuthVelocity, rightAzimuthVelocity;
    leftElevationVelocity = m_velocityGain * leftImageIntersection(1) / leftEyeDistance; //The Y axis is pointing upward. If the operator is intersecting the image above the center, moves the eye up
    rightElevationVelocity = m_velocityGain * rightImageIntersection(1) / rightEyeDistance;

    leftAzimuthVelocity = -m_velocityGain * leftImageIntersection(0) / leftEyeDistance; //The X axis is pointing to the right. If the operator is looking on the right of the center, moves the eye clockwise (hence the minus sign).
    rightAzimuthVelocity = -m_velocityGain * rightImageIntersection(0) / rightEyeDistance;

    //Compute the dual eye velocity
    tiltSpeedInRadS = 0.5 * (leftElevationVelocity + rightElevationVelocity); //Ideally they should be equal. Set the desired velocity to the average value.
    //Compute the version and vergence velocity according to https://icub-tech-iit.github.io/documentation/icub_kinematics/icub-vergence-version/icub-vergence-version
    double leftVersVelocity = -leftAzimuthVelocity; //According to https://icub-tech-iit.github.io/documentation/icub_kinematics/icub-vergence-version/icub-vergence-version, positive version is clockwise, while positive azimuth is anticlockwise
    double rightVersVelocity = -rightAzimuthVelocity;
    versionSpeedInRadS = 0.5 * (leftVersVelocity + rightVersVelocity);
    vergenceSpeedInRadS = leftVersVelocity - rightVersVelocity;

    return true;
}

bool GazeRetargeting::VRInterface::isActive()
{
    if (m_isActive)
    {
        return true;
    }

    if ((yarp::os::Time::now() - m_lastActiveCheck) < 1.0) //Check once per second
    {
        return false;
    }
    m_lastActiveCheck = yarp::os::Time::now();

    if (m_VRDeviceRPCOutputPort.getOutputCount() == 0)
    {
        yInfo() << "[GazeRetargeting::VRInterface::isActive] The RPC port has not been connected yet to the VR device...";
        return false;
    }

    bool leftEyeActive{false}, rightEyeActive{false};

    if (!getValueFromRPC("isLeftEyeActive", leftEyeActive) || !leftEyeActive)
    {
        yInfo() << "[GazeRetargeting::VRInterface::isActive] The left eye is still not active...";
        return false;
    }

    if (!getValueFromRPC("isRightEyeActive", rightEyeActive) || !rightEyeActive)
    {
        yInfo() << "[GazeRetargeting::VRInterface::isActive] The right eye is still not active...";
        return false;
    }

    double interCameraDistance = 0.0;

    if (!getValueFromRPC("getInterCameraDistance", interCameraDistance))
    {
        yError() << "[GazeRetargeting::VRInterface::isActive] Failed to retrieve the inter camera distance from the VR device.";
        return false;
    }

    m_leftEye.eyePosition.zero();
    m_leftEye.eyePosition(0) = -interCameraDistance/2.0;
    m_rightEye.eyePosition.zero();
    m_rightEye.eyePosition(0) = interCameraDistance/2.0;

    double eyesZPosition = -1.0;

    if (!getValueFromRPC("getEyesZPosition", eyesZPosition))
    {
        yError() << "[GazeRetargeting::VRInterface::isActive] Failed to retrieve the eyes Z position from the VR device.";
        return false;
    }

    m_leftEye.imageRelativePosition.zero();
    m_leftEye.imageRelativePosition(2) = eyesZPosition;
    m_rightEye.imageRelativePosition.zero();
    m_rightEye.imageRelativePosition(2) = eyesZPosition;

    std::string leftEyePortName;

    if (!getValueFromRPC("getLeftImageControlPortName", leftEyePortName))
    {
        yError() << "[GazeRetargeting::VRInterface::isActive] Failed to retrieve the left image control port name from the VR device.";
        return false;
    }

    std::string leftEyeOutputPortName = "/" + m_name + "/leftEye/control:o";

    if (!m_leftEye.imageControlPort.open(leftEyeOutputPortName))
    {
        yError() << "[GazeRetargeting::VRInterface::isActive] Failed to open the port" << leftEyeOutputPortName;
        return false;
    }

    if (!yarp::os::Network::connect(leftEyeOutputPortName, leftEyePortName))
    {
        yError() << "[GazeRetargeting::VRInterface::isActive] Failed to connect the port" << leftEyeOutputPortName << "to" << leftEyePortName;
        return false;
    }

    std::string rightEyePortName;


    if (!getValueFromRPC("getRightImageControlPortName", rightEyePortName))
    {
        yError() << "[GazeRetargeting::VRInterface::isActive] Failed to retrieve the right image control port name from the VR device.";
        return false;
    }

    std::string rightEyeOutputPortName = "/" + m_name + "/rightEye/control:o";

    if (!m_rightEye.imageControlPort.open(rightEyeOutputPortName))
    {
        yError() << "[GazeRetargeting::VRInterface::isActive] Failed to open the port" << rightEyeOutputPortName;
        return false;
    }

    if (!yarp::os::Network::connect(rightEyeOutputPortName, rightEyePortName))
    {
        yError() << "[GazeRetargeting::VRInterface::isActive] Failed to connect the port" << rightEyeOutputPortName << "to" << rightEyePortName;
        return false;
    }

    setVRImagesPose(0.0, 0.0, 0.0);

    m_isActive = true;

    yInfo() << "[GazeRetargeting::VRInterface::isActive] Gaze retargeting ready!";

    return true;;
}

void GazeRetargeting::VRInterface::close()
{
    m_VRDeviceRPCOutputPort.close();
    m_leftEye.close();
    m_rightEye.close();
}

void GazeRetargeting::VRInterface::EyeControl::sendAngles()
{
    yarp::sig::Vector& output = imageControlPort.prepare();

    output.resize(2);
    output(0) = azimuth;
    output(1) = elevation;

    imageControlPort.write();
}

iDynTree::Transform GazeRetargeting::VRInterface::EyeControl::currentImageTransform()
{
    //The X axis is pointing to the right in the VIEW space, the Y axis is pointing upwards in the VIEW space
    iDynTree::Rotation rotation = iDynTree::Rotation::RotX(elevation) * iDynTree::Rotation::RotY(azimuth);
    iDynTree::Position position = rotation * imageRelativePosition + eyePosition;

    return iDynTree::Transform(rotation, position);
}

bool GazeRetargeting::VRInterface::EyeControl::intersectionInImage(const iDynTree::Axis &operatorGazeInSRanipalFrame, iDynTree::Vector2& output)
{
    output.zero();

    iDynTree::Transform headsetToSranipalTransform(iDynTree::Rotation::RotY(M_PI), //The frame in which gazeInSRanipalFrame has the Y pointing up and the Z forward. The headset frame has the Z backward
                                                   iDynTree::Position::Zero());    //See https://www.researchgate.net/figure/Coordinate-system-of-HTC-VIVE-Pro-Eye-based-on-the-manual-of-SRanipal-SDK-A_fig1_346058398

    iDynTree::Axis gazeInHeadsetFrame = headsetToSranipalTransform * operatorGazeInSRanipalFrame;

    iDynTree::Axis gazeInImage
        = currentImageTransform().inverse() * gazeInHeadsetFrame;

    const iDynTree::Position& origin = gazeInImage.getOrigin();
    const iDynTree::Direction& direction = gazeInImage.getDirection();

    if (std::abs(direction(2)) < 1e-15)
    {
        return false;
    }

    double alpha = -origin(2) / direction(2);

    output(0) = origin(0) + alpha * direction(0);
    output(1) = origin(1) + alpha * direction(1);

    return true;

}

void GazeRetargeting::VRInterface::EyeControl::close()
{
    azimuth = 0.0;
    elevation = 0.0;
    sendAngles();
    imageControlPort.close();
}

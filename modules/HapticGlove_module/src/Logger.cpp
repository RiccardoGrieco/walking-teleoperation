/**
 * @file Logger.cpp
 * @authors  Kourosh Darvish <kourosh.darvish@iit.it>
 * @copyright 2021 Artificial and Mechanical Intelligence - Istituto Italiano di Tecnologia
 *            Released under the terms of the LGPLv2.1 or later, see LGPL.TXT
 * @date 2021
 */

// teleoperation
#include <Logger.hpp>
#include <Utils.hpp>

// yarp
#include <yarp/os/LogStream.h>

using namespace HapticGlove;
Teleoperation::Logger::Logger(const Teleoperation& module, const bool isRightHand)
    : m_teleoperation(module)
{
    m_isRightHand = isRightHand;
    m_handName = m_isRightHand ? "Right" : "Left";

    m_robotPrefix = "robot" + m_handName + "Hand";
    m_humanPrefix = "human" + m_handName + "Hand";

    m_logPrefix = "Logger::" + m_handName + ":: ";

    m_numRobotActuatedAxes
        = m_teleoperation.m_robotController->controlHelper()->getNumberOfActuatedAxis();
    m_numRobotActuatedJoints
        = m_teleoperation.m_robotController->controlHelper()->getNumberOfActuatedJoints();

    m_numHumanHandFingers = m_teleoperation.m_humanGlove->getNumOfFingers();
    m_numHumanHandJoints = m_teleoperation.m_humanGlove->getNumOfHandJoints();
    m_numHumanVibrotactileFeedback = m_teleoperation.m_humanGlove->getNumOfVibrotactileFeedbacks();
    m_numHumanForceFeedback = m_teleoperation.m_humanGlove->getNumOfForceFeedback();
    m_numberRobotTactileFeedbacks = m_teleoperation.m_robotSkin->getNumOfTactileFeedbacks();

    // initialize the data structure
    m_data.time = yarp::os::Time::now();
    // robot
    m_data.robotAxisReferences.resize(m_numRobotActuatedAxes, 0.0);
    m_data.robotAxisFeedbacks.resize(m_numRobotActuatedAxes, 0.0);
    m_data.robotAxisVelocityFeedbacks.resize(m_numRobotActuatedAxes, 0.0);

    m_data.robotJointReferences.resize(m_numRobotActuatedJoints, 0.0);
    m_data.robotJointFeedbacks.resize(m_numRobotActuatedJoints, 0.0);

    m_data.robotAxisValueErrors.resize(m_numRobotActuatedAxes, 0.0);
    m_data.robotAxisVelocityErrors.resize(m_numRobotActuatedAxes, 0.0);

    m_data.robotMotorCurrentReferences.resize(m_numRobotActuatedAxes, 0.0);
    m_data.robotMotorCurrentFeedbacks.resize(m_numRobotActuatedAxes, 0.0);

    m_data.robotMotorPwmReferences.resize(m_numRobotActuatedAxes, 0.0);
    m_data.robotMotorPwmFeedbacks.resize(m_numRobotActuatedAxes, 0.0);

    m_data.robotMotorPidOutputs.resize(m_numRobotActuatedAxes, 0.0);

    m_data.robotAxisValueReferencesKf.resize(m_numRobotActuatedAxes, 0.0);
    m_data.robotAxisVelocityReferencesKf.resize(m_numRobotActuatedAxes, 0.0);
    m_data.robotAxisAccelerationReferencesKf.resize(m_numRobotActuatedAxes, 0.0);
    m_data.robotAxisCovReferencesKf = Eigen::MatrixXd::Zero(m_numRobotActuatedAxes, 9);

    m_data.robotAxisValueFeedbacksKf.resize(m_numRobotActuatedAxes, 0.0);
    m_data.robotAxisVelocityFeedbacksKf.resize(m_numRobotActuatedAxes, 0.0);
    m_data.robotAxisAccelerationFeedbacksKf.resize(m_numRobotActuatedAxes, 0.0);
    m_data.robotAxisCovFeedbacksKf = Eigen::MatrixXd::Zero(m_numRobotActuatedAxes, 9);

    m_data.robotJointsExpectedKf.resize(m_numRobotActuatedJoints, 0.0);
    m_data.robotJointsFeedbackKf.resize(m_numRobotActuatedJoints, 0.0);
    // human
    m_data.humanJointValues.resize(m_numHumanHandJoints, 0.0);
    m_data.humanFingertipPoses = Eigen::MatrixXd::Zero(m_numHumanHandFingers, 7);
    m_data.humanKinestheticForceFeedbacks.resize(m_numHumanForceFeedback, 0.0);
    m_data.kinestheticVibrotactileFeedbacks.resize(m_numHumanVibrotactileFeedback, 0.0);
    m_data.humanPalmRotation.resize(4, 0.0); // 4: number of quaternions
    m_data.fingertipsTactileFeedback.resize(m_numberRobotTactileFeedbacks, 0.0);
    m_data.robotFingerSkinVibrotactileFeedbacks.resize(m_numHumanVibrotactileFeedback, 0.0);
}

Teleoperation::Logger::~Logger() = default;

bool Teleoperation::Logger::openLogger()
{
#ifdef ENABLE_LOGGER
    std::string currentTime = YarpHelper::getTimeDateMatExtension();
    m_logFileName = "HapticGloveModule_" + m_handName + "Hand_" + currentTime + "_log.mat";

    yInfo() << "log file name: " << currentTime << m_logFileName;

    m_logger = XBot::MatLogger2::MakeLogger(m_logFileName);
    m_appender = XBot::MatAppender::MakeInstance();
    m_appender->add_logger(m_logger);
    m_appender->start_flush_thread();

    // create the data structures to save
    // time
    m_logger->create("time", 1);

    // axis
    m_logger->create(m_robotPrefix + "AxisReferences", m_numRobotActuatedAxes);
    m_logger->create(m_robotPrefix + "AxisFeedbacks", m_numRobotActuatedAxes);
    m_logger->create(m_robotPrefix + "AxisVelocityFeedbacks", m_numRobotActuatedAxes);

    // robot hand joints
    m_logger->create(m_robotPrefix + "JointReferences", m_numRobotActuatedJoints);
    m_logger->create(m_robotPrefix + "JointFeedbacks", m_numRobotActuatedJoints);

    // robot axis errors
    m_logger->create(m_robotPrefix + "AxisValueErrors", m_numRobotActuatedAxes);
    m_logger->create(m_robotPrefix + "AxisVelocityErrors", m_numRobotActuatedAxes);

    // to check if it is real robot or simulation
    if (m_teleoperation.m_robot == "icub")
    {
        // current
        m_logger->create(m_robotPrefix + "MotorCurrentReferences", m_numRobotActuatedAxes);
        m_logger->create(m_robotPrefix + "MotorCurrentFeedbacks", m_numRobotActuatedAxes);

        // pwm
        m_logger->create(m_robotPrefix + "MotorPwmReferences", m_numRobotActuatedAxes);
        m_logger->create(m_robotPrefix + "MotorPwmFeedbacks", m_numRobotActuatedAxes);
    }

    // pid
    m_logger->create(m_robotPrefix + "MotorPidOutputs", m_numRobotActuatedAxes);

    // axis reference KF
    m_logger->create(m_robotPrefix + "AxisValueReferencesKf", m_numRobotActuatedAxes);
    m_logger->create(m_robotPrefix + "AxisVelocityReferencesKf", m_numRobotActuatedAxes);
    m_logger->create(m_robotPrefix + "AxisAccelerationReferencesKf", m_numRobotActuatedAxes);
    m_logger->create(m_robotPrefix + "AxisCovReferencesKf",
                     m_numRobotActuatedAxes,
                     9); // states: value, velocity, acceleration --> cov matrix size: 3X3=9

    // axis feedback KF
    m_logger->create(m_robotPrefix + "AxisValueFeedbacksKf", m_numRobotActuatedAxes);
    m_logger->create(m_robotPrefix + "AxisVelocityFeedbacksKf", m_numRobotActuatedAxes);
    m_logger->create(m_robotPrefix + "AxisAccelerationFeedbacksKf", m_numRobotActuatedAxes);
    m_logger->create(m_robotPrefix + "AxisCovFeedbacksKf",
                     m_numRobotActuatedAxes,
                     9); // states: value, velocity, acceleration --> cov matrix size: 3X3=9

    // joints KF
    m_logger->create(m_robotPrefix + "JointsExpectedKf", m_numRobotActuatedJoints);
    m_logger->create(m_robotPrefix + "JointsFeedbackKf", m_numRobotActuatedJoints);

    // Human data
    m_logger->create(m_humanPrefix + "JointValues", m_numHumanHandJoints);
    m_logger->create(m_humanPrefix + "FingertipPoses", m_numHumanHandFingers, 7);
    m_logger->create(m_humanPrefix + "ForceFeedbacks", m_numHumanForceFeedback);
    m_logger->create(m_humanPrefix + "VibrotactileFeedbacks", m_numHumanVibrotactileFeedback);
    m_logger->create(m_humanPrefix + "PalmRotation", 4);

    // skin
    m_logger->create(m_robotPrefix + "FingertipsTactileFeedbacks", m_numberRobotTactileFeedbacks);
    m_logger->create(m_humanPrefix + "VibrotactileFeedbacksUsingSkin",
                     m_numHumanVibrotactileFeedback);

    // add the robot and human fingers, axes, joints.
    std::vector<std::string> robotActuatedAxisNames;
    std::vector<std::string> robotActuatedJointNames;
    std::vector<std::string> humanJointNames;
    std::vector<std::string> humanFingerNames;

    m_teleoperation.m_robotController->controlHelper()->getActuatedAxisNames(
        robotActuatedAxisNames);
    m_teleoperation.m_robotController->controlHelper()->getActuatedJointNames(
        robotActuatedJointNames);
    m_teleoperation.m_humanGlove->getHumanHandJointsNames(humanJointNames);
    m_teleoperation.m_humanGlove->getHumanHandFingerNames(humanFingerNames);

    m_logger->create(m_robotPrefix + "ActuatedAxisNames", m_numRobotActuatedAxes);
    m_logger->create(m_robotPrefix + "ActuatedJointNames", m_numRobotActuatedJoints);
    m_logger->create(m_humanPrefix + "JointNames", m_numHumanHandJoints);
    m_logger->create(m_humanPrefix + "FingerNames", m_numHumanHandFingers);

    //    m_logger->add(m_robotPrefix + "ActuatedAxisNames", robotActuatedAxisNames);
    //    m_logger->add(m_robotPrefix + "ActuatedJointNames", robotActuatedJointNames);
    //    m_logger->add(m_humanPrefix + "JointNames", humanJointNames);
    //    m_logger->add(m_humanPrefix + "FingerNames", humanFingerNames);

    // print
    yInfo() << m_logPrefix << "logging is active.";

#else
    yInfo() << m_logPrefix << " logging option is not active in CMakeLists.";

#endif

    return true;
}

bool Teleoperation::Logger::updateData()
{
    m_data.time = yarp::os::Time::now();

    // robot
    m_teleoperation.m_robotController->getAxisValueReferences(m_data.robotAxisReferences);

    m_teleoperation.m_robotController->getAxisValueFeedbacks(m_data.robotAxisFeedbacks);

    m_teleoperation.m_robotController->getAxisVelocityFeedbacks(m_data.robotAxisVelocityFeedbacks);

    m_teleoperation.m_robotController->getJointReferences(m_data.robotJointReferences);

    m_teleoperation.m_robotController->getJointValueFeedbacks(m_data.robotJointFeedbacks);

    m_teleoperation.m_retargeting->getAxisError(m_data.robotAxisValueErrors,
                                                m_data.robotAxisVelocityErrors);

    if (m_teleoperation.m_robot == "icub")
    {
        m_teleoperation.m_robotController->getMotorCurrentReference(
            m_data.robotMotorCurrentReferences);

        m_teleoperation.m_robotController->getMotorCurrentFeedback(
            m_data.robotMotorCurrentFeedbacks);

        m_teleoperation.m_robotController->getMotorPwmReference(m_data.robotMotorPwmReferences);

        m_teleoperation.m_robotController->getMotorPwmFeedback(m_data.robotMotorPwmFeedbacks);
    }

    m_teleoperation.m_robotController->getMotorPidOutputs(m_data.robotMotorPidOutputs);

    m_teleoperation.m_robotController->getEstimatedMotorsState(
        m_data.robotAxisValueFeedbacksKf,
        m_data.robotAxisVelocityFeedbacksKf,
        m_data.robotAxisAccelerationFeedbacksKf,
        m_data.robotAxisCovFeedbacksKf,
        m_data.robotAxisValueReferencesKf,
        m_data.robotAxisVelocityReferencesKf,
        m_data.robotAxisAccelerationReferencesKf,
        m_data.robotAxisCovReferencesKf);

    m_teleoperation.m_robotController->getEstimatedJointValuesKf(m_data.robotJointsExpectedKf,
                                                                 m_data.robotJointsFeedbackKf);
    // human
    m_teleoperation.m_humanGlove->getHandJointAngles(m_data.humanJointValues);

    m_teleoperation.m_humanGlove->getFingertipPoses(m_data.humanFingertipPoses);

    m_teleoperation.m_retargeting->getForceFeedbackToHuman(m_data.humanKinestheticForceFeedbacks);

    m_teleoperation.m_retargeting->getVibrotactileFeedbackToHuman(
        m_data.kinestheticVibrotactileFeedbacks);

    m_teleoperation.m_humanGlove->getHandPalmRotation(m_data.humanPalmRotation);

    // skin
    m_teleoperation.m_robotSkin->getSerializedFingertipsTactileFeedbacks(
        m_data.fingertipsTactileFeedback);

    m_teleoperation.m_robotSkin->getVibrotactileFeedback(
        m_data.robotFingerSkinVibrotactileFeedbacks);

    return true;
}
bool Teleoperation::Logger::logData()
{
#ifdef ENABLE_LOGGER

    if (!this->updateData())
    {
        yWarning() << m_logPrefix << "cannot update the data.";
    }

    // time
    m_logger->add("time", m_data.time);

    // axis
    m_logger->add(m_robotPrefix + "AxisReferences", m_data.robotAxisReferences);
    m_logger->add(m_robotPrefix + "AxisFeedbacks", m_data.robotAxisFeedbacks);
    m_logger->add(m_robotPrefix + "AxisVelocityFeedbacks", m_data.robotAxisVelocityFeedbacks);

    // robot hand joints
    m_logger->add(m_robotPrefix + "JointReferences", m_data.robotJointReferences);
    m_logger->add(m_robotPrefix + "JointFeedbacks", m_data.robotJointFeedbacks);

    // robot axis errors
    m_logger->add(m_robotPrefix + "AxisValueErrors", m_data.robotAxisValueErrors);
    m_logger->add(m_robotPrefix + "AxisVelocityErrors", m_data.robotAxisVelocityErrors);

    // to check if it is real robot or simulation
    if (m_teleoperation.m_robot == "icub")
    {
        // current
        m_logger->add(m_robotPrefix + "MotorCurrentReferences", m_data.robotMotorCurrentReferences);
        m_logger->add(m_robotPrefix + "MotorCurrentFeedbacks", m_data.robotMotorCurrentFeedbacks);

        // pwm
        m_logger->add(m_robotPrefix + "MotorPwmReferences", m_data.robotMotorPwmReferences);
        m_logger->add(m_robotPrefix + "MotorPwmFeedbacks", m_data.robotMotorPwmFeedbacks);
    }

    // pid
    m_logger->add(m_robotPrefix + "MotorPidOutputs", m_data.robotMotorPidOutputs);

    // axis reference KF
    m_logger->add(m_robotPrefix + "AxisValueReferencesKf", m_data.robotAxisValueReferencesKf);
    m_logger->add(m_robotPrefix + "AxisVelocityReferencesKf", m_data.robotAxisVelocityReferencesKf);
    m_logger->add(m_robotPrefix + "AxisAccelerationReferencesKf",
                  m_data.robotAxisAccelerationReferencesKf);
    m_logger->add(m_robotPrefix + "AxisCovReferencesKf", m_data.robotAxisCovReferencesKf);

    // axis feedback KF
    m_logger->add(m_robotPrefix + "AxisValueFeedbacksKf", m_data.robotAxisValueFeedbacksKf);
    m_logger->add(m_robotPrefix + "AxisVelocityFeedbacksKf", m_data.robotAxisVelocityFeedbacksKf);
    m_logger->add(m_robotPrefix + "AxisAccelerationFeedbacksKf",
                  m_data.robotAxisAccelerationFeedbacksKf);
    m_logger->add(m_robotPrefix + "AxisCovFeedbacksKf", m_data.robotAxisCovFeedbacksKf);

    // joints KF
    m_logger->add(m_robotPrefix + "JointsExpectedKf", m_data.robotJointsExpectedKf);
    m_logger->add(m_robotPrefix + "JointsFeedbackKf", m_data.robotJointsFeedbackKf);

    // Human data
    m_logger->add(m_humanPrefix + "JointValues", m_data.humanJointValues);
    m_logger->add(m_humanPrefix + "FingertipPoses", m_data.humanFingertipPoses);
    m_logger->add(m_humanPrefix + "ForceFeedbacks", m_data.humanKinestheticForceFeedbacks);
    m_logger->add(m_humanPrefix + "VibrotactileFeedbacks", m_data.kinestheticVibrotactileFeedbacks);
    m_logger->add(m_humanPrefix + "PalmRotation", m_data.humanPalmRotation);

    // skin
    m_logger->add(m_robotPrefix + "FingertipsTactileFeedbacks", m_data.fingertipsTactileFeedback);
    m_logger->add(m_humanPrefix + "VibrotactileFeedbacksUsingSkin",
                  m_data.robotFingerSkinVibrotactileFeedbacks);

#endif

    return true;
}

bool Teleoperation::Logger::closeLogger()
{

#ifdef ENABLE_LOGGER
    m_logger->flush_available_data();
    //    m_logger->~MatLogger2();
    m_logger = nullptr;
#endif
    yInfo() << m_logPrefix << "logger is closing.";
    yInfo() << m_logPrefix << "log file is saved in: " << m_logFileName;
    return true;
}

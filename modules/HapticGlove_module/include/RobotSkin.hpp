/**
 * @file RobotSkin.hpp
 * @authors Kourosh Darvish <kourosh.darvish@iit.it>
 * @copyright 2021 iCub Facility - Istituto Italiano di Tecnologia
 *            Released under the terms of the LGPLv2.1 or later, see LGPL.TXT
 * @date 2021
 */

#ifndef ROBOT_SKIN_HPP
#define ROBOT_SKIN_HPP

// std
#include <algorithm> // std::min_element, std::max_element
#include <iostream>
#include <memory>

// teleoperation
#include <ControlHelper.hpp>
#include <RobotInterface.hpp>

// yarp
#include <yarp/os/Searchable.h>

namespace HapticGlove
{
class RobotSkin;
struct FingertipTactileData;
} // namespace HapticGlove

/**
 * FingertipTactileData structure useful to manage the fingertip skin data.
 */
struct HapticGlove::FingertipTactileData
{
    std::string fingerName;
    size_t indexStart;
    size_t indexEnd;
    size_t noTactileSensors;
    std::vector<double>
        rawTactileData; /// range: 0-256; value 240 shows no load, and 0 shows max load
    std::vector<double> tactileData; /// range: 0-1; 0 shows no load, and 1 shows max load
    std::vector<double> calibratedTactileData; /// range: almost 0-1: 0: no load, 1 max load

    const double maxTactileValue = 255.0;
    const double minTactileValue = 0.0;
    const double noLoadValue = 240.0;

    double contactThresholdValue = 5.0; /// default value
    double vibrotactileGain = 1.0; /// default value

    std::vector<double> biasTactileSensor; /// mean of the tactile sensors when not touched
    std::vector<double>
        stdTactileSensor; /// standard deviation (std) of the tactile sensors when not touched

    CtrlHelper::Eigen_Mat collectedTactileData; /**< The logged data to find the bias and standard
                            deviation (std) of tactile sensors;
                            - dimension <o, t>:
                            - o: number of observations (logged data),
                            - m: number of tactile sensors*/

    bool isFingerInContact = false;

    double maxTactileFeedbackValue()
    {
        return *std::max_element(calibratedTactileData.begin(), calibratedTactileData.end());
    }

    double contactThreshold()
    {
        return contactThresholdValue
               * stdTactileSensor[std::distance(
                   calibratedTactileData.begin(),
                   std::max_element(calibratedTactileData.begin(), calibratedTactileData.end()))];
    }

    void printInfo() const
    {
        std::cout << "==================" << std::endl;
        std::cout << "finger name: " << fingerName << std::endl;
        std::cout << "starting index: " << indexStart << std::endl;
        std::cout << "ending index: " << indexEnd << std::endl;
        std::cout << "number of tactile sensor: " << noTactileSensors << std::endl;
        std::cout << "max tactile threshold: " << maxTactileValue << std::endl;
        std::cout << "min tactile threshold: " << minTactileValue << std::endl;
        std::cout << "no load tactile threshold: " << noLoadValue << std::endl;
        std::cout << "contact threshold: " << contactThresholdValue << std::endl;
        std::cout << "vibrotactile gain: " << vibrotactileGain << std::endl;
        std::cout << "==================" << std::endl;
    }
};

/**
 * RobotSkin Class useful to manage the fingertip skin data.
 */
class HapticGlove::RobotSkin
{
private:
    std::string m_logPrefix;

    bool m_rightHand;
    size_t m_noFingers;
    size_t m_totalNoTactile;
    std::vector<FingertipTactileData> m_fingersTactileData;

    std::vector<bool> m_areTactileSensorsWorking;
    std::vector<bool> m_areFingersInContact;

    std::vector<double> m_fingersVibrotactileFeedback;
    std::vector<double> m_fingersContactStrength;
    double m_tactileWorkingThreshold;

    void setRawTactileFeedbacks(const std::vector<double>& rawTactileFeedbacks);

    void computeVibrotactileFeedback();

    void computeMaxContactStrength();

    void computeAreFingersInContact();

public:
    RobotSkin();
    /**
     * Configure the object.
     * @param config reference to a resource finder object.
     * @param name name of the robot
     * @return true in case of success and false otherwise.
     */
    bool
    configure(const yarp::os::Searchable& config, const std::string& name, const bool& rightHand);

    void updateTactileFeedbacks(const std::vector<double>& rawTactileFeedbacks);

    bool computeCalibrationParamters();

    bool getFingertipTactileFeedbacks(const size_t fingertipIndex, std::vector<double>& skinData);

    bool collectSkinDataForCalibration();

    bool getSerializedFingertipsTactileFeedbacks(std::vector<double>& fingertipsTactileFeedback);

    bool getFingertipMaxTactileFeedback(std::vector<double>& fingertipPressure);

    void areFingersInContact(std::vector<bool>& areFingersIncontact);

    void getContactStrength(std::vector<double>& fingersContactStrength);

    void getVibrotactileFeedback(std::vector<double>& fingersVibrotactileFeedback);

    const size_t getNumOfTactileFeedbacks();

    void doesTactileSensorsWork(std::vector<bool>& tactileSensorsAreWorking);
};

#endif // ROBOT_SKIN_HPP

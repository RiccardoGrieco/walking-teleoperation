/**
 * @file GloveWearable.hpp
 * @authors  Kourosh Darvish <kourosh.darvish@iit.it>
 * @copyright 2020 iCub Facility - Istituto Italiano di Tecnologia
 *            Released under the terms of the LGPLv2.1 or later, see LGPL.TXT
 * @date 2021
 */

#ifndef GLOVE_WEARABLE_HPP
#define GLOVE_WEARABLE_HPP


#include <Wearable/IWear/IWear.h>
#include <yarp/sig/Vector.h>
#include <vector>



class GloveWearableImpl
{
private:

    wearable::IWear* m_iWear{nullptr}; /**< Sense glove wearable interface. */

    std::string m_handLinkName;

    std::vector<std::string> m_humanJointNameList;

    std::vector<std::string> m_humanFingerNameList;

    wearable::SensorPtr<const wearable::sensor::IVirtualLinkKinSensor> m_linkSensor;

    std::vector<wearable::SensorPtr<const wearable::sensor::IVirtualJointKinSensor>> m_jointSensors;

    std::vector<wearable::SensorPtr<const wearable::actuator::IHaptic>> m_ForceFeedbackActuators;

    std::vector<wearable::SensorPtr<const wearable::actuator::IHaptic>> m_VibroTactileActuators;

public:

    GloveWearableImpl();

    ~GloveWearableImpl();

    bool configure (const yarp::os::Searchable& config, const std::string& name, const bool& rightHand);

    bool createWearableDataMap();

    bool getSenseGloveHumanJointValues(std::vector<double> values);


    bool getSenseGloveImuValues(std::vector<double> values);


    bool setSenseGloveForceFeedbackValues( std::vector<double> values);

    bool setSenseGloveVibroTactileValues(std::vector<double> values);


};


#endif

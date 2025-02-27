/**
 * @file LinearRegression.hpp
 * @authors  Kourosh Darvish <kourosh.darvish@iit.it>
 * @copyright 2021 iCub Facility - Istituto Italiano di Tecnologia
 *            Released under the terms of the LGPLv2.1 or later, see LGPL.TXT
 * @date 2021
 */

#ifndef LINEAR_REGRESSION_HPP
#define LINEAR_REGRESSION_HPP

#include <ControlHelper.hpp>
#include <iostream>
namespace HapticGlove
{
class LinearRegression;
} // namespace HapticGlove

/**
 * LinearRegression Class useful to learn a linear relationship between inputs and outputs.
 */
class HapticGlove::LinearRegression
{

public:
    LinearRegression();
    ~LinearRegression();

    bool Initialize();

    bool LearnOneShot(const CtrlHelper::Eigen_Mat& inputData,
                      const CtrlHelper::Eigen_Mat& outputData,
                      CtrlHelper::Eigen_Mat& tetha);

    bool LearnOneShotMatrix(const CtrlHelper::Eigen_Mat& inputData,
                            const CtrlHelper::Eigen_Mat& ouputData,
                            CtrlHelper::Eigen_Mat& theta); // A size: <n,m>

    bool LearnIncrementally();
};

#endif // LINEAR_REGRESSION_HPP

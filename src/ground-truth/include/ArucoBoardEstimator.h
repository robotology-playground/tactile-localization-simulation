/******************************************************************************
 *                                                                            *
 * Copyright (C) 2018 Fondazione Istituto Italiano di Tecnologia (IIT)        *
 * All Rights Reserved.                                                       *
 *                                                                            *
 ******************************************************************************/

/**
 * @authors: Nicola Piga
 */

#ifndef ARUCO_BOARD_ESTIMATOR
#define ARUCO_BOARD_ESTIMATOR

// opencv
#include <opencv2/opencv.hpp>
#include <opencv2/aruco.hpp>

//
#include <UcoBoardEstimator.h>

class ArucoBoardEstimator : public UcoBoardEstimator
{
private:
    cv::Ptr<cv::aruco::Board> board;
    cv::Ptr<cv::aruco::Dictionary> dictionary;
    cv::Mat camIntrinsic;
    cv::Mat camDistortion;
public:
    bool configure(const int &n_x, const int &n_y, const double &size1, const double &size2,
                   const cv::Mat &camMatrix, const cv::Mat &distCoeffs);
    void estimateBoardPose(const cv::Mat &img_in, cv::Mat &img_out);
};

#endif

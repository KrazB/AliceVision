// This file is part of the AliceVision project.
// Copyright (c) 2022 AliceVision contributors.
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#include <aliceVision/system/cmdline.hpp>
#include <aliceVision/system/Logger.hpp>
#include <aliceVision/system/main.hpp>


#include <aliceVision/sfmData/SfMData.hpp>
#include <aliceVision/sfmDataIO/sfmDataIO.hpp>

#include <aliceVision/calibration/checkerDetector.hpp>
#include <aliceVision/calibration/checkerDetector_io.hpp>
#include <aliceVision/calibration/distortionEstimation.hpp>

#include <boost/program_options.hpp>

#include <fstream>

// These constants define the current software version.
// They must be updated when the command line is changed.
#define ALICEVISION_SOFTWARE_VERSION_MAJOR 0
#define ALICEVISION_SOFTWARE_VERSION_MINOR 1

namespace po = boost::program_options;
using namespace aliceVision;

bool retrieveLines(std::vector<calibration::LineWithPoints>& lineWithPoints, const calibration::CheckerDetector & detect)
{
    lineWithPoints.clear();

    std::vector<calibration::CheckerDetector::CheckerBoardCorner> corners = detect.getCorners();
    std::vector<calibration::CheckerDetector::CheckerBoard> boards = detect.getBoards();

    int boardIdx = 0;
    for (auto& b : boards)
    {
        // Create horizontal lines
        for (int i = 0; i < b.rows(); i++)
        {
            //Random init
            calibration::LineWithPoints line;
            line.angle = M_PI_4;
            line.dist = 1;
            line.horizontal = true;
            line.index = i;
            line.board = boardIdx;

            for (int j = 0; j < b.cols(); j++)
            {
                const IndexT idx = b(i, j);
                if (idx == UndefinedIndexT) continue;

                const calibration::CheckerDetector::CheckerBoardCorner& p = corners[idx];
                line.points.push_back(p.center);
            }

            //Check we don't have a too small line which won't be easy to estimate
            if (line.points.size() < 10) continue;
            lineWithPoints.push_back(line);
        }

        // Create vertical lines
        for (int j = 0; j < b.cols(); j++)
        {
            calibration::LineWithPoints line;
            line.angle = M_PI_4;
            line.dist = 1;
            line.horizontal = false;
            line.index = j;
            line.board = boardIdx;

            for (int i = 0; i < b.rows(); i++)
            {
                const IndexT idx = b(i, j);
                if (idx == UndefinedIndexT) continue;

                const calibration::CheckerDetector::CheckerBoardCorner& p = corners[idx];
                line.points.push_back(p.center);
            }

            //Check we don't have a too small line which won't be easy to estimate
            if (line.points.size() < 10) continue;

            
            lineWithPoints.push_back(line);
        }

        // Create diagonal 1 lines
        for (int i = 0; i < b.rows(); i++)
        {
            calibration::LineWithPoints line;
            line.angle = M_PI_4;
            line.dist = 1;
            line.horizontal = false;
            line.index = i;
            line.board = boardIdx;

            for (int j = 0; j < b.cols(); j++)
            {
                if (i + j >= b.rows())
                {
                    break;
                }

                const IndexT idx = b(i + j, j);
                if (idx == UndefinedIndexT) continue;

                const calibration::CheckerDetector::CheckerBoardCorner& p = corners[idx];
                line.points.push_back(p.center);
            }

            //Check we don't have a too small line which won't be easy to estimate
            if (line.points.size() < 10) continue;

            lineWithPoints.push_back(line);
        }

        // Create diagonal 2 lines
        for (int j = 0; j < b.cols(); j++)
        {
            calibration::LineWithPoints line;
            line.angle = M_PI_4;
            line.dist = 1;
            line.horizontal = false;
            line.index = j;
            line.board = boardIdx;

            for (int i = 0; i < b.rows(); i++)
            {
                if (i + j >= b.cols())
                {
                    break;
                }

                const IndexT idx = b(i, i + j);
                if (idx == UndefinedIndexT) continue;

                const calibration::CheckerDetector::CheckerBoardCorner& p = corners[idx];
                line.points.push_back(p.center);
            }

            //Check we don't have a too small line which won't be easy to estimate
            if (line.points.size() < 10) continue;

            lineWithPoints.push_back(line);
        }

        // Create diagonal 3 lines
        for (int j = 0; j < b.cols(); j++)
        {
            calibration::LineWithPoints line;
            line.angle = M_PI_4;
            line.dist = 1;
            line.horizontal = false;
            line.index = j;
            line.board = boardIdx;

            for (int i = 0; i < b.rows(); i++)
            {
                if (i + j >= b.cols())
                {
                    break;
                }

                const IndexT idx = b(b.rows() - 1 - i, i + j);
                if (idx == UndefinedIndexT) continue;

                const calibration::CheckerDetector::CheckerBoardCorner& p = corners[idx];
                line.points.push_back(p.center);
            }

            //Check we don't have a too small line which won't be easy to estimate
            if (line.points.size() < 10) continue;

            lineWithPoints.push_back(line);
        }


        boardIdx++;
    }

    if (lineWithPoints.size() < 2)
    {
        return false;
    }

    return true;
}

template <class T>
bool estimateDistortionK1(std::shared_ptr<camera::Pinhole>& camera, calibration::Statistics& statistics, std::vector<T>& items)
{
    std::vector<bool> locksDistortions = { true };

    //Everything locked except lines parameters
    locksDistortions[0] = true;
    if (!calibration::estimate(camera, statistics, items, true, true, locksDistortions, false))
    {
        ALICEVISION_LOG_ERROR("Failed to calibrate");
        return false;
    }

    //Relax distortion 1st order
    locksDistortions[0] = false;
    if (!calibration::estimate(camera, statistics, items, true, true, locksDistortions, false))
    {
        ALICEVISION_LOG_ERROR("Failed to calibrate");
        return false;
    }

    //Relax offcenter
    locksDistortions[0] = false;
    if (!calibration::estimate(camera, statistics, items, true, false, locksDistortions, false))
    {
        ALICEVISION_LOG_ERROR("Failed to calibrate");
        return false;
    }

    return true;
}

template <class T>
bool estimateDistortionK3(std::shared_ptr<camera::Pinhole>& camera, calibration::Statistics& statistics, std::vector<T>& items)
{
    std::vector<bool> locksDistortions = { true, true, true };

    //Everything locked except lines parameters
    locksDistortions[0] = true;
    if (!calibration::estimate(camera, statistics, items, true, true, locksDistortions, false))
    {
        ALICEVISION_LOG_ERROR("Failed to calibrate");
        return false;
    }

    //Relax distortion 1st order
    locksDistortions[0] = false;
    if (!calibration::estimate(camera, statistics, items, true, true, locksDistortions, false))
    {
        ALICEVISION_LOG_ERROR("Failed to calibrate");
        return false;
    }

    //Relax offcenter
    locksDistortions[0] = false;
    if (!calibration::estimate(camera, statistics, items, true, false, locksDistortions, false))
    {
        ALICEVISION_LOG_ERROR("Failed to calibrate");
        return false;
    }

    //Relax offcenter
    locksDistortions[0] = false;
    locksDistortions[1] = false;
    locksDistortions[2] = false;
    if (!calibration::estimate(camera, statistics, items, true, false, locksDistortions, false))
    {
        ALICEVISION_LOG_ERROR("Failed to calibrate");
        return false;
    }

    return true;
}

template <class T>
bool estimateDistortion3DER4(std::shared_ptr<camera::Pinhole>& camera, calibration::Statistics& statistics, std::vector<T>& items)
{
    std::vector<bool> locksDistortions = { true, true, true, true, true, true };

    //Everything locked except lines parameters
    locksDistortions[0] = true;
    if (!calibration::estimate(camera, statistics, items, true, true, locksDistortions, false))
    {
        ALICEVISION_LOG_ERROR("Failed to calibrate");
        return false;
    }

    //Relax distortion 1st order
    locksDistortions[0] = false;
    if (!calibration::estimate(camera, statistics, items, true, true, locksDistortions, false))
    {
        ALICEVISION_LOG_ERROR("Failed to calibrate");
        return false;
    }

    //Relax offcenter
    locksDistortions[0] = false;
    if (!calibration::estimate(camera, statistics, items, true, false, locksDistortions, false))
    {
        ALICEVISION_LOG_ERROR("Failed to calibrate");
        return false;
    }

    //Relax offcenter
    locksDistortions[0] = false;
    locksDistortions[1] = false;
    locksDistortions[2] = false;
    locksDistortions[3] = false;
    locksDistortions[4] = false;
    locksDistortions[5] = false;
    if (!calibration::estimate(camera, statistics, items, true, false, locksDistortions, false))
    {
        ALICEVISION_LOG_ERROR("Failed to calibrate");
        return false;
    }

    return true;
}

template <class T>
bool estimateDistortion3DEA4(std::shared_ptr<camera::Pinhole>& camera, calibration::Statistics& statistics, std::vector<T>& items)
{
    std::shared_ptr<camera::Pinhole> simpleCamera = std::make_shared<camera::PinholeRadialK1>(camera->w(), camera->h(), camera->getScale()[0], camera->getScale()[1], camera->getOffset()[0], camera->getOffset()[1], 0.0);
    if (!estimateDistortionK1(simpleCamera, statistics, items))
    {
        return false;
    }

    std::vector<bool> locksDistortions = { true, true, true, true };

    //Relax distortion all orders
    locksDistortions[0] = false;
    locksDistortions[1] = false;
    locksDistortions[2] = false;
    locksDistortions[3] = false;

    const double k1 = simpleCamera->getDistortionParams()[0];
    camera->setDistortionParams({ k1,k1,k1,k1 });

    /*if (!calibration::estimate(camera, statistics, items, true, false, locksDistortions, false))
    {
        ALICEVISION_LOG_ERROR("Failed to calibrate");
        return false;
    }*/

    if (!calibration::estimate(camera, statistics, items, true, false, locksDistortions, true))
    {
        ALICEVISION_LOG_ERROR("Failed to calibrate");
        return false;
    }

    return true;
}

template <class T>
bool estimateDistortion3DELD(std::shared_ptr<camera::Pinhole>& camera, calibration::Statistics& statistics, std::vector<T>& items)
{
    std::vector<double> params = camera->getDistortionParams();
    params[0] = 0.0;
    params[1] = M_PI_2;
    params[2] = 0.0;
    params[3] = 0.0;
    params[4] = 0.0;
    camera->setDistortionParams(params);

    std::vector<bool> locksDistortions = { true, true, true, true, true };

    //Everything locked except lines parameters
    locksDistortions[0] = true;
    if (!calibration::estimate(camera, statistics, items, true, true, locksDistortions, false))
    {
        ALICEVISION_LOG_ERROR("Failed to calibrate");
        return false;
    }

    //Relax distortion 1st order
    locksDistortions[0] = false;
    if (!calibration::estimate(camera, statistics, items, true, true, locksDistortions, false))
    {
        ALICEVISION_LOG_ERROR("Failed to calibrate");
        return false;
    }

    //Relax offcenter
    locksDistortions[0] = false;
    if (!calibration::estimate(camera, statistics, items, true, false, locksDistortions, false))
    {
        ALICEVISION_LOG_ERROR("Failed to calibrate");
        return false;
    }

    //Relax offcenter
    locksDistortions[0] = false;
    locksDistortions[1] = true;
    locksDistortions[2] = false;
    locksDistortions[3] = false;
    locksDistortions[4] = true;
    if (!calibration::estimate(camera, statistics, items, true, false, locksDistortions, false))
    {
        ALICEVISION_LOG_ERROR("Failed to calibrate");
        return false;
    }

    //Relax offcenter
    locksDistortions[0] = false;
    locksDistortions[1] = false;
    locksDistortions[2] = false;
    locksDistortions[3] = false;
    locksDistortions[4] = false;
    if (!calibration::estimate(camera, statistics, items, true, false, locksDistortions, true))
    {
        ALICEVISION_LOG_ERROR("Failed to calibrate");
        return false;
    }

    return true;
}

bool generatePoints(std::vector<calibration::PointPair>& points, const std::shared_ptr<camera::Pinhole>& camera, const std::vector<calibration::LineWithPoints>& lineWithPoints)
{
    for (auto& l : lineWithPoints)
    {
        for (auto& pt : l.points)
        {
            calibration::PointPair pp;

            //Everything is reverted in the given model (distorting equals to undistorting)
            pp.undistortedPoint = camera->get_d_pixel(pt);
            pp.distortedPoint = pt; 
            const double err = (camera->get_ud_pixel(pp.undistortedPoint) - pp.distortedPoint).norm();
            if (err > 1e-3)
            {
                continue;
            }

            points.push_back(pp);
        }
    }

    return true;
}

int aliceVision_main(int argc, char* argv[]) 
{
    std::string sfmInputDataFilepath;
    std::string checkerBoardsPath;
    std::string sfmOutputDataFilepath;
    std::string verboseLevel = system::EVerboseLevel_enumToString(system::Logger::getDefaultVerboseLevel());

    // Command line parameters
    po::options_description allParams(
        "Parse external information about cameras used in a panorama.\n"
        "AliceVision PanoramaInit");

    po::options_description requiredParams("Required parameters");
    requiredParams.add_options()
        ("input,i", po::value<std::string>(&sfmInputDataFilepath)->required(), "SfMData file input.")
        ("checkerboards", po::value<std::string>(&checkerBoardsPath)->required(), "Checkerboards json files directory.")
        ("outSfMData,o", po::value<std::string>(&sfmOutputDataFilepath)->required(), "SfMData file output.")
        ;

    po::options_description logParams("Log parameters");
    logParams.add_options()
        ("verboseLevel,v", po::value<std::string>(&verboseLevel)->default_value(verboseLevel),
            "verbosity level (fatal, error, warning, info, debug, trace).");

    allParams.add(requiredParams).add(logParams);

    // Parse command line
    po::variables_map vm;
    try
    {
        po::store(po::parse_command_line(argc, argv, allParams), vm);

        if (vm.count("help") || (argc == 1))
        {
            ALICEVISION_COUT(allParams);
            return EXIT_SUCCESS;
        }
        po::notify(vm);
    }
    catch (boost::program_options::required_option& e)
    {
        ALICEVISION_CERR("ERROR: " << e.what());
        ALICEVISION_COUT("Usage:\n\n" << allParams);
        return EXIT_FAILURE;
    }
    catch (boost::program_options::error& e)
    {
        ALICEVISION_CERR("ERROR: " << e.what());
        ALICEVISION_COUT("Usage:\n\n" << allParams);
        return EXIT_FAILURE;
    }

    ALICEVISION_COUT("Program called with the following parameters:");
    ALICEVISION_COUT(vm);

    system::Logger::get()->setLogLevel(verboseLevel);

    //Load sfmData from disk
    sfmData::SfMData sfmData;
    if (!sfmDataIO::Load(sfmData, sfmInputDataFilepath, sfmDataIO::ESfMData(sfmDataIO::ALL)))
    {
        ALICEVISION_LOG_ERROR("The input SfMData file '" << sfmInputDataFilepath << "' cannot be read.");
        return EXIT_FAILURE;
    }


    //Load the checkerboards
    std::map < IndexT, calibration::CheckerDetector> boardsAllImages;
    for (auto& pv : sfmData.getViews())
    {
        IndexT viewId = pv.first;

        // Read the json file
        std::stringstream ss;
        ss << checkerBoardsPath << "/" << "checkers_" << viewId << ".json";
        std::ifstream inputfile(ss.str());
        if (inputfile.is_open() == false)
        {
            continue;
        }

        std::stringstream buffer;
        buffer << inputfile.rdbuf();
        boost::json::value jv = boost::json::parse(buffer.str());

        //Store the checkerboard
        calibration::CheckerDetector detector(boost::json::value_to<calibration::CheckerDetector>(jv));
        boardsAllImages[viewId] = detector;
    }

    //Calibrate each intrinsic independently
    for (auto& pi : sfmData.getIntrinsics())
    {
        IndexT intrinsicId = pi.first;

        //Convert to pinhole
        std::shared_ptr<camera::IntrinsicBase>& intrinsicPtr = pi.second;
        std::shared_ptr<camera::Pinhole> cameraPinhole = std::dynamic_pointer_cast<camera::Pinhole>(intrinsicPtr);
        if (!cameraPinhole)
        {
            ALICEVISION_LOG_ERROR("Only work for pinhole cameras");
            return EXIT_FAILURE;
        }
        ALICEVISION_LOG_INFO("Processing Intrinsic " << intrinsicId);


        //Transform checkerboards to line With points
        std::vector<calibration::LineWithPoints> allLinesWithPoints;
        for (auto& pv : sfmData.getViews())
        {
            if (pv.second->getIntrinsicId() != intrinsicId)
            {
                continue;
            }

            std::vector<calibration::LineWithPoints> linesWithPoints;
            if (!retrieveLines(linesWithPoints, boardsAllImages[pv.first]))
            {
                continue;
            }

            allLinesWithPoints.insert(allLinesWithPoints.end(), linesWithPoints.begin(), linesWithPoints.end());
        }


        calibration::Statistics statistics;

        // Estimate distortion
        if (std::dynamic_pointer_cast<camera::PinholeRadialK1>(cameraPinhole))
        {
            if (!estimateDistortionK1(cameraPinhole, statistics, allLinesWithPoints))
            {
                ALICEVISION_LOG_ERROR("Error estimating distortion");
                continue;
            }
        }
        else if (std::dynamic_pointer_cast<camera::PinholeRadialK3>(cameraPinhole))
        {
            if (!estimateDistortionK3(cameraPinhole, statistics, allLinesWithPoints))
            {
                ALICEVISION_LOG_ERROR("Error estimating distortion");
                continue;
            }
        }
        else if (std::dynamic_pointer_cast<camera::Pinhole3DERadial4>(cameraPinhole))
        {
            if (!estimateDistortion3DER4(cameraPinhole, statistics, allLinesWithPoints))
            {
                ALICEVISION_LOG_ERROR("Error estimating distortion");
                continue;
            }
        }
        else if (std::dynamic_pointer_cast<camera::Pinhole3DEAnamorphic4>(cameraPinhole))
        {
            if (!estimateDistortion3DEA4(cameraPinhole, statistics, allLinesWithPoints))
            {
                ALICEVISION_LOG_ERROR("Error estimating distortion");
                continue;
            }
        }
        else if (std::dynamic_pointer_cast<camera::Pinhole3DEClassicLD>(cameraPinhole))
        {
            if (!estimateDistortion3DELD(cameraPinhole, statistics, allLinesWithPoints))
            {
                ALICEVISION_LOG_ERROR("Error estimating distortion");
                continue;
            }
        }
        else
        {
            ALICEVISION_LOG_ERROR("Incompatible camera distortion model");
        }

        ALICEVISION_LOG_INFO("Result quality of calibration: ");
        ALICEVISION_LOG_INFO("Mean of error (stddev): " << statistics.mean << "(" << statistics.stddev << ")");
        ALICEVISION_LOG_INFO("Median of error: " << statistics.median);

        //Now, the distortion is estimated, but we have the inverted problem : how to dedistort, we need to inverse the solution
        std::vector<calibration::PointPair> points;
        if (!generatePoints(points, cameraPinhole, allLinesWithPoints))
        {
            ALICEVISION_LOG_ERROR("Error generating points");
            continue;
        }

        // Estimate distortion
        if (std::dynamic_pointer_cast<camera::PinholeRadialK1>(cameraPinhole))
        {
            if (!estimateDistortionK1(cameraPinhole, statistics, points))
            {
                ALICEVISION_LOG_ERROR("Error estimating reverse distortion");
                continue;
            }
        }
        else if (std::dynamic_pointer_cast<camera::PinholeRadialK3>(cameraPinhole))
        {
            if (!estimateDistortionK3(cameraPinhole, statistics, points))
            {
                ALICEVISION_LOG_ERROR("Error estimating reverse distortion");
                continue;
            }
        }
        else if (std::dynamic_pointer_cast<camera::Pinhole3DERadial4>(cameraPinhole))
        {
            if (!estimateDistortion3DER4(cameraPinhole, statistics, points))
            {
                ALICEVISION_LOG_ERROR("Error estimating reverse distortion");
                continue;
            }
        }
        else if (std::dynamic_pointer_cast<camera::Pinhole3DEAnamorphic4>(cameraPinhole))
        {
            if (!estimateDistortion3DEA4(cameraPinhole, statistics, points))
            {
                ALICEVISION_LOG_ERROR("Error estimating reverse distortion");
                continue;
            }
        }
        else if (std::dynamic_pointer_cast<camera::Pinhole3DEClassicLD>(cameraPinhole))
        {
            if (!estimateDistortion3DELD(cameraPinhole, statistics, points))
            {
                ALICEVISION_LOG_ERROR("Error estimating reverse distortion");
                continue;
            }
        }
        else
        {
            ALICEVISION_LOG_ERROR("Incompatible camera distortion model");
        }

        ALICEVISION_LOG_INFO("Result quality of inversion: ");
        ALICEVISION_LOG_INFO("Mean of error (stddev): " << statistics.mean << "(" << statistics.stddev << ")");
        ALICEVISION_LOG_INFO("Median of error: " << statistics.median);
    }

    //Save sfmData to disk
    if (!sfmDataIO::Save(sfmData, sfmOutputDataFilepath, sfmDataIO::ESfMData(sfmDataIO::ALL)))
    {
        ALICEVISION_LOG_ERROR("The output SfMData file '" << sfmOutputDataFilepath << "' cannot be written.");
        return EXIT_FAILURE;
    }

	return EXIT_SUCCESS;
}

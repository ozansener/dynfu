#ifndef DYNFU_DYNFUSION_HPP
#define DYNFU_DYNFUSION_HPP

/* dynfu includes */
#include <dynfu/utils/ceres_solver.hpp>
#include <dynfu/utils/dual_quaternion.hpp>
#include <dynfu/utils/frame.hpp>
#include <dynfu/utils/opt_solver.hpp>
#include <dynfu/utils/pointcloud_viz.hpp>
#include <dynfu/warp_field.hpp>

/* ceres includes */
#include <ceres/ceres.h>

/* typedefs */
#include <kfusion/cuda/tsdf_volume.hpp>
#include <kfusion/types.hpp>

/* sys headers */
#include <math.h>
#include <thread>

using namespace kfusion;
using namespace kfusion::cuda;

/* */
class DynFusion {
public:
    DynFusion();
    ~DynFusion();

    /* helper function to determine whether a point has NaNs */
    static bool DynFusion::hasNaNs(kfusion::Point pt);

    /* helper function to determine whether vertex is part of the background */
    static bool isBackground(kfusion::Point pt);

    /* helper function to determine whether vertex is part of the canonical model */
    static bool DynFusion::isCanonical(cv::Vec3f vertex);

    void init(cv::Ptr<kfusion::cuda::TsdfVolume> &tsdfVolume, kfusion::cuda::Cloud &vertices,
              kfusion::cuda::Cloud &normals);

    void initCanonicalFrame(std::vector<cv::Vec3f> vertices, std::vector<cv::Vec3f> normals);

    // void updateCanonicalFrame();

    /* update the warp field if the no. of deformation nodes is insufficient to capture the geometry of canonical model
     */
    void updateWarpfield();

    /* warp canonical frame to live frame using Ceres */
    void warpCanonicalToLive();

    /* warp canonical frame to live frame using Opt */
    void warpCanonicalToLiveOpt();

    /* update the current live frame */
    void addLiveFrame(int frameID, kfusion::cuda::Cloud &vertices, kfusion::cuda::Normals &normals);

    /* get the canonical frame warped to live */
    std::shared_ptr<dynfu::Frame> getCanonicalWarpedToLive();

    /* control the thread deletion */
    static bool nextFrameReady;

private:
    std::shared_ptr<dynfu::Frame> canonicalFrame;
    std::shared_ptr<dynfu::Frame> canonicalWarpedToLive;
    std::shared_ptr<dynfu::Frame> liveFrame;

    std::shared_ptr<Warpfield> warpfield;

    /* convert cloud to OpenCV matrix */
    cv::Mat cloudToMat(kfusion::cuda::Cloud cloud);
    /* convert OpenCV matrix to cloud */
    kfusion::cuda::Cloud matToCloud(cv::Mat matrix);
    /* convert normals to OpenCV matrix */
    cv::Mat normalsToMat(kfusion::cuda::Normals normals);
    /* convert OpenCV matrix to vector of Vec3f */
    std::vector<cv::Vec3f> matToVector(cv::Mat);
};

/* DYNFU_DYNFUSION_HPP */
#endif

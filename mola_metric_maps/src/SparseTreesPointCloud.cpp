/* -------------------------------------------------------------------------
 *   A Modular Optimization framework for Localization and mApping  (MOLA)
 *
 * Copyright (C) 2018-2024 Jose Luis Blanco, University of Almeria
 * Licensed under the GNU GPL v3 for non-commercial applications.
 *
 * This file is part of MOLA.
 * MOLA is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * MOLA is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * MOLA. If not, see <https://www.gnu.org/licenses/>.
 * ------------------------------------------------------------------------- */

/**
 * @file   SparseTreesPointCloud.cpp
 * @brief  Point cloud stored as a 3D grid of KD-trees/pointclouds
 * @author Jose Luis Blanco Claraco
 * @date   Nov 11, 2023
 */

#include <mola_metric_maps/SparseTreesPointCloud.h>
#include <mrpt/config/CConfigFileBase.h>  // MRPT_LOAD_CONFIG_VAR
#include <mrpt/maps/CSimplePointsMap.h>
#include <mrpt/math/CHistogram.h>
#include <mrpt/math/distributions.h>  // confidenceIntervals()
#include <mrpt/obs/CObservation2DRangeScan.h>
#include <mrpt/obs/CObservation3DRangeScan.h>
#include <mrpt/obs/CObservationPointCloud.h>
#include <mrpt/obs/CObservationVelodyneScan.h>
#include <mrpt/opengl/CPointCloud.h>
#include <mrpt/opengl/CPointCloudColoured.h>
#include <mrpt/serialization/CArchive.h>  // serialization
#include <mrpt/system/os.h>

#include <cmath>

//#define USE_DEBUG_PROFILER

#ifdef USE_DEBUG_PROFILER
#include <mrpt/system/CTimeLogger.h>
static mrpt::system::CTimeLogger profiler(true, "SparseTreesPointCloud");
#endif

using namespace mola;

//  =========== Begin of Map definition ============
MAP_DEFINITION_REGISTER(
    "mola::SparseTreesPointCloud,SparseTreesPointCloud",
    mola::SparseTreesPointCloud)

SparseTreesPointCloud::TMapDefinition::TMapDefinition() = default;
void SparseTreesPointCloud::TMapDefinition::loadFromConfigFile_map_specific(
    const mrpt::config::CConfigFileBase& s, const std::string& sectionPrefix)
{
    using namespace std::string_literals;

    // [<sectionNamePrefix>+"_creationOpts"]
    const std::string sSectCreation = sectionPrefix + "_creationOpts"s;
    MRPT_LOAD_CONFIG_VAR(grid_size, float, s, sSectCreation);

    ASSERT_(s.sectionExists(sectionPrefix + "_insertOpts"s));
    insertionOpts.loadFromConfigFile(s, sectionPrefix + "_insertOpts"s);

    ASSERT_(s.sectionExists(sectionPrefix + "_likelihoodOpts"s));
    likelihoodOpts.loadFromConfigFile(s, sectionPrefix + "_likelihoodOpts"s);

    ASSERT_(s.sectionExists(sectionPrefix + "_renderOpts"s));
    renderOpts.loadFromConfigFile(s, sectionPrefix + "_renderOpts"s);
}

void SparseTreesPointCloud::TMapDefinition::dumpToTextStream_map_specific(
    std::ostream& out) const
{
    LOADABLEOPTS_DUMP_VAR(grid_size, float);

    insertionOpts.dumpToTextStream(out);
    likelihoodOpts.dumpToTextStream(out);
    renderOpts.dumpToTextStream(out);
}

mrpt::maps::CMetricMap::Ptr
    SparseTreesPointCloud::internal_CreateFromMapDefinition(
        const mrpt::maps::TMetricMapInitializer& _def)
{
    const SparseTreesPointCloud::TMapDefinition* def =
        dynamic_cast<const SparseTreesPointCloud::TMapDefinition*>(&_def);
    ASSERT_(def);
    auto obj = SparseTreesPointCloud::Create(def->grid_size);

    obj->insertionOptions  = def->insertionOpts;
    obj->likelihoodOptions = def->likelihoodOpts;
    obj->renderOptions     = def->renderOpts;

    return obj;
}
//  =========== End of Map definition Block =========

IMPLEMENTS_SERIALIZABLE(SparseTreesPointCloud, CMetricMap, mola)

// =====================================
// Serialization
// =====================================

uint8_t SparseTreesPointCloud::serializeGetVersion() const { return 0; }
void    SparseTreesPointCloud::serializeTo(
    mrpt::serialization::CArchive& out) const
{
    // params:
    out << GLOBAL_ID_SUBVOXEL_BITCOUNT << grid_size_;
    insertionOptions.writeToStream(out);
    likelihoodOptions.writeToStream(out);
    renderOptions.writeToStream(out);

    // data:
    out.WriteAs<uint32_t>(grids_.size());
    for (const auto& kv : grids_)
    {
        out << kv.first.cx << kv.first.cy << kv.first.cz;
        out << kv.second.points();
    }
}
void SparseTreesPointCloud::serializeFrom(
    mrpt::serialization::CArchive& in, uint8_t version)
{
    switch (version)
    {
        case 0:
        {
            // params:
            const auto expected_grid_bit_count = in.ReadAs<uint8_t>();
            ASSERT_EQUAL_(expected_grid_bit_count, GLOBAL_ID_SUBVOXEL_BITCOUNT);

            in >> grid_size_;

            // clear contents and compute computed fields:
            this->setGridProperties(grid_size_);

            insertionOptions.readFromStream(in);
            likelihoodOptions.readFromStream(in);
            renderOptions.readFromStream(in);

            // data:
            const auto nGrids = in.ReadAs<uint32_t>();
            for (uint32_t i = 0; i < nGrids; i++)
            {
                outer_index3d_t idx;
                in >> idx.cx >> idx.cy >> idx.cz;

                auto& grid = grids_[idx];
                in >> grid.points();
            }
        }
        break;
        default:
            MRPT_THROW_UNKNOWN_SERIALIZATION_VERSION(version);
    };

    // cache reset:
    cached_.reset();
}

// Ctor:
SparseTreesPointCloud::SparseTreesPointCloud(float grid_size)
{
    setGridProperties(grid_size);
}

SparseTreesPointCloud::~SparseTreesPointCloud() = default;

void SparseTreesPointCloud::setGridProperties(float grid_size)
{
    grid_size_ = grid_size;

    // calculated fields:
    grid_size_inv_  = 1.0f / grid_size;
    grid_size_half_ = 0.5f * grid_size;
    gridVector_     = mrpt::math::TVector3D(1., 1., 1.) * grid_size_;

    // clear all:
    SparseTreesPointCloud::internal_clear();
}

std::string SparseTreesPointCloud::asString() const
{
    return mrpt::format(
        "SparseTreesPointCloud, grid_size=%.03f bbox=%s", grid_size_,
        boundingBox().asString().c_str());
}

void SparseTreesPointCloud::getVisualizationInto(
    mrpt::opengl::CSetOfObjects& outObj) const
{
    MRPT_START
    if (!genericMapParams.enableSaveAs3DObject) return;

    if (renderOptions.colormap == mrpt::img::cmNONE)
    {
        // Single color:
        auto obj = mrpt::opengl::CPointCloud::Create();

        const auto lambdaVisitPoints = [&obj](const mrpt::math::TPoint3Df& pt) {
            obj->insertPoint(pt);
        };
        this->visitAllPoints(lambdaVisitPoints);

        obj->setColor(renderOptions.color);
        obj->setPointSize(renderOptions.point_size);
        obj->enableColorFromZ(false);
        outObj.insert(obj);
    }
    else
    {
        auto obj = mrpt::opengl::CPointCloudColoured::Create();

        auto bb = this->boundingBox();

        // handle planar maps (avoids error in histogram below):
        for (int i = 0; i < 3; i++)
            if (bb.max[i] == bb.min[i]) bb.max[i] = bb.min[i] + 0.1f;

        // Use a histogram to discard outliers from the colormap extremes:
        constexpr size_t nBins = 100;
        // for x,y,z
        std::array<mrpt::math::CHistogram, 3> hists = {
            mrpt::math::CHistogram(bb.min.x, bb.max.x, nBins),
            mrpt::math::CHistogram(bb.min.y, bb.max.y, nBins),
            mrpt::math::CHistogram(bb.min.z, bb.max.z, nBins)};

        const auto lambdaVisitPoints =
            [&obj, &hists](const mrpt::math::TPoint3Df& pt) {
                // x y z R G B [A]
                obj->insertPoint({pt.x, pt.y, pt.z, 0, 0, 0});
                for (int i = 0; i < 3; i++) hists[i].add(pt[i]);
            };

        this->visitAllPoints(lambdaVisitPoints);

        obj->setPointSize(renderOptions.point_size);

        // Analyze the histograms and get confidence intervals:
        std::vector<double> coords;
        std::vector<double> hits;

        const int idx = renderOptions.recolorizeByCoordinateIndex;
        ASSERT_(idx >= 0 && idx < 3);

        float            min = .0, max = 1.f;
        constexpr double confidenceInterval = 0.02;

        hists[idx].getHistogramNormalized(coords, hits);
        mrpt::math::confidenceIntervalsFromHistogram(
            coords, hits, min, max, confidenceInterval);

        obj->recolorizeByCoordinate(
            min, max, renderOptions.recolorizeByCoordinateIndex,
            renderOptions.colormap);
        outObj.insert(obj);
    }
    if (renderOptions.show_inner_grid_boxes)
    {
        auto lambdaForEachGrid =
            [this, &outObj](const outer_index3d_t& idxs, const GridData&) {
                const mrpt::math::TPoint3Df gridCenter = outerIdxToCoord(idxs);

                auto glBox = mrpt::opengl::CBox::Create();
                glBox->setWireframe(true);
                const auto org = gridCenter.cast<double>();
                glBox->setBoxCorners(org, org + gridVector_);

                outObj.insert(glBox);
            };

        this->visitAllGrids(lambdaForEachGrid);
    }
    MRPT_END
}

void SparseTreesPointCloud::internal_clear() { grids_.clear(); }

bool SparseTreesPointCloud::internal_insertObservation(
    const mrpt::obs::CObservation&                   obs,
    const std::optional<const mrpt::poses::CPose3D>& robotPose)
{
    MRPT_START

    using namespace mrpt::obs;

    mrpt::poses::CPose2D robotPose2D;
    mrpt::poses::CPose3D robotPose3D;

    if (robotPose)
    {
        robotPose2D = mrpt::poses::CPose2D(*robotPose);
        robotPose3D = (*robotPose);

        if (insertionOptions.remove_submaps_farther_than > 0)
        {
            eraseGridsFartherThan(
                robotPose3D.translation().cast<float>(),
                insertionOptions.remove_submaps_farther_than);
        }
    }
    else
    {
        // Default values are (0,0,0)
    }

    if (IS_CLASS(obs, CObservation2DRangeScan))
    {
        /********************************************************************
                    OBSERVATION TYPE: CObservation2DRangeScan
         ********************************************************************/
        const auto& o = static_cast<const CObservation2DRangeScan&>(obs);

        // Build (if not done before) the points map representation of
        // this observation:
        const auto* scanPoints = o.buildAuxPointsMap<mrpt::maps::CPointsMap>();

        if (scanPoints->empty()) return 0;

        const auto& xs = scanPoints->getPointsBufferRef_x();
        const auto& ys = scanPoints->getPointsBufferRef_y();
        const auto& zs = scanPoints->getPointsBufferRef_z();

        internal_insertPointCloud3D(
            robotPose3D, xs.data(), ys.data(), zs.data(), xs.size());

        return true;
    }
    else if (IS_CLASS(obs, CObservation3DRangeScan))
    {
        /********************************************************************
                    OBSERVATION TYPE: CObservation3DRangeScan
         ********************************************************************/
        const auto& o = static_cast<const CObservation3DRangeScan&>(obs);

        mrpt::obs::T3DPointsProjectionParams pp;
        pp.takeIntoAccountSensorPoseOnRobot = true;

        // Empty point set, or load from XYZ in observation:
        if (o.hasPoints3D)
        {
            for (size_t i = 0; i < o.points3D_x.size(); i++)
                this->insertPoint(robotPose3D.composePoint(
                    {o.points3D_x[i], o.points3D_y[i], o.points3D_z[i]}));

            return true;
        }
        else if (o.hasRangeImage)
        {
            mrpt::maps::CSimplePointsMap pointMap;
            const_cast<CObservation3DRangeScan&>(o).unprojectInto(pointMap, pp);

            const auto& xs = pointMap.getPointsBufferRef_x();
            const auto& ys = pointMap.getPointsBufferRef_y();
            const auto& zs = pointMap.getPointsBufferRef_z();

            internal_insertPointCloud3D(
                robotPose3D, xs.data(), ys.data(), zs.data(), xs.size());

            return true;
        }
        else
            return false;
    }
    else if (IS_CLASS(obs, CObservationVelodyneScan))
    {
        /********************************************************************
                    OBSERVATION TYPE: CObservationVelodyneScan
         ********************************************************************/
        const auto& o = static_cast<const CObservationVelodyneScan&>(obs);

        // Automatically generate pointcloud if needed:
        if (!o.point_cloud.size())
            const_cast<CObservationVelodyneScan&>(o).generatePointCloud();

        internal_insertPointCloud3D(
            robotPose3D, o.point_cloud.x.data(), o.point_cloud.y.data(),
            o.point_cloud.z.data(), o.point_cloud.x.size());

        return true;
    }
    else if (IS_CLASS(obs, CObservationPointCloud))
    {
        const auto& o = static_cast<const CObservationPointCloud&>(obs);
        ASSERT_(o.pointcloud);

        const auto& xs = o.pointcloud->getPointsBufferRef_x();
        const auto& ys = o.pointcloud->getPointsBufferRef_y();
        const auto& zs = o.pointcloud->getPointsBufferRef_z();

        internal_insertPointCloud3D(
            robotPose3D, xs.data(), ys.data(), zs.data(), xs.size());

        return true;
    }
    else
    {
        /********************************************************************
                    OBSERVATION TYPE: Unknown
        ********************************************************************/
        return false;
    }

    MRPT_END
}

double SparseTreesPointCloud::internal_computeObservationLikelihood(
    const mrpt::obs::CObservation& obs,
    const mrpt::poses::CPose3D&    takenFrom) const
{
    using namespace mrpt::obs;
    using namespace mrpt::poses;
    using namespace mrpt::maps;

    if (isEmpty()) return 0;

    // This function depends on the observation type:
    // -----------------------------------------------------
    if (IS_CLASS(obs, CObservation2DRangeScan))
    {
        // Observation is a laser range scan:
        // -------------------------------------------
        const auto& o = static_cast<const CObservation2DRangeScan&>(obs);

        // Build (if not done before) the points map representation of
        // this observation:
        const auto* scanPoints = o.buildAuxPointsMap<CPointsMap>();

        const size_t N = scanPoints->size();
        if (!N) return 0;

        const auto& xs = scanPoints->getPointsBufferRef_x();
        const auto& ys = scanPoints->getPointsBufferRef_y();
        const auto& zs = scanPoints->getPointsBufferRef_z();

        return internal_computeObservationLikelihoodPointCloud3D(
            takenFrom, xs.data(), ys.data(), zs.data(), N);
    }
    else if (IS_CLASS(obs, CObservationVelodyneScan))
    {
        const auto& o = dynamic_cast<const CObservationVelodyneScan&>(obs);

        // Automatically generate pointcloud if needed:
        if (!o.point_cloud.size())
            const_cast<CObservationVelodyneScan&>(o).generatePointCloud();

        const size_t N = o.point_cloud.size();
        if (!N) return 0;

        const CPose3D sensorAbsPose = takenFrom + o.sensorPose;

        const auto& xs = o.point_cloud.x;
        const auto& ys = o.point_cloud.y;
        const auto& zs = o.point_cloud.z;

        return internal_computeObservationLikelihoodPointCloud3D(
            sensorAbsPose, xs.data(), ys.data(), zs.data(), N);
    }
    else if (IS_CLASS(obs, CObservationPointCloud))
    {
        const auto& o = dynamic_cast<const CObservationPointCloud&>(obs);

        const size_t N = o.pointcloud->size();
        if (!N) return 0;

        const CPose3D sensorAbsPose = takenFrom + o.sensorPose;

        auto xs = o.pointcloud->getPointsBufferRef_x();
        auto ys = o.pointcloud->getPointsBufferRef_y();
        auto zs = o.pointcloud->getPointsBufferRef_z();

        return internal_computeObservationLikelihoodPointCloud3D(
            sensorAbsPose, xs.data(), ys.data(), zs.data(), N);
    }

    return .0;
}

double SparseTreesPointCloud::internal_computeObservationLikelihoodPointCloud3D(
    const mrpt::poses::CPose3D& pc_in_map, const float* xs, const float* ys,
    const float* zs, const std::size_t num_pts) const
{
    MRPT_TRY_START

    ASSERT_GT_(likelihoodOptions.sigma_dist, .0);

    mrpt::math::TPoint3Df closest;
    float                 closest_err;
    uint64_t              closest_id;
    const float max_sqr_err = mrpt::square(likelihoodOptions.max_corr_distance);
    double      sumSqrDist  = .0;

    std::size_t nPtsForAverage = 0;
    for (std::size_t i = 0; i < num_pts;
         i += likelihoodOptions.decimation, nPtsForAverage++)
    {
        // Transform the point from the scan reference to its global 3D
        // position:
        const auto gPt = pc_in_map.composePoint({xs[i], ys[i], zs[i]});

        const bool found =
            nn_single_search(gPt, closest, closest_err, closest_id);
        if (!found) continue;

        // Put a limit:
        mrpt::keep_min(closest_err, max_sqr_err);

        sumSqrDist += static_cast<double>(closest_err);
    }
    if (nPtsForAverage) sumSqrDist /= nPtsForAverage;

    // Log-likelihood:
    return -sumSqrDist / likelihoodOptions.sigma_dist;

    MRPT_TRY_END
}

bool SparseTreesPointCloud::internal_canComputeObservationLikelihood(
    const mrpt::obs::CObservation& obs) const
{
    using namespace mrpt::obs;

    return IS_CLASS(obs, CObservation2DRangeScan) ||
           IS_CLASS(obs, CObservationVelodyneScan) ||
           IS_CLASS(obs, CObservationPointCloud);
}

bool SparseTreesPointCloud::isEmpty() const
{
    // empty if no voxels exist:
    return grids_.empty();
}

void SparseTreesPointCloud::saveMetricMapRepresentationToFile(
    const std::string& filNamePrefix) const
{
    using namespace std::string_literals;

    const auto fil = filNamePrefix + ".txt"s;
    saveToTextFile(fil);
}

bool SparseTreesPointCloud::saveToTextFile(const std::string& file) const
{
    FILE* f = mrpt::system::os::fopen(file.c_str(), "wt");
    if (!f) return false;

    const auto lambdaVisitPoints = [f](const mrpt::math::TPoint3Df& pt) {
        mrpt::system::os::fprintf(f, "%f %f %f\n", pt.x, pt.y, pt.z);
    };

    this->visitAllPoints(lambdaVisitPoints);

    mrpt::system::os::fclose(f);
    return true;
}

bool SparseTreesPointCloud::nn_has_indices_or_ids() const
{  // false: IDs, not contiguous indices
    return false;
}

void SparseTreesPointCloud::nn_prepare_for_2d_queries() const
{
    for (auto& [idx, grid] : grids_) grid.points().nn_prepare_for_2d_queries();
}
void SparseTreesPointCloud::nn_prepare_for_3d_queries() const
{
    for (auto& [idx, grid] : grids_) grid.points().nn_prepare_for_3d_queries();
}

size_t SparseTreesPointCloud::nn_index_count() const
{  // Not used.
    return 0;
}

bool SparseTreesPointCloud::nn_single_search(
    [[maybe_unused]] const mrpt::math::TPoint2Df& query,
    [[maybe_unused]] mrpt::math::TPoint2Df&       result,
    [[maybe_unused]] float&                       out_dist_sqr,
    [[maybe_unused]] uint64_t&                    resultIndexOrID) const
{
    THROW_EXCEPTION("Cannot run a 2D search on a SparseTreesPointCloud");
}
void SparseTreesPointCloud::nn_multiple_search(
    [[maybe_unused]] const mrpt::math::TPoint2Df&        query,
    [[maybe_unused]] const size_t                        N,
    [[maybe_unused]] std::vector<mrpt::math::TPoint2Df>& results,
    [[maybe_unused]] std::vector<float>&                 out_dists_sqr,
    [[maybe_unused]] std::vector<uint64_t>& resultIndicesOrIDs) const
{
    THROW_EXCEPTION("Cannot run a 2D search on a SparseTreesPointCloud");
}
void SparseTreesPointCloud::nn_radius_search(
    [[maybe_unused]] const mrpt::math::TPoint2Df&        query,
    [[maybe_unused]] const float                         search_radius_sqr,
    [[maybe_unused]] std::vector<mrpt::math::TPoint2Df>& results,
    [[maybe_unused]] std::vector<float>&                 out_dists_sqr,
    [[maybe_unused]] std::vector<uint64_t>&              resultIndicesOrIDs,
    [[maybe_unused]] size_t                              maxPoints) const
{
    THROW_EXCEPTION("Cannot run a 2D search on a SparseTreesPointCloud");
}

bool SparseTreesPointCloud::nn_single_search(
    const mrpt::math::TPoint3Df& query, mrpt::math::TPoint3Df& result,
    float& out_dist_sqr, uint64_t& resultIndexOrID) const
{
    const outer_index3d_t idxs = coordToOuterIdx(query);

    auto* g = gridByOuterIdxs(idxs, false);
    if (!g) return false;

    return g->points().nn_single_search(
        query, result, out_dist_sqr, resultIndexOrID);
}

void SparseTreesPointCloud::nn_multiple_search(
    const mrpt::math::TPoint3Df& query, const size_t N,
    std::vector<mrpt::math::TPoint3Df>& results,
    std::vector<float>&                 out_dists_sqr,
    std::vector<uint64_t>&              resultIndicesOrIDs) const
{
    const outer_index3d_t idxs = coordToOuterIdx(query);

    results.clear();
    out_dists_sqr.clear();
    resultIndicesOrIDs.clear();

    auto* g = gridByOuterIdxs(idxs, false);
    if (!g) return;

    g->points().nn_multiple_search(
        query, N, results, out_dists_sqr, resultIndicesOrIDs);
}

void SparseTreesPointCloud::nn_radius_search(
    const mrpt::math::TPoint3Df& query, const float search_radius_sqr,
    std::vector<mrpt::math::TPoint3Df>& results,
    std::vector<float>&                 out_dists_sqr,
    std::vector<uint64_t>& resultIndicesOrIDs, size_t maxPoints) const
{
    //    const double t0 = mrpt::Clock::nowDouble();

    results.clear();
    out_dists_sqr.clear();
    resultIndicesOrIDs.clear();

    if (search_radius_sqr <= 0) return;

    const float radius   = std::sqrt(search_radius_sqr);
    const auto  diagonal = mrpt::math::TPoint3Df(1.0f, 1.0f, 1.0f) * radius;

    const outer_index3d_t idxs0 = coordToOuterIdx(query - diagonal);
    const outer_index3d_t idxs1 = coordToOuterIdx(query + diagonal);

    // Used to sort all candidates by distance:
    std::map<
        float /*distSqr*/, std::pair<mrpt::math::TPoint3Df, uint64_t /*id*/>>
        candidates;

    auto lmbAddCandidate = [&](const float                  distSqr,
                               const mrpt::math::TPoint3Df& pt, uint64_t id) {
        candidates[distSqr] = {pt, id};
    };

    auto lambdaCheckCell = [&](const outer_index3d_t& p) {
        auto* g = gridByOuterIdxs(p, false);
        if (!g) return;

        std::vector<mrpt::math::TPoint3Df> sub_results;
        std::vector<float>                 sub_out_dists_sqr;
        std::vector<uint64_t>              sub_resultIndicesOrIDs;

        g->points().nn_radius_search(
            query, search_radius_sqr, sub_results, sub_out_dists_sqr,
            sub_resultIndicesOrIDs, maxPoints);

        for (size_t i = 0; i < sub_results.size(); i++)
            lmbAddCandidate(
                sub_out_dists_sqr[i], sub_results[i],
                sub_resultIndicesOrIDs[i]);
    };

    for (int32_t cx = idxs0.cx; cx <= idxs1.cx; cx++)
        for (int32_t cy = idxs0.cy; cy <= idxs1.cy; cy++)
            for (int32_t cz = idxs0.cz; cz <= idxs1.cz; cz++)
                lambdaCheckCell({cx, cy, cz});

    for (const auto& kv : candidates)
    {
        results.push_back(kv.second.first);
        out_dists_sqr.push_back(kv.first);
        resultIndicesOrIDs.push_back(kv.second.second);
    }

#if 0
    const double t1 = mrpt::Clock::nowDouble();
    printf(
        "nnRadiusSearch: cells=%i t=%f ms\n",
        (idxs1.cz - idxs0.cz + 1) * (idxs1.cy - idxs0.cy + 1) *
            (idxs1.cx - idxs0.cx + 1),
        1e3 * (t1 - t0));
#endif
}

mrpt::math::TBoundingBoxf SparseTreesPointCloud::boundingBox() const
{
    if (!cached_.boundingBox_)
    {
        cached_.boundingBox_.emplace();
        if (this->isEmpty())
        {
            cached_.boundingBox_->min = {0, 0, 0};
            cached_.boundingBox_->max = {0, 0, 0};
        }
        else
        {
            cached_.boundingBox_ =
                mrpt::math::TBoundingBoxf::PlusMinusInfinity();

            auto lambdaForEachPt = [this](const mrpt::math::TPoint3Df& pt) {
                cached_.boundingBox_->updateWithPoint(pt);
                cached_.boundingBox_->updateWithPoint(pt);
            };

            this->visitAllPoints(lambdaForEachPt);
        }
    }

    return cached_.boundingBox_.value();
}

void SparseTreesPointCloud::visitAllPoints(
    const std::function<void(const mrpt::math::TPoint3Df&)>& f) const
{
    for (const auto& kv : grids_)
    {
        const auto& pts = kv.second.points();

        const auto&  xs = pts.getPointsBufferRef_x();
        const auto&  ys = pts.getPointsBufferRef_y();
        const auto&  zs = pts.getPointsBufferRef_z();
        const size_t N  = xs.size();

        for (size_t i = 0; i < N; i++)  //
            f({xs[i], ys[i], zs[i]});
    }
}

void SparseTreesPointCloud::visitAllGrids(
    const std::function<void(const outer_index3d_t&, const GridData&)>& f) const
{
    for (const auto& kv : grids_)
    {
        const outer_index3d_t outer_idx = kv.first;
        f(outer_idx, kv.second);
    }
}

// ========== Option structures ==========
void SparseTreesPointCloud::TInsertionOptions::writeToStream(
    mrpt::serialization::CArchive& out) const
{
    const int8_t version = 0;
    out << version;

    out << minimum_points_clearance << remove_submaps_farther_than;
}

void SparseTreesPointCloud::TInsertionOptions::readFromStream(
    mrpt::serialization::CArchive& in)
{
    int8_t version;
    in >> version;
    switch (version)
    {
        case 0:
        {
            in >> minimum_points_clearance >> remove_submaps_farther_than;
        }
        break;
        default:
            MRPT_THROW_UNKNOWN_SERIALIZATION_VERSION(version);
    }
}

void SparseTreesPointCloud::TLikelihoodOptions::writeToStream(
    mrpt::serialization::CArchive& out) const
{
    const int8_t version = 0;
    out << version;
    out << sigma_dist << max_corr_distance << decimation;
}

void SparseTreesPointCloud::TLikelihoodOptions::readFromStream(
    mrpt::serialization::CArchive& in)
{
    int8_t version;
    in >> version;
    switch (version)
    {
        case 0:
        {
            in >> sigma_dist >> max_corr_distance >> decimation;
        }
        break;
        default:
            MRPT_THROW_UNKNOWN_SERIALIZATION_VERSION(version);
    }
}

void SparseTreesPointCloud::TRenderOptions::writeToStream(
    mrpt::serialization::CArchive& out) const
{
    const int8_t version = 0;
    out << version;
    out << point_size << show_inner_grid_boxes << color << int8_t(colormap)
        << recolorizeByCoordinateIndex;
}

void SparseTreesPointCloud::TRenderOptions::readFromStream(
    mrpt::serialization::CArchive& in)
{
    int8_t version;
    in >> version;
    switch (version)
    {
        case 0:
        {
            in >> point_size >> show_inner_grid_boxes;
            in >> this->color;
            in.ReadAsAndCastTo<int8_t>(this->colormap);
            in >> recolorizeByCoordinateIndex;
        }
        break;
        default:
            MRPT_THROW_UNKNOWN_SERIALIZATION_VERSION(version);
    }
}

void SparseTreesPointCloud::TInsertionOptions::dumpToTextStream(
    std::ostream& out) const
{
    out << "\n------ [SparseTreesPointCloud::TInsertionOptions] "
           "------- "
           "\n\n";

    LOADABLEOPTS_DUMP_VAR(minimum_points_clearance, double);
    LOADABLEOPTS_DUMP_VAR(remove_submaps_farther_than, int);
}

void SparseTreesPointCloud::TLikelihoodOptions::dumpToTextStream(
    std::ostream& out) const
{
    out << "\n------ [SparseTreesPointCloud::TLikelihoodOptions] "
           "------- "
           "\n\n";

    LOADABLEOPTS_DUMP_VAR(sigma_dist, double);
    LOADABLEOPTS_DUMP_VAR(max_corr_distance, double);
    LOADABLEOPTS_DUMP_VAR(decimation, int);
}

void SparseTreesPointCloud::TRenderOptions::dumpToTextStream(
    std::ostream& out) const
{
    out << "\n------ [SparseTreesPointCloud::TRenderOptions] ------- "
           "\n\n";

    LOADABLEOPTS_DUMP_VAR(point_size, float);
    LOADABLEOPTS_DUMP_VAR(show_inner_grid_boxes, bool);
    LOADABLEOPTS_DUMP_VAR(color.R, float);
    LOADABLEOPTS_DUMP_VAR(color.G, float);
    LOADABLEOPTS_DUMP_VAR(color.B, float);
    LOADABLEOPTS_DUMP_VAR(colormap, int);
    LOADABLEOPTS_DUMP_VAR(recolorizeByCoordinateIndex, int);
}

void SparseTreesPointCloud::TInsertionOptions::loadFromConfigFile(
    [[maybe_unused]] const mrpt::config::CConfigFileBase& c,
    [[maybe_unused]] const std::string&                   s)
{
    MRPT_LOAD_CONFIG_VAR(minimum_points_clearance, double, c, s);
    MRPT_LOAD_CONFIG_VAR(remove_submaps_farther_than, uint64_t, c, s);
}

void SparseTreesPointCloud::TLikelihoodOptions::loadFromConfigFile(
    const mrpt::config::CConfigFileBase& c, const std::string& s)
{
    MRPT_LOAD_CONFIG_VAR(sigma_dist, double, c, s);
    MRPT_LOAD_CONFIG_VAR(max_corr_distance, double, c, s);
    MRPT_LOAD_CONFIG_VAR(decimation, int, c, s);
}

void SparseTreesPointCloud::TRenderOptions::loadFromConfigFile(
    const mrpt::config::CConfigFileBase& c, const std::string& s)
{
    MRPT_LOAD_CONFIG_VAR(point_size, float, c, s);
    MRPT_LOAD_CONFIG_VAR(show_inner_grid_boxes, bool, c, s);
    MRPT_LOAD_CONFIG_VAR(color.R, float, c, s);
    MRPT_LOAD_CONFIG_VAR(color.G, float, c, s);
    MRPT_LOAD_CONFIG_VAR(color.B, float, c, s);
    colormap = c.read_enum(s, "colormap", this->colormap);
    MRPT_LOAD_CONFIG_VAR(recolorizeByCoordinateIndex, int, c, s);
}

void SparseTreesPointCloud::internal_insertPointCloud3D(
    const mrpt::poses::CPose3D& pc_in_map, const float* xs, const float* ys,
    const float* zs, const std::size_t num_pts)
{
    MRPT_TRY_START

    if (!num_pts) return;

    // Make a temporary buffer for transformed point cloud.
    // I'll do it in the stack to save alloc & free time:
    float*   gXs = reinterpret_cast<float*>(::alloca(sizeof(float) * num_pts));
    float*   gYs = reinterpret_cast<float*>(::alloca(sizeof(float) * num_pts));
    float*   gZs = reinterpret_cast<float*>(::alloca(sizeof(float) * num_pts));
    uint8_t* doInsert =
        reinterpret_cast<uint8_t*>(::alloca(sizeof(uint8_t) * num_pts));

    const float minSqrDist =
        mrpt::square(insertionOptions.minimum_points_clearance);

    for (std::size_t i = 0; i < num_pts; i++)
    {
        // Transform the point from the scan reference to its global 3D
        // position:
        const auto gPt = pc_in_map.composePoint({xs[i], ys[i], zs[i]});
        gXs[i]         = gPt.x;
        gYs[i]         = gPt.y;
        gZs[i]         = gPt.z;

        // check for closest existing point:
        mrpt::math::TPoint3Df neig;
        float                 nnSqrDist;
        uint64_t              nnId;
        bool found = nn_single_search(gPt, neig, nnSqrDist, nnId);

        doInsert[i] = (!found || nnSqrDist > minSqrDist) ? 1 : 0;
    }

    // Insert *after* the loop above, to prevent having to rebuild the
    // KD-Tree "N" times (!!!)
    for (std::size_t i = 0; i < num_pts; i++)
        if (doInsert[i]) insertPoint({gXs[i], gYs[i], gZs[i]});

    MRPT_TRY_END
}

void SparseTreesPointCloud::eraseGridsFartherThan(
    const mrpt::math::TPoint3Df& pt, const float distanceMeters)
{
    const auto curIdxs = coordToOuterIdx(pt);

    const int d = static_cast<int>(std::ceil(distanceMeters * grid_size_inv_));

    const auto curIdxs0 = curIdxs - outer_index3d_t(d, d, d);
    const auto curIdxs1 = curIdxs + outer_index3d_t(d, d, d);

    std::set<outer_index3d_t, index3d_hash<int32_t>> gridsToRemove;

    auto lmbPerGrid = [&](const outer_index3d_t& idx, const GridData&) {
        if (idx.cx >= curIdxs0.cx && idx.cy >= curIdxs0.cy &&
            idx.cz >= curIdxs0.cz && idx.cx <= curIdxs1.cx &&
            idx.cy <= curIdxs1.cy && idx.cz <= curIdxs1.cz)
            return;
        gridsToRemove.insert(idx);
    };
    visitAllGrids(lmbPerGrid);

    // Remove the grid blocks from the local map, and also from the
    // access cache:
    cachedMtx_.lock();

    for (auto& idx : gridsToRemove)
    {
        this->grids_.erase(idx);

        for (int i = 0; i < CachedData::NUM_CACHED_IDXS; i++)
        {
            if (cached_.lastAccessIdx[i] == idx)
            {
                cached_.lastAccessIdx[i]  = {0, 0, 0};
                cached_.lastAccessGrid[i] = nullptr;
            }
        }
    }
    cachedMtx_.unlock();
}

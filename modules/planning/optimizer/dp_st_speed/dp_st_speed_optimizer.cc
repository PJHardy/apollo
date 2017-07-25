/******************************************************************************
 * Copyright 2017 The Apollo Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *****************************************************************************/

/**
 * @file dp_st_speed_optimizer.cc
 **/

#include "modules/planning/optimizer/dp_st_speed/dp_st_speed_optimizer.h"

#include <vector>

#include "modules/common/configs/vehicle_config_helper.h"
#include "modules/planning/optimizer/dp_st_speed/dp_st_boundary_mapper.h"
#include "modules/planning/optimizer/dp_st_speed/dp_st_configuration.h"
#include "modules/planning/optimizer/dp_st_speed/dp_st_graph.h"
#include "modules/planning/optimizer/st_graph/st_graph_data.h"
#include "modules/common/adapters/adapter_manager.h"

namespace apollo {
namespace planning {

using apollo::common::ErrorCode;
using apollo::common::Status;
using apollo::common::config::VehicleConfigHelper;
using ::apollo::common::adapter::AdapterManager;
using ::apollo::localization::LocalizationEstimate;

DpStSpeedOptimizer::DpStSpeedOptimizer(const std::string& name)
    : SpeedOptimizer(name) {}

Status DpStSpeedOptimizer::Process(const PathData& path_data,
                                   const TrajectoryPoint& init_point,
                                   DecisionData* const decision_data,
                                   SpeedData* const speed_data) const {
  ::apollo::common::config::VehicleParam veh_param =
      VehicleConfigHelper::GetConfig().vehicle_param();

  // TODO: load boundary mapper and st graph configuration
  StBoundaryConfig st_boundary_config;
  DpStConfiguration dp_st_speed_config;
  //TODO(yifei) will change to pb later
  //DpStSpeedConfig dp_st_speed_config;
  //if (!common::util::GetProtoFromFile(FLAGS_dp_st_speed_config_file,
  //                                    &dp_st_speed_config)) {
  //  AERROR << "failed to load config file " << FLAGS_dp_st_speed_config_file;
  //  return Status(ErrorCode::PLANNING_ERROR_FAILED,
  //                "failed to load config file");
  //}

  // step 1 get boundaries
  DpStBoundaryMapper st_mapper(st_boundary_config, veh_param);
  double planning_distance = std::min(dp_st_speed_config.total_path_length(),
                                      path_data.path().param_length());

  std::vector<StGraphBoundary> boundaries;

  common::TrajectoryPoint initial_planning_point = DataCenter::instance()
                                                       ->current_frame()
                                                       ->mutable_planning_data()
                                                       ->init_planning_point();
  if (!st_mapper
           .get_graph_boundary(initial_planning_point, *decision_data,
                               path_data,
                               dp_st_speed_config.total_path_length(),
                               dp_st_speed_config.total_time(), &boundaries)
           .ok()) {
    const std::string msg =
        "Mapping obstacle for dp st speed optimizer failed.";
    AERROR << msg;
    return Status(ErrorCode::PLANNING_ERROR, msg);
  }

  // step 2 perform graph search
  SpeedLimit speed_limit;
  auto localization_adapter = AdapterManager::GetLocalization();
  if (localization_adapter->Empty()) {
    AINFO << "No Localization msg yet. ";
    return Status(ErrorCode::PLANNING_ERROR_FAILED, "No localization msg");
  }
  LocalizationEstimate localization = localization_adapter->GetLatestObserved();

  if (st_mapper.get_speed_limits( localization.pose(),
      DataCenter::instance()->map(),
      path_data,
      planning_distance,
      dp_st_speed_config.matrix_dimension_s(),
      dp_st_speed_config.max_speed(),
      &speed_limit) != Status::OK()) {
    AERROR << "Getting speed limits for dp st speed optimizer failed!";
    return Status(ErrorCode::PLANNING_ERROR_FAILED,
                  "Getting speed limits for dp st speed optimizer failed!");

  }

  StGraphData st_graph_data(boundaries, init_point, speed_limit,
                            path_data.path().param_length());

  DpStGraph st_graph(dp_st_speed_config, veh_param);

  if (!st_graph.search(st_graph_data, decision_data, speed_data).ok()) {
    const std::string msg = "Failed to search graph with dynamic programming.";
    AERROR << msg;
    return Status(ErrorCode::PLANNING_ERROR, msg);
  }

  return Status::OK();
}

}  // namespace planning
}  // namespace apollo

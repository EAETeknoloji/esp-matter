// Copyright 2022 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <esp_log.h>
#include <esp_matter.h>
#include <esp_matter_core.h>

#include <app/clusters/bindings/BindingManager.h>
#include <zap-generated/CHIPClusters.h>

using chip::Callback::Callback;
using chip::DeviceProxy;
using chip::FabricInfo;
using chip::kInvalidEndpointId;
using chip::OperationalDeviceProxy;
using chip::PeerId;

static const char *TAG = "esp_matter_client";

static esp_matter_client_command_callback_t client_command_callback = NULL;
static void *client_command_callback_priv_data = NULL;

esp_err_t esp_matter_set_client_command_callback(esp_matter_client_command_callback_t callback, void *priv_data)
{
    client_command_callback = callback;
    client_command_callback_priv_data = priv_data;
    return ESP_OK;
}

/** TODO: Change g_remote_endpoint_id to something better. */
int g_remote_endpoint_id = kInvalidEndpointId;
void esp_matter_new_connection_success_callback(void *context, OperationalDeviceProxy *peer_device)
{
    ESP_LOGI(TAG, "New connection success");
    if (client_command_callback) {
        client_command_callback(peer_device, g_remote_endpoint_id, client_command_callback_priv_data);
    }
}

void esp_matter_new_connection_failure_callback(void *context, PeerId peerId, CHIP_ERROR error)
{
    ESP_LOGI(TAG, "New connection failure");
}

esp_err_t esp_matter_connect(int fabric_index, int node_id, int remote_endpoint_id)
{
    /* Get info */
    FabricInfo *fabric_info = chip::Server::GetInstance().GetFabricTable().FindFabricWithIndex(fabric_index);
    PeerId peer_id = fabric_info->GetPeerIdForNode(node_id);

    /* Find existing */
    DeviceProxy *peer_device = chip::Server::GetInstance().GetCASESessionManager()->FindExistingSession(peer_id);
    if (peer_device) {
        /* Callback if found */
        if (client_command_callback) {
            client_command_callback(peer_device, remote_endpoint_id, client_command_callback_priv_data);
        }
        return ESP_OK;
    }

    /* Create new connection */
    g_remote_endpoint_id = remote_endpoint_id;
    static Callback<chip::OnDeviceConnected> success_callback(esp_matter_new_connection_success_callback, NULL);
    static Callback<chip::OnDeviceConnectionFailure> failure_callback(esp_matter_new_connection_failure_callback, NULL);
    chip::Server::GetInstance().GetCASESessionManager()->FindOrEstablishSession(peer_id, &success_callback,
                                                                                &failure_callback);

    return ESP_OK;
}

static void esp_matter_command_client_binding_callback(const EmberBindingTableEntry *binding, DeviceProxy *peer_device,
                                                       void *context)
{
    if (client_command_callback) {
        client_command_callback(peer_device, binding->remote, client_command_callback_priv_data);
    }
}

esp_err_t esp_matter_client_cluster_update(int endpoint_id, int cluster_id)
{
    chip::BindingManager::GetInstance().NotifyBoundClusterChanged(endpoint_id, cluster_id, NULL);
    return ESP_OK;
}

void esp_matter_binding_manager_init()
{
    static bool init_done = false;
    if (init_done) {
        return;
    }
    chip::BindingManager::GetInstance().SetAppServer(&chip::Server::GetInstance());
    chip::BindingManager::GetInstance().RegisterBoundDeviceChangedHandler(esp_matter_command_client_binding_callback);
    init_done = true;
}

static void esp_matter_send_command_success_callback(void *context, const chip::app::DataModel::NullObjectType &data)
{
    ESP_LOGI(TAG, "Send command success");
}

static void esp_matter_send_command_failure_callback(void *context, CHIP_ERROR error)
{
    ESP_LOGI(TAG, "FSend command failure");
}

esp_err_t esp_matter_on_off_send_command_on(esp_matter_peer_device_t *remote_device, int remote_endpoint_id)
{
    chip::Controller::OnOffCluster cluster;
    chip::app::Clusters::OnOff::Commands::On::Type command_data;

    cluster.Associate(remote_device, remote_endpoint_id);
    cluster.InvokeCommand(command_data, NULL, esp_matter_send_command_success_callback,
                          esp_matter_send_command_failure_callback);
    return ESP_OK;
}

esp_err_t esp_matter_on_off_send_command_off(esp_matter_peer_device_t *remote_device, int remote_endpoint_id)
{
    chip::Controller::OnOffCluster cluster;
    chip::app::Clusters::OnOff::Commands::Off::Type command_data;

    cluster.Associate(remote_device, remote_endpoint_id);
    cluster.InvokeCommand(command_data, NULL, esp_matter_send_command_success_callback,
                          esp_matter_send_command_failure_callback);
    return ESP_OK;
}

esp_err_t esp_matter_on_off_send_command_toggle(esp_matter_peer_device_t *remote_device, int remote_endpoint_id)
{
    chip::Controller::OnOffCluster cluster;
    chip::app::Clusters::OnOff::Commands::Toggle::Type command_data;

    cluster.Associate(remote_device, remote_endpoint_id);
    cluster.InvokeCommand(command_data, NULL, esp_matter_send_command_success_callback,
                          esp_matter_send_command_failure_callback);
    return ESP_OK;
}

esp_err_t esp_matter_level_control_send_command_move(esp_matter_peer_device_t *remote_device, int remote_endpoint_id,
                                                     uint8_t move_mode, uint8_t rate, uint8_t option_mask,
                                                     uint8_t option_override)
{
    chip::Controller::LevelControlCluster cluster;
    chip::app::Clusters::LevelControl::Commands::Move::Type command_data;
    command_data.moveMode = (chip::app::Clusters::LevelControl::MoveMode)move_mode;
    command_data.rate = rate;
    command_data.optionMask = option_mask;
    command_data.optionOverride = option_override;

    cluster.Associate(remote_device, remote_endpoint_id);
    cluster.InvokeCommand(command_data, NULL, esp_matter_send_command_success_callback,
                          esp_matter_send_command_failure_callback);
    return ESP_OK;
}

esp_err_t esp_matter_level_control_send_command_move_to_level(esp_matter_peer_device_t *remote_device,
                                                              int remote_endpoint_id, uint8_t level,
                                                              uint16_t transition_time, uint8_t option_mask,
                                                              uint8_t option_override)
{
    chip::Controller::LevelControlCluster cluster;
    chip::app::Clusters::LevelControl::Commands::MoveToLevel::Type command_data;
    command_data.level = level;
    command_data.transitionTime = transition_time;
    command_data.optionMask = option_mask;
    command_data.optionOverride = option_override;

    cluster.Associate(remote_device, remote_endpoint_id);
    cluster.InvokeCommand(command_data, NULL, esp_matter_send_command_success_callback,
                          esp_matter_send_command_failure_callback);
    return ESP_OK;
}

esp_err_t esp_matter_level_control_send_command_move_to_level_with_on_off(esp_matter_peer_device_t *remote_device,
                                                                          int remote_endpoint_id, uint8_t level,
                                                                          uint16_t transition_time)
{
    chip::Controller::LevelControlCluster cluster;
    chip::app::Clusters::LevelControl::Commands::MoveToLevelWithOnOff::Type command_data;
    command_data.level = level;
    command_data.transitionTime = transition_time;

    cluster.Associate(remote_device, remote_endpoint_id);
    cluster.InvokeCommand(command_data, NULL, esp_matter_send_command_success_callback,
                          esp_matter_send_command_failure_callback);
    return ESP_OK;
}

esp_err_t esp_matter_level_control_send_command_move_with_on_off(esp_matter_peer_device_t *remote_device,
                                                                 int remote_endpoint_id, uint8_t move_mode,
                                                                 uint8_t rate)
{
    chip::Controller::LevelControlCluster cluster;
    chip::app::Clusters::LevelControl::Commands::MoveWithOnOff::Type command_data;
    command_data.moveMode = (chip::app::Clusters::LevelControl::MoveMode)move_mode;
    command_data.rate = rate;

    cluster.Associate(remote_device, remote_endpoint_id);
    cluster.InvokeCommand(command_data, NULL, esp_matter_send_command_success_callback,
                          esp_matter_send_command_failure_callback);
    return ESP_OK;
}

esp_err_t esp_matter_level_control_send_command_step(esp_matter_peer_device_t *remote_device, int remote_endpoint_id,
                                                     uint8_t step_mode, uint8_t step_size, uint16_t transition_time,
                                                     uint8_t option_mask, uint8_t option_override)
{
    chip::Controller::LevelControlCluster cluster;
    chip::app::Clusters::LevelControl::Commands::Step::Type command_data;
    command_data.stepMode = (chip::app::Clusters::LevelControl::StepMode)step_mode;
    command_data.stepSize = step_size;
    command_data.transitionTime = transition_time;
    command_data.optionMask = option_mask;
    command_data.optionOverride = option_override;

    cluster.Associate(remote_device, remote_endpoint_id);
    cluster.InvokeCommand(command_data, NULL, esp_matter_send_command_success_callback,
                          esp_matter_send_command_failure_callback);
    return ESP_OK;
}

esp_err_t esp_matter_level_control_send_command_step_with_on_off(esp_matter_peer_device_t *remote_device,
                                                                 int remote_endpoint_id, uint8_t step_mode,
                                                                 uint8_t step_size, uint16_t transition_time)
{
    chip::Controller::LevelControlCluster cluster;
    chip::app::Clusters::LevelControl::Commands::StepWithOnOff::Type command_data;
    command_data.stepMode = (chip::app::Clusters::LevelControl::StepMode)step_mode;
    command_data.stepSize = step_size;
    command_data.transitionTime = transition_time;

    cluster.Associate(remote_device, remote_endpoint_id);
    cluster.InvokeCommand(command_data, NULL, esp_matter_send_command_success_callback,
                          esp_matter_send_command_failure_callback);
    return ESP_OK;
}

esp_err_t esp_matter_level_control_send_command_stop(esp_matter_peer_device_t *remote_device, int remote_endpoint_id,
                                                     uint8_t option_mask, uint8_t option_override)
{
    chip::Controller::LevelControlCluster cluster;
    chip::app::Clusters::LevelControl::Commands::Stop::Type command_data;
    command_data.optionMask = option_mask;
    command_data.optionOverride = option_override;

    cluster.Associate(remote_device, remote_endpoint_id);
    cluster.InvokeCommand(command_data, NULL, esp_matter_send_command_success_callback,
                          esp_matter_send_command_failure_callback);
    return ESP_OK;
}

esp_err_t esp_matter_level_control_send_command_stop_with_on_off(esp_matter_peer_device_t *remote_device,
                                                                 int remote_endpoint_id)
{
    chip::Controller::LevelControlCluster cluster;
    chip::app::Clusters::LevelControl::Commands::Stop::Type command_data;

    cluster.Associate(remote_device, remote_endpoint_id);
    cluster.InvokeCommand(command_data, NULL, esp_matter_send_command_success_callback,
                          esp_matter_send_command_failure_callback);
    return ESP_OK;
}

esp_err_t esp_matter_color_control_send_command_move_hue(esp_matter_peer_device_t *remote_device,
                                                         int remote_endpoint_id, uint8_t move_mode, uint8_t rate,
                                                         uint8_t option_mask, uint8_t option_override)
{
    chip::Controller::ColorControlCluster cluster;
    chip::app::Clusters::ColorControl::Commands::MoveHue::Type command_data;
    command_data.moveMode = (chip::app::Clusters::ColorControl::HueMoveMode)move_mode;
    command_data.rate = rate;
    command_data.optionsMask = option_mask;
    command_data.optionsOverride = option_override;

    cluster.Associate(remote_device, remote_endpoint_id);
    cluster.InvokeCommand(command_data, NULL, esp_matter_send_command_success_callback,
                          esp_matter_send_command_failure_callback);
    return ESP_OK;
}

esp_err_t esp_matter_color_control_send_command_move_saturation(esp_matter_peer_device_t *remote_device,
                                                                int remote_endpoint_id, uint8_t move_mode, uint8_t rate,
                                                                uint8_t option_mask, uint8_t option_override)
{
    chip::Controller::ColorControlCluster cluster;
    chip::app::Clusters::ColorControl::Commands::MoveSaturation::Type command_data;
    command_data.moveMode = (chip::app::Clusters::ColorControl::SaturationMoveMode)move_mode;
    command_data.rate = rate;
    command_data.optionsMask = option_mask;
    command_data.optionsOverride = option_override;

    cluster.Associate(remote_device, remote_endpoint_id);
    cluster.InvokeCommand(command_data, NULL, esp_matter_send_command_success_callback,
                          esp_matter_send_command_failure_callback);
    return ESP_OK;
}

esp_err_t esp_matter_color_control_send_command_move_to_hue(esp_matter_peer_device_t *remote_device,
                                                            int remote_endpoint_id, uint8_t hue, uint8_t direction,
                                                            uint16_t transition_time, uint8_t option_mask,
                                                            uint8_t option_override)
{
    chip::Controller::ColorControlCluster cluster;
    chip::app::Clusters::ColorControl::Commands::MoveToHue::Type command_data;
    command_data.hue = hue;
    command_data.direction = (chip::app::Clusters::ColorControl::HueDirection)direction;
    command_data.transitionTime = transition_time;
    command_data.optionsMask = option_mask;
    command_data.optionsOverride = option_override;

    cluster.Associate(remote_device, remote_endpoint_id);
    cluster.InvokeCommand(command_data, NULL, esp_matter_send_command_success_callback,
                          esp_matter_send_command_failure_callback);
    return ESP_OK;
}

esp_err_t esp_matter_color_control_send_command_move_to_hue_and_saturation(esp_matter_peer_device_t *remote_device,
                                                                        int remote_endpoint_id, uint8_t hue,
                                                                        uint8_t saturation, uint16_t transition_time,
                                                                        uint8_t option_mask, uint8_t option_override)
{
    chip::Controller::ColorControlCluster cluster;
    chip::app::Clusters::ColorControl::Commands::MoveToHueAndSaturation::Type command_data;
    command_data.hue = hue;
    command_data.saturation = saturation;
    command_data.transitionTime = transition_time;
    command_data.optionsMask = option_mask;
    command_data.optionsOverride = option_override;

    cluster.Associate(remote_device, remote_endpoint_id);
    cluster.InvokeCommand(command_data, NULL, esp_matter_send_command_success_callback,
                          esp_matter_send_command_failure_callback);
    return ESP_OK;
}

esp_err_t esp_matter_color_control_send_command_move_to_saturation(esp_matter_peer_device_t *remote_device,
                                                                   int remote_endpoint_id, uint8_t saturation,
                                                                   uint16_t transition_time, uint8_t option_mask,
                                                                   uint8_t option_override)
{
    chip::Controller::ColorControlCluster cluster;
    chip::app::Clusters::ColorControl::Commands::MoveToSaturation::Type command_data;
    command_data.saturation = saturation;
    command_data.transitionTime = transition_time;
    command_data.optionsMask = option_mask;
    command_data.optionsOverride = option_override;

    cluster.Associate(remote_device, remote_endpoint_id);
    cluster.InvokeCommand(command_data, NULL, esp_matter_send_command_success_callback,
                          esp_matter_send_command_failure_callback);
    return ESP_OK;
}

esp_err_t esp_matter_color_control_send_command_step_hue(esp_matter_peer_device_t *remote_device,
                                                         int remote_endpoint_id, uint8_t step_mode, uint8_t step_size,
                                                         uint16_t transition_time, uint8_t option_mask,
                                                         uint8_t option_override)
{
    chip::Controller::ColorControlCluster cluster;
    chip::app::Clusters::ColorControl::Commands::StepHue::Type command_data;
    command_data.stepMode = (chip::app::Clusters::ColorControl::HueStepMode)step_mode;
    command_data.stepSize = step_size;
    command_data.transitionTime = transition_time;
    command_data.optionsMask = option_mask;
    command_data.optionsOverride = option_override;

    cluster.Associate(remote_device, remote_endpoint_id);
    cluster.InvokeCommand(command_data, NULL, esp_matter_send_command_success_callback,
                          esp_matter_send_command_failure_callback);
    return ESP_OK;
}

esp_err_t esp_matter_color_control_send_command_step_saturation(esp_matter_peer_device_t *remote_device,
                                                                int remote_endpoint_id, uint8_t step_mode,
                                                                uint8_t step_size, uint16_t transition_time,
                                                                uint8_t option_mask, uint8_t option_override)
{
    chip::Controller::ColorControlCluster cluster;
    chip::app::Clusters::ColorControl::Commands::StepSaturation::Type command_data;
    command_data.stepMode = (chip::app::Clusters::ColorControl::SaturationStepMode)step_mode;
    command_data.stepSize = step_size;
    command_data.transitionTime = transition_time;
    command_data.optionsMask = option_mask;
    command_data.optionsOverride = option_override;

    cluster.Associate(remote_device, remote_endpoint_id);
    cluster.InvokeCommand(command_data, NULL, esp_matter_send_command_success_callback,
                          esp_matter_send_command_failure_callback);
    return ESP_OK;
}

//#ifdef USE_ARDUINO

#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "tcl_climate.h"
#include <map>  // Add this include
#include <set>

namespace esphome {
namespace tcl_climate {

// Use constexpr for compile-time constants
static constexpr uint8_t REQ_CMD[] = {0xBB, 0x00, 0x01, 0x04, 0x02, 0x01, 0x00, 0xBD};
static constexpr int MAX_LINE_LENGTH = 100;
static constexpr int UPDATE_INTERVAL_MS = 450;

//bool skip_next_update = false;

void TCLClimate::set_current_temperature(float current_temperature) {
  if (std::abs(this->current_temperature - current_temperature) < 0.01f) return; //no change
  this->is_changed = true;
  this->current_temperature = current_temperature;

  ESP_LOGD("TCL", "Current temperature reading updated to: %.1f°C", current_temperature); // Log current temperature change
}

void TCLClimate::set_custom_fan_mode(StringRef fan_mode) {
  StringRef current(this->get_custom_fan_mode());
  if (!current.empty() && fan_mode == current.c_str())  return;  //no change
  this->is_changed = true;
  this->set_custom_fan_mode_(fan_mode.c_str());
  
  ESP_LOGI("TCL", "Fan mode changed to: %s", fan_mode.c_str());   // Log the fan mode change
}

void TCLClimate::set_mode(climate::ClimateMode mode) {
  if (this->mode == mode) return;
  this->is_changed = true;
  this->mode = mode;

  // Log the HVAC mode change
  const char* mode_str = "";
  switch (mode) {
    case climate::CLIMATE_MODE_OFF: mode_str = "OFF"; break;
    case climate::CLIMATE_MODE_COOL: mode_str = "COOL"; break;
    case climate::CLIMATE_MODE_HEAT: mode_str = "HEAT"; break;
    case climate::CLIMATE_MODE_FAN_ONLY: mode_str = "FAN ONLY"; break;
    case climate::CLIMATE_MODE_DRY: mode_str = "DRY"; break;
    case climate::CLIMATE_MODE_AUTO: mode_str = "AUTO"; break;
    default: mode_str = "UNKNOWN"; break;
  }
  ESP_LOGI("TCL", "HVAC mode changed to: %s", mode_str);
  
}

void TCLClimate::set_swing_mode(climate::ClimateSwingMode swing_mode) {
  if (this->swing_mode == swing_mode) return;
  this->is_changed = true;
  this->swing_mode = swing_mode;

  const char* swing_str = "";
  switch (swing_mode) {
    case climate::CLIMATE_SWING_OFF: swing_str = "OFF"; break;
    case climate::CLIMATE_SWING_BOTH: swing_str = "BOTH"; break;
    case climate::CLIMATE_SWING_VERTICAL: swing_str = "VERTICAL"; break;
    case climate::CLIMATE_SWING_HORIZONTAL: swing_str = "HORIZONTAL"; break;
    default: swing_str = "UNKNOWN"; break;
  }
  ESP_LOGI("TCL", "Swing mode changed to: %s", swing_str);
  
}

void TCLClimate::set_hswing_pos(const std::string &hswing_pos) {
  if (this->hswing_pos == hswing_pos) return;
  this->hswing_pos = hswing_pos;

  ESP_LOGI("TCL", "Horizontal swing position: %s", hswing_pos.c_str());  
}

void TCLClimate::set_vswing_pos(const std::string &vswing_pos) {
  if (this->vswing_pos == vswing_pos) return;
  this->vswing_pos = vswing_pos;
  
  ESP_LOGI("TCL", "Vertical swing position: %s", vswing_pos.c_str());
}

void TCLClimate::set_target_temperature(float target_temperature) {
  if (std::abs(this->target_temperature - target_temperature) < 0.01f) return; // no change
  this->is_changed = true;
  this->target_temperature = target_temperature;

  ESP_LOGI("TCL", "Target temperature changed to: %.1f°C", target_temperature);  // Log temperature change
}

void TCLClimate::build_set_cmd(desired_ac_status_t *desired_ac_status) {
    memcpy(outgoing_tx_command.raw, outgoing_tx_cmd_template, sizeof(outgoing_tx_command.raw));

    outgoing_tx_command.data.power = desired_ac_status->data.power;
    outgoing_tx_command.data.off_timer_en = 0; // not implemented
    outgoing_tx_command.data.on_timer_en = 0;  // not implemented
    outgoing_tx_command.data.beep = beep_on ? 1 : 0;
    outgoing_tx_command.data.disp = display_on ? 1 : 0;
    outgoing_tx_command.data.eco = desired_ac_status->data.eco;
    outgoing_tx_command.data.turbo = desired_ac_status->data.turbo;
    outgoing_tx_command.data.mute = desired_ac_status->data.mute;

    // Mode mapping using lookup table
    static constexpr uint8_t MODE_MAP[] = {
        0x00, // 0x00 - unused
        0x03, // 0x01 -> 0x03
        0x07, // 0x02 -> 0x07 (fan only)
        0x02, // 0x03 -> 0x02 (dry)
        0x01, // 0x04 -> 0x01 (heat)
        0x08  // 0x05 -> 0x08 (auto)
    };

    if (desired_ac_status->data.mode < sizeof(MODE_MAP)) {
        outgoing_tx_command.data.mode = MODE_MAP[desired_ac_status->data.mode];
    }

    // Temperature conversion
    outgoing_tx_command.data.temp = 15 - desired_ac_status->data.temp;

    // Fan mode mapping using lookup table
    static constexpr uint8_t FAN_MAP[] = {
        0x00, // 0x00 -> 0x00 (auto)
        0x02, // 0x01 -> 0x02 (speed 1)
        0x03, // 0x02 -> 0x03 (speed 3)
        0x05, // 0x03 -> 0x05 (speed 5)
        0x06, // 0x04 -> 0x06 (speed 2)
        0x07  // 0x05 -> 0x07 (speed 4)
    };

    if (desired_ac_status->data.fan < sizeof(FAN_MAP)) {
        outgoing_tx_command.data.fan = FAN_MAP[desired_ac_status->data.fan];
    }

    // Swing control - extracted from old code
    if (desired_ac_status->data.vswing_mv) {
      outgoing_tx_command.data.vswing = 0x07;
      outgoing_tx_command.data.vswing_fix = 0;
      outgoing_tx_command.data.vswing_mv = desired_ac_status->data.vswing_mv;
    } else if (desired_ac_status->data.vswing_fix) {
      outgoing_tx_command.data.vswing = 0;
      outgoing_tx_command.data.vswing_fix = desired_ac_status->data.vswing_fix;
      outgoing_tx_command.data.vswing_mv = 0;
    }

    if (desired_ac_status->data.hswing_mv) {
      outgoing_tx_command.data.hswing = 0x01;
      outgoing_tx_command.data.hswing_fix = 0;
      outgoing_tx_command.data.hswing_mv = desired_ac_status->data.hswing_mv;
    } else if (desired_ac_status->data.hswing_fix) {
      outgoing_tx_command.data.hswing = 0;
      outgoing_tx_command.data.hswing_fix = desired_ac_status->data.hswing_fix;
      outgoing_tx_command.data.hswing_mv = 0;
    }

    outgoing_tx_command.data.half_degree = 0;  // not implemented

    // Calculate XOR checksum
    uint8_t xor_byte = 0;
    for (size_t i = 0; i < sizeof(outgoing_tx_command.raw) - 1; i++) {
        xor_byte ^= outgoing_tx_command.raw[i];
    }
    outgoing_tx_command.raw[sizeof(outgoing_tx_command.raw) - 1] = xor_byte;
}

void TCLClimate::setup() {
  set_update_interval(UPDATE_INTERVAL_MS);
  this->set_supported_custom_fan_modes({"Turbo", "Mute", "Automatic", "1", "2", "3", "4", "5"});
}

// Swing control methods from old code
void TCLClimate::control_vertical_swing(const std::string &swing_mode) {

  desired_ac_status_t desired_ac_status = {0};
  memcpy(desired_ac_status.raw, last_ac_status.raw, sizeof(desired_ac_status.raw));

  desired_ac_status.data.vswing_mv = 0;
  desired_ac_status.data.vswing_fix = 0;

  if (swing_mode == "Move full") desired_ac_status.data.vswing_mv = 0x01;
  else if (swing_mode == "Move upper")  desired_ac_status.data.vswing_mv = 0x02;
  else if (swing_mode == "Move lower")  desired_ac_status.data.vswing_mv = 0x03;
  else if (swing_mode == "Fix top") desired_ac_status.data.vswing_fix = 0x01;
  else if (swing_mode == "Fix upper") desired_ac_status.data.vswing_fix = 0x02;
  else if (swing_mode == "Fix mid") desired_ac_status.data.vswing_fix = 0x03;
  else if (swing_mode == "Fix lower") desired_ac_status.data.vswing_fix = 0x04;
  else if (swing_mode == "Fix bottom") desired_ac_status.data.vswing_fix = 0x05;

  if (desired_ac_status.data.vswing_mv) desired_ac_status.data.vswing = 0x01;
  else desired_ac_status.data.vswing = 0;

  build_set_cmd(&desired_ac_status);
  ready_to_send_set_cmd_flag = true;
}

void TCLClimate::control_horizontal_swing(const std::string &swing_mode) {

  desired_ac_status_t desired_ac_status = {0};
  memcpy(desired_ac_status.raw, last_ac_status.raw, sizeof(desired_ac_status.raw));

  desired_ac_status.data.hswing_mv = 0;
  desired_ac_status.data.hswing_fix = 0;

  if (swing_mode == "Move full") desired_ac_status.data.hswing_mv = 0x01;
  else if (swing_mode == "Move left") desired_ac_status.data.hswing_mv = 0x02;
  else if (swing_mode == "Move mid") desired_ac_status.data.hswing_mv = 0x03;
  else if (swing_mode == "Move right") desired_ac_status.data.hswing_mv = 0x04;
  else if (swing_mode == "Fix left") desired_ac_status.data.hswing_fix = 0x01;
  else if (swing_mode == "Fix mid left") desired_ac_status.data.hswing_fix = 0x02;
  else if (swing_mode == "Fix mid") desired_ac_status.data.hswing_fix = 0x03;
  else if (swing_mode == "Fix mid right") desired_ac_status.data.hswing_fix = 0x04;
  else if (swing_mode == "Fix right") desired_ac_status.data.hswing_fix = 0x05;

  if (desired_ac_status.data.hswing_mv) desired_ac_status.data.hswing = 0x01;
  else desired_ac_status.data.hswing = 0;

  build_set_cmd(&desired_ac_status);
  ready_to_send_set_cmd_flag = true;

}

void TCLClimate::control(const climate::ClimateCall &call) {
    desired_ac_status_t desired_ac_status = {0};
    memcpy(desired_ac_status.raw, last_ac_status.raw, sizeof(desired_ac_status.raw));
    bool should_build_cmd = false;

    if (call.get_preset().has_value()) {
        climate::ClimatePreset preset = *call.get_preset();
        desired_ac_status.data.eco = (preset == climate::CLIMATE_PRESET_ECO) ? 1 : 0;
        should_build_cmd = true;
    }

    if (call.get_mode().has_value()) {
        climate::ClimateMode climate_mode = *call.get_mode();
        ESP_LOGI("TCL", "Received mode control command: %d", static_cast<int>(climate_mode));

        if (climate_mode == climate::CLIMATE_MODE_OFF) {
            desired_ac_status.data.power = 0x00;
        } else {
            desired_ac_status.data.power = 0x01;
            switch (climate_mode) {
                case climate::CLIMATE_MODE_COOL:    desired_ac_status.data.mode = 0x01; break;
                case climate::CLIMATE_MODE_DRY:     desired_ac_status.data.mode = 0x03; break;
                case climate::CLIMATE_MODE_FAN_ONLY:desired_ac_status.data.mode = 0x02; break;
                case climate::CLIMATE_MODE_HEAT:
                case climate::CLIMATE_MODE_HEAT_COOL:desired_ac_status.data.mode = 0x04; break;
                case climate::CLIMATE_MODE_AUTO:    desired_ac_status.data.mode = 0x05; break;
                default: break;
            }
        }
        should_build_cmd = true;
    }

    if (call.get_target_temperature().has_value()) {
        float temp = *call.get_target_temperature();
        ESP_LOGI("TCL", "Received temperature control command: %.1f°C", temp);

        desired_ac_status.data.temp = static_cast<uint8_t>(temp) - 16;
        should_build_cmd = true;
    }

    if (call.get_swing_mode().has_value()) {
        climate::ClimateSwingMode swing_mode = *call.get_swing_mode();

        switch(swing_mode) {
            case climate::CLIMATE_SWING_OFF:
                desired_ac_status.data.hswing = 0;
                desired_ac_status.data.vswing = 0;
                break;
            case climate::CLIMATE_SWING_BOTH:
                desired_ac_status.data.hswing = 1;
                desired_ac_status.data.vswing = 1;
                break;
            case climate::CLIMATE_SWING_VERTICAL:
                desired_ac_status.data.hswing = 0;
                desired_ac_status.data.vswing = 1;
                break;
            case climate::CLIMATE_SWING_HORIZONTAL:
                desired_ac_status.data.hswing = 1;
                desired_ac_status.data.vswing = 0;
                break;
        }
        should_build_cmd = true;
    }

    // Updated custom fan mode handling
    StringRef custom_fan_mode(call.get_custom_fan_mode());
    if (!custom_fan_mode.empty()) {
        std::string fan_mode(custom_fan_mode.c_str());
        ESP_LOGI("TCL", "Received fan mode control command: %s", fan_mode.c_str());

        desired_ac_status.data.turbo = 0x00;
        desired_ac_status.data.mute = 0x00;

        // Use map for fan mode parsing
        static const std::map<std::string, std::pair<uint8_t, uint8_t>> FAN_MODE_MAP = {
            {"Turbo",      {0x03, 0x01}},
            {"Mute",       {0x01, 0x01}},
            {"Automatic",  {0x00, 0x00}},
            {"1",          {0x01, 0x00}},
            {"2",          {0x04, 0x00}},
            {"3",          {0x02, 0x00}},
            {"4",          {0x05, 0x00}},
            {"5",          {0x03, 0x00}}
        };

        auto it = FAN_MODE_MAP.find(fan_mode);
        if (it != FAN_MODE_MAP.end()) {
            desired_ac_status.data.fan = it->second.first;
            if (fan_mode == "Turbo") desired_ac_status.data.turbo = 0x01;
            else if (fan_mode == "Mute") desired_ac_status.data.mute = 0x01;
        }
        should_build_cmd = true;
    }

    if (should_build_cmd) {
        ESP_LOGI("TCL", "Building command to AC unit");

        build_set_cmd(&desired_ac_status);
        //ready_to_send_set_cmd_flag = true;
       
       memcpy(last_ac_status.raw, desired_ac_status.raw, sizeof(last_ac_status.raw)); // copy draft array back into recieve_tx_array

       write_array(outgoing_tx_command.raw, sizeof(outgoing_tx_command.raw));
      ESP_LOGI("TCL", "Sending command to AC unit");
    }
}

climate::ClimateTraits TCLClimate::traits() {
  auto traits = climate::ClimateTraits();
  traits.add_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE);
  traits.set_supported_presets({climate::CLIMATE_PRESET_NONE, climate::CLIMATE_PRESET_ECO});
  traits.set_supported_modes({
    climate::CLIMATE_MODE_OFF,
    climate::CLIMATE_MODE_COOL,
    climate::CLIMATE_MODE_HEAT,
    climate::CLIMATE_MODE_FAN_ONLY,
    climate::CLIMATE_MODE_DRY,
    climate::CLIMATE_MODE_AUTO
  });
  traits.set_supported_swing_modes({
    climate::CLIMATE_SWING_OFF,
    climate::CLIMATE_SWING_BOTH,
    climate::CLIMATE_SWING_VERTICAL,
    climate::CLIMATE_SWING_HORIZONTAL
  });
  traits.set_visual_min_temperature(16.0);
  traits.set_visual_max_temperature(31.0);
  traits.set_visual_target_temperature_step(1.0);
  return traits;
}

void TCLClimate::update() {
    if (ready_to_send_set_cmd_flag) {
        ready_to_send_set_cmd_flag = false;
        //write_array(outgoing_tx_command.raw, sizeof(outgoing_tx_command.raw));
        //ESP_LOGI("TCL", "Sending command to AC unit");
        //skip_next_update = true;  // the AC takes a while to respond to our command, and the first status report back usualy contains the old AC state
    } else {

       // if( skip_next_update)
       // {
         // skip_next_update = false;
          //ESP_LOGI("TCL", "Skipping 1 status update request.");
         // return;
       // }
        write_array(REQ_CMD, sizeof(REQ_CMD));
    }
}

int TCLClimate::read_data_line(int readch, uint8_t *buffer, int len) {
    static int pos = 0;
    static bool wait_len = false;
    static int skipch = 0;

    if (readch < 0) return -1;

    if (readch == 0xBB && skipch == 0 && !wait_len) {
        pos = 0;
        skipch = 3; // wait for length byte
        wait_len = true;
        if (pos < len) buffer[pos++] = static_cast<uint8_t>(readch);
    } else if (skipch == 0 && wait_len) {
        if (pos < len) buffer[pos++] = static_cast<uint8_t>(readch);
        skipch = readch + 1; // +1 for checksum
        wait_len = false;
    } else if (skipch > 0) {
        if (pos < len) buffer[pos++] = static_cast<uint8_t>(readch);
        if (--skipch == 0 && !wait_len) return pos;
    }

    return -1;
}

bool TCLClimate::is_valid_xor(uint8_t *buffer, int len) {
    if (len < 1) return false;

    uint8_t xor_byte = 0;
    for (int i = 0; i < len - 1; i++) {
        xor_byte ^= buffer[i];
    }
    return xor_byte == buffer[len - 1];
}

void TCLClimate::print_hex_str(uint8_t *buffer, int len) {
    if (len <= 0) return;

    char str[MAX_LINE_LENGTH * 3] = {0};
    char *pstr = str;

    for (int i = 0; i < len && (pstr - str) < sizeof(str) - 3; i++) {
        pstr += sprintf(pstr, "%02X ", buffer[i]);
    }

    ESP_LOGD("TCL", "Received: %s", str);
}

void TCLClimate::loop() {
    static uint8_t buffer[MAX_LINE_LENGTH];

    while (available()) {
        int len = read_data_line(read(), buffer, MAX_LINE_LENGTH);

        if (len == sizeof(last_ac_status) && buffer[3] == 0x04) {
            memcpy(last_ac_status.raw, buffer, len);

            if (is_valid_xor(buffer, len)) {
                print_hex_str(buffer, len);

                  if(ready_to_send_set_cmd_flag ) 
                  {
                     ESP_LOGD("TCL", "Skipping status report processing due to a waiting command.");
                     return; // do not process a status report line if we have an outgoing command waiting
                  }

                //  if( skip_next_update) 
                //  {
                   //  ESP_LOGD("TCL", "Skipping status report processing due to a recent command.");
                    // return; // do not process a status report line if we have an outgoing command waiting
                 // }

                // Current temperature - rate-limited to reject noise
                // Also logs alternative byte position [16][17] for comparison
                float curr_temp = (((uint16_t)buffer[17] << 8 | (uint16_t)buffer[18]) / 374.0f - 32.0f) / 1.8f;
                float alt_temp  = (((uint16_t)buffer[16] << 8 | (uint16_t)buffer[17]) / 374.0f - 32.0f) / 1.8f;
                
                this->is_changed = false;

                // Set mode
                if (last_ac_status.data.power == 0x00) {
                    this->set_mode(climate::CLIMATE_MODE_OFF);
                } else {
                    static const std::map<uint8_t, climate::ClimateMode> MODE_MAP = {
                        {0x01, climate::CLIMATE_MODE_COOL},
                        {0x02, climate::CLIMATE_MODE_FAN_ONLY}, // GET 0x02 = FAN (confirmed)
                        {0x03, climate::CLIMATE_MODE_DRY},      // GET 0x03 = DRY (confirmed)
                        {0x04, climate::CLIMATE_MODE_HEAT},
                        {0x05, climate::CLIMATE_MODE_AUTO}
                    };
                    auto it = MODE_MAP.find(last_ac_status.data.mode);
                    if (it != MODE_MAP.end()) {
                        this->set_mode(it->second);
                    }
                }

                // Set fan mode
                static const std::map<uint8_t, std::string> FAN_MODE_MAP = {
                    {0x00, "Automatic"},
                    {0x01, "1"},
                    {0x04, "2"},
                    {0x02, "3"},
                    {0x05, "4"},
                    {0x03, "5"}
                };

                StringRef current_fan(StringRef(this->get_custom_fan_mode()));

                if (last_ac_status.data.turbo) {
                  // String literal to StringRef - use explicit construction
                  this->set_custom_fan_mode(StringRef("Turbo"));
                } else if (last_ac_status.data.mute) {
                  this->set_custom_fan_mode(StringRef("Mute"));
                } else {
                  auto it = FAN_MODE_MAP.find(last_ac_status.data.fan);
                  if (it != FAN_MODE_MAP.end()) {
                    StringRef current_fan(StringRef(this->get_custom_fan_mode()));
                    if (current_fan.empty() || current_fan != it->second) {
                      // Convert std::string to StringRef
                      this->set_custom_fan_mode(StringRef(it->second.c_str(), it->second.size()));
                    }
                  }
                }


                // Set swing mode - extracted from old code
                if (last_ac_status.data.hswing && last_ac_status.data.vswing) {
                    this->set_swing_mode(climate::CLIMATE_SWING_BOTH);
                } else if (!last_ac_status.data.hswing && !last_ac_status.data.vswing) {
                    this->set_swing_mode(climate::CLIMATE_SWING_OFF);
                } else if (last_ac_status.data.vswing) {
                    this->set_swing_mode(climate::CLIMATE_SWING_VERTICAL);
                } else if (last_ac_status.data.hswing) {
                    this->set_swing_mode(climate::CLIMATE_SWING_HORIZONTAL);
                }

                // Set swing positions - extracted from old code
                if (last_ac_status.data.vswing_mv == 0x01) set_vswing_pos("Move full");
                else if (last_ac_status.data.vswing_mv == 0x02) set_vswing_pos("Move upper");
                else if (last_ac_status.data.vswing_mv == 0x03) set_vswing_pos("Move lower");
                else if (last_ac_status.data.vswing_fix == 0x01) set_vswing_pos("Fix top");
                else if (last_ac_status.data.vswing_fix == 0x02) set_vswing_pos("Fix upper");
                else if (last_ac_status.data.vswing_fix == 0x03) set_vswing_pos("Fix mid");
                else if (last_ac_status.data.vswing_fix == 0x04) set_vswing_pos("Fix lower");
                else if (last_ac_status.data.vswing_fix == 0x05) set_vswing_pos("Fix bottom");
                else set_vswing_pos("Last position");

                if (last_ac_status.data.hswing_mv == 0x01) set_hswing_pos("Move full");
                else if (last_ac_status.data.hswing_mv == 0x02) set_hswing_pos("Move left");
                else if (last_ac_status.data.hswing_mv == 0x03) set_hswing_pos("Move mid");
                else if (last_ac_status.data.hswing_mv == 0x04) set_hswing_pos("Move right");
                else if (last_ac_status.data.hswing_fix == 0x01) set_hswing_pos("Fix left");
                else if (last_ac_status.data.hswing_fix == 0x02) set_hswing_pos("Fix mid left");
                else if (last_ac_status.data.hswing_fix == 0x03) set_hswing_pos("Fix mid");
                else if (last_ac_status.data.hswing_fix == 0x04) set_hswing_pos("Fix mid right");
                else if (last_ac_status.data.hswing_fix == 0x05) set_hswing_pos("Fix right");
                else set_hswing_pos("Last position");

                this->set_target_temperature(static_cast<float>(last_ac_status.data.temp + 16));

                this->set_current_temperature(curr_temp);

                // Eco readback - direct preset assignment, must track change manually
                climate::ClimatePreset new_preset = last_ac_status.data.eco ?
                    climate::CLIMATE_PRESET_ECO : climate::CLIMATE_PRESET_NONE;
                if (this->preset != new_preset) {
                    this->preset = new_preset;
                    this->is_changed = true;
                }

                if (this->is_changed) {
                    this->publish_state();
                }

                ESP_LOGD("TCL", "RX HVAC Power %i ", last_ac_status.data.power);
                ESP_LOGD("TCL", "RX HVAC Mode %i ", last_ac_status.data.mode);
                ESP_LOGD("TCL", "RX Fan Mode %i ", last_ac_status.data.fan);
                ESP_LOGD("TCL", "RX Temp [17][18]=%.2f°C  [16][17]=%.2f°C", curr_temp, alt_temp);
                ESP_LOGD("TCL", "RX Target Temp =%.2f°C ", static_cast<float>(last_ac_status.data.temp + 16));
                ESP_LOGD("TCL", "RX Disp Mode %i ", last_ac_status.data.disp);
                ESP_LOGD("TCL", "RX Turbo Mode %i ", last_ac_status.data.turbo);
                ESP_LOGD("TCL", "RX Mute Mode %i ", last_ac_status.data.mute);
              
            }
        }
    }
}

}  // namespace tcl_climate
}  // namespace esphome
//#endif  // USE_ARDUINO

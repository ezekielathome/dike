#pragma once

#include "eleos.hpp"     // for interface
#include "valve.hpp"     // for user_cmd, create_interface, edict (ptr only)
#include <cstddef>       // for size_t
#include <deque>         // for _Deque_iterator, deque
#include <map>           // for map
#include <string>        // for allocator, string
#include <unordered_map> // for unordered_map

#define DEBUG_DETECTIONS

// TODO: replace with pattern scanning to make game support easy
constexpr std::size_t RUNCOMMAND_INDEX = 479;

// max amount of deviation from rounded number
constexpr float DEVIATION_THRESHOLD = 0.25;
// lower threshold for mouse delta handling
constexpr float LOWER_THRESHOLD = 0.000001;
// upper threshold for mouse delta handling
constexpr float UPPER_THRESHOLD = 128.0;

// offset to GetFOVForNetworking
constexpr std::size_t OFFSET_GET_FOV = 0x7C9430;
// offset to GetDefaultFOV
constexpr std::size_t OFFSET_GET_DEFAULT_FOV = 0x61b340;
// offset to GetFOVDistanceAdjustFactorForNetworking
constexpr std::size_t OFFSET_FOV_ADJUST = 0x7C9540;

// offset to m_bIsScoped
constexpr std::size_t OFFSET_IS_SCOPED = 5800;
// offset to m_bResumeZoom
constexpr std::size_t OFFSET_RESUME_ZOOM = 5802;
// offset to m_bResumeZoom
constexpr std::size_t OFFSET_FOV_START = 875;

enum scaling_variable : int {
  sensitivity = 0,
  pitch,
  yaw,
  zoom_ratio,
};

struct snapshot_t {
  valve::user_cmd cmd;
  float fov;
  bool zoomed;
  bool resume_zoom;
};

struct player_context_t {
  std::deque< snapshot_t > snapshots;
  std::map< scaling_variable, float > scaling;
  int last_violation;
};

class dike_plugin
    : public eleos::interface
    , valve::plugin_callbacks::v4 {
public:
  dike_plugin( ) noexcept
      : eleos::interface( this, "ISERVERPLUGINCALLBACKS004" ) { };

  auto description( ) -> const char * override { return "dike"; };

  auto
  load( valve::create_interface factory, valve::create_interface /*unused*/ )
      -> bool override;
  auto client_loaded( valve::edict *edict ) -> void override;
  auto query_cvar_callback(
      int cookie,
      valve::edict *edict,
      valve::cvar_status status,
      const char *name,
      const char *value ) -> void override {
    helpers->handle_convar_callback( cookie, std::string( value ) );
  }

public:
  static std::unordered_map< void *, player_context_t > cache;
  valve::plugin_helpers *helpers;
};
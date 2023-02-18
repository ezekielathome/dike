#pragma once

#include <deque>          // for deque
#include <unordered_map>  // for unordered_map
#include "valve.hpp"      // for user_cmd

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

struct player_entry_t {
  std::deque< snapshot_t > snapshots;
  std::unordered_map< scaling_variable, float > scaling;
  int last_violation;
};

using player_store_t = std::unordered_map< void *, player_entry_t >;

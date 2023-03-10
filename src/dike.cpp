#include "dike.hpp"
#include <algorithm>      // for clamp
#include <apate.hpp>      // for declared
#include <bit>            // for bit_cast
#include <bits/std_abs.h> // for abs
#include <chrono>         // for seconds
#include <cmath>          // for fabsf, roundf
#include <cstdint>        // for uintptr_t
#include <deque>          // for _Deque_iterator, deque, operator==, _Deque...
#include <dlfcn.h>        // for dlopen, RTLD_NOW
#include <elf.h>          // for Elf32_Addr
#include <future>         // for shared_future, future_status, future_statu...
#include <iterator>       // for end
#include <link.h>         // for link_map
#include <map>            // for map, operator==, _Rb_tree_iterator, _Rb_tr...
#include <memory>         // for unique_ptr, make_unique, allocator_traits<...
#include <stdio.h>        // for printf
#include <thread>         // for thread
#include <type_traits>    // for remove_reference<>::type
#include <unordered_map>  // for unordered_map, _Node_iterator, operator==
#include <utility>        // for move, pair, tuple_element<>::type
#include <vector>         // for vector

std::unique_ptr< apate::declared > run_command;

void hooked_run_command( void *self, valve::user_cmd *cmd, void *helper ) {
  // ignore player commands if they haven't responded to our query
  if ( !plugin.store.contains( self ) )
    return; // original( self, cmd, helper );
  auto *entry = &plugin.store[ self ];

  // TODO: don't hardcode addresses, implement pattern scanning.
  static auto server =
      std::bit_cast< link_map * >( dlopen( "csgo/bin/server.so", RTLD_NOW ) );

  static auto get_fov = std::bit_cast< int ( * )( void * ) >(
      server->l_addr +
      toml::find< uintptr_t >( plugin.config.section, "get_fov" ) );
  static auto get_default_fov = std::bit_cast< int ( * )( void * ) >(
      server->l_addr +
      toml::find< uintptr_t >( plugin.config.section, "get_default_fov" ) );
  static auto get_distance_adjustment = std::bit_cast< float ( * )( void * ) >(
      server->l_addr +
      toml::find< uintptr_t >( plugin.config.section, "get_adjustment" ) );

  static auto is_scoped =
      toml::find< uintptr_t >( plugin.config.section, "is_scoped" );
  static auto resume_zoom =
      toml::find< uintptr_t >( plugin.config.section, "resume_zoom" );

  const auto scoped = *std::bit_cast< bool * >(
      std::bit_cast< uintptr_t >( self ) + is_scoped );
  const auto is_resuming_zoom = *std::bit_cast< bool * >(
      std::bit_cast< uintptr_t >( self ) + resume_zoom );

  const auto fov = static_cast< float >( get_fov( self ) );
  const auto default_fov = static_cast< float >( get_default_fov( self ) );
  auto distance_adjustment = get_distance_adjustment( self );

  const auto zoomed = ( fov != default_fov );

  // apply zoom sensitivity ratio to adjustment
  distance_adjustment *= entry->scaling[ scaling_variable::zoom_ratio ];

  // process snapshots to check if sensitivity might be variable during current
  // cmd.
  if ( ( cmd->buttons & button_flags::IN_ATTACK2 ) ||
       ( cmd->buttons & button_flags::IN_ATTACK ) && zoomed ||
       is_resuming_zoom )
    goto final;

  for ( const auto snapshot : entry->snapshots ) {
    if ( snapshot.cmd.buttons & button_flags::IN_ATTACK2 )
      goto final;
    if ( snapshot.fov != fov )
      goto final;
  }

  // check for both yaw and pitch
  for ( size_t idx = 0; idx <= 1; idx++ ) {
    // get m_yaw (1) / m_pitch (2)
    const auto scaling = static_cast< scaling_variable >( idx + 1 );
    float mouse_delta =
        ( cmd->view[ idx ] - entry->snapshots.front( ).cmd.view[ idx ] ) /
        entry->scaling[ scaling ];

    // yaw is negated ( angles[yaw] -= yaw * mx )
    if ( scaling == scaling_variable::yaw )
      mouse_delta = -mouse_delta;

    // apply sensitivity and distance adjustment to delta
    mouse_delta /= entry->scaling[ scaling_variable::sensitivity ];
    if ( zoomed )
      mouse_delta /= distance_adjustment;

    float abs_delta = fabsf( mouse_delta );
    if ( std::clamp( abs_delta, LOWER_THRESHOLD, UPPER_THRESHOLD ) ==
         abs_delta ) {
      float deviation = std::abs( roundf( mouse_delta ) - mouse_delta );
      if ( deviation > DEVIATION_THRESHOLD ) {
        printf(
            "%p: deviation (%.4g%%) beyond threshold (%.4g%%)\n",
            self,
            deviation * 100,
            DEVIATION_THRESHOLD * 100 );
        entry->last_violation = cmd->number;
#ifdef DEBUG_DETECTIONS
        printf( "  DEBUG:\n" );
        printf( "    delta: %f\n", mouse_delta );
        printf( "    m_bIsScoped: %i\n", scoped );
        printf( "    fov: %f\n", ( fov ) );
        printf( "    default_fov: %f\n", ( default_fov ) );
        printf( "    resume_zoom: %i\n", ( resume_zoom ) );
        printf( "    zoomed: %i\n", ( fov != default_fov ) );
        printf( "    adjustment: %f\n", distance_adjustment );
        printf( "    number: %d\n", cmd->number );
        for ( size_t idx = 0; auto const &snapshot : entry->snapshots ) {
          printf( "  snapshot(%d)\n", idx++ );

          printf( "    cmd #%d\n", snapshot.cmd.number );
          printf( "      tick_count: %d\n", snapshot.cmd.tick_count );
          printf(
              "      x: %f, y: %f, z: %f\n",
              snapshot.cmd.view[ 0 ],
              snapshot.cmd.view[ 1 ],
              snapshot.cmd.view[ 2 ] );

          printf( "      buttons\n" );
          printf(
              "        IN_ATTACK: %i\n",
              snapshot.cmd.buttons & button_flags::IN_ATTACK );
          printf(
              "        IN_ATTACK2: %i\n",
              snapshot.cmd.buttons & button_flags::IN_ATTACK2 );
          printf( "    fov: %f\n", snapshot.fov );
          printf( "    zoomed: %i\n", snapshot.zoomed );
          printf( "    resume_zoom: %i\n", snapshot.resume_zoom );
        }
#endif
      }
    }
  }

final:
  if ( entry->snapshots.size( ) >= 4 )
    entry->snapshots.pop_back( );
  entry->snapshots.emplace_front(
      snapshot_t { .cmd = *cmd,
                   .fov = fov,
                   .zoomed = zoomed,
                   .resume_zoom = is_resuming_zoom } );

  // "punishment" system
  // this sets unsets the IN_ATTACK flag, leading to the player being unable to
  // attack.
  if ( cmd->number < entry->last_violation + 16 )
    cmd->buttons &= ~button_flags::IN_ATTACK;

  // call original run_command to let server handle the command.
  run_command->get_original< void ( * )( void *, void *, void * ) >( )(
      self, cmd, helper );
}

auto dike_plugin::load(
    valve::create_interface factory, valve::create_interface server_factory )
    -> bool {
  helpers = new valve::plugin_helpers { std::bit_cast< void * >(
      factory( "ISERVERPLUGINHELPERS001", nullptr ) ) };
  auto server = std::bit_cast< valve::server_game * >(
      server_factory( "ServerGameDLL005", nullptr ) );

  config.data = toml::parse( "csgo/addons/dike/config.toml" );
  if ( server ) {
    auto sections = toml::find( config.data, "section" );
    auto descriptor = server->get_game_descriptor( );
    if ( descriptor ) {
      config.section = toml::find( sections, descriptor );
      printf( "dike: found section for descriptor `%s`\n", descriptor );
      return true;
    } else {
      printf( "dike: failed to get game descriptor\n" );
    }
  } else {
    printf( "dike: failed to get server factory `ServerGameDLL005`" );
  }

  return false;
};

auto dike_plugin::client_loaded( valve::edict *edict ) -> void {
  // when a client has loaded into the server we want to query the convars we
  // need from them, and hook their base entity's runcommand in order to handle
  // input.

  // convars we need from the client in order to verify their input
  static std::vector< std::string > convars = {
    "sensitivity", "m_yaw", "m_pitch", "zoom_sensitivity_ratio_mouse"
  };
  std::unordered_map< std::string, std::shared_future< std::string > > futures;
  for ( auto const &convar : convars ) {
    auto future = helpers->query_convar( edict, convar );
    futures.insert_or_assign( convar, std::move( future ) );
  }

  player_entry_t entry = { };
  std::thread( [ = ]( ) mutable {
    for ( auto [ convar, future ] : futures ) {
      static std::map< std::string, scaling_variable > lookup = {
        { "sensitivity", scaling_variable::sensitivity },
        { "m_yaw", scaling_variable::yaw },
        { "m_pitch", scaling_variable::pitch },
        { "zoom_sensitivity_ratio_mouse", scaling_variable::zoom_ratio },
      };
      auto result = future.wait_for( std::chrono::seconds( CONVAR_TIMEOUT ) );
      if ( result != std::future_status::ready )
        return; // assume client isn't sending convars on purpose, we break
                // out so we dont assign to cache, leading to player commands
                // being ignored...
      auto iter = lookup.find( convar );
      if ( iter != std::end( lookup ) )
        entry.scaling[ iter->second ] = std::stof( future.get( ) );
    }

    plugin.store.insert_or_assign( edict->unknown, entry );
  } ).detach( );

  // only hook once, this could be done better (for example, in load)
  // but this is the easiest way of getting the vmt pointer
  static bool ran = false;
  if ( !ran ) {
    auto method = std::bit_cast< void * >(
        &( ( *std::bit_cast< uintptr_t ** >( edict->unknown ) )
               [ toml::find< size_t >( config.section, "run_command" ) ] ) );

    run_command = std::make_unique< apate::declared >( method );
    run_command->hook( &hooked_run_command );

    ran = true;
  }
}

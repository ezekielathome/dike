#include "dike.hpp"
#include <algorithm>      // for clamp
#include <bit>            // for bit_cast
#include <bits/std_abs.h> // for abs
#include <chrono>         // for seconds
#include <cmath>          // for fabsf, roundf
#include <cstdint>        // for uintptr_t
#include <dlfcn.h>        // for dlopen, RTLD_NOW
#include <elf.h>          // for Elf32_Addr
#include <future>         // for shared_future, future_status, future_statu...
#include <iterator>       // for end
#include <link.h>         // for link_map
#include <map>            // for map, operator==, _Rb_tree_iterator, _Rb_tr...
#include <memory>         // for allocator_traits<>::value_type
#include <stdio.h>        // for printf, perror
#include <sys/mman.h>     // for mprotect, PROT_READ, PROT_WRITE
#include <thread>         // for thread
#include <type_traits>    // for remove_reference<>::type
#include <unistd.h>       // for sysconf, _SC_PAGE_SIZE
#include <unordered_map>  // for unordered_map, _Node_iterator, operator==
#include <utility>        // for move, pair, tuple_element<>::type
#include <vector>         // for vector

using PlayerRunCommand_t = void ( * )( void *, void *, void * );
PlayerRunCommand_t original;

std::unordered_map< void *, player_context_t > cache;

void hooked_run_command( void *self, valve::user_cmd *cmd, void *helper ) {
  // ignore player commands if they haven't repsonded to our query
  if ( !cache.contains( self ) )
    return; // original( self, cmd, helper );
  auto *ctx = &cache[ self ];

  // TODO: don't hardcode addresses, implement pattern scanning.
  static auto server = ( link_map * ) dlopen( "csgo/bin/server.so", RTLD_NOW );
  static auto get_fov =
      std::bit_cast< int ( * )( void * ) >( server->l_addr + OFFSET_GET_FOV );
  static auto get_default_fov = std::bit_cast< int ( * )( void * ) >(
      server->l_addr + OFFSET_GET_DEFAULT_FOV );
  static auto get_distance_adjustment = std::bit_cast< float ( * )( void * ) >(
      server->l_addr + OFFSET_FOV_ADJUST );

  const auto scoped = *std::bit_cast< bool * >(
      std::bit_cast< uintptr_t >( self ) + OFFSET_IS_SCOPED );
  const auto resume_zoom = *std::bit_cast< bool * >(
      std::bit_cast< uintptr_t >( self ) + OFFSET_RESUME_ZOOM );

  const auto fov = static_cast< float >( get_fov( self ) );
  const auto default_fov = static_cast< float >( get_default_fov( self ) );
  auto distance_adjustment = get_distance_adjustment( self );

  const auto zoomed = ( fov != default_fov );

  // apply zoom sensitivity ratio to adjustment
  distance_adjustment *= ctx->scaling[ scaling_variable::zoom_ratio ];

  // process snapshots to check if sensitivity might be variable during current
  // cmd.
  if ( ( cmd->buttons & button_flags::IN_ATTACK2 ) ||
       ( cmd->buttons & button_flags::IN_ATTACK ) && zoomed || resume_zoom )
    goto final;

  for ( int i = 0; i < ctx->snapshots.size( ); i++ ) {
    if ( ctx->snapshots[ i ].cmd.buttons & button_flags::IN_ATTACK2 )
      goto final;
    if ( ctx->snapshots[ i ].fov != fov )
      goto final;
  }

  // check for both yaw and pitch
  for ( int i = 0; i <= 1; i++ ) {
    // get m_yaw (1) / m_pitch (2)
    const auto scaling = static_cast< scaling_variable >( i + 1 );
    float mouse_delta =
        ( cmd->view[ i ] - ctx->snapshots.front( ).cmd.view[ i ] ) /
        ctx->scaling[ scaling ];

    // yaw is negated ( angles[yaw] -= yaw * mx )
    if ( scaling == scaling_variable::yaw )
      mouse_delta = -mouse_delta;

    // apply sensitivity and distance adjustment to delta
    mouse_delta /= ctx->scaling[ scaling_variable::sensitivity ];
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
        ctx->last_violation = cmd->number;
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
        for ( size_t idx = 0; auto const &snapshot : ctx->snapshots ) {
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
  if ( ctx->snapshots.size( ) >= 4 )
    ctx->snapshots.pop_back( );
  ctx->snapshots.emplace_front( snapshot_t {
      .cmd = *cmd, .fov = fov, .zoomed = zoomed, .resume_zoom = resume_zoom } );

  // "punishment" system
  // this sets unsets the IN_ATTACK flag, leading to the player being unable to
  // attack.
  if ( cmd->number < ctx->last_violation + 16 )
    cmd->buttons &= ~button_flags::IN_ATTACK;

  // call original run_command to let server handle the command.
  original( self, cmd, helper );
}

auto dike_plugin::load(
    valve::create_interface factory, valve::create_interface /*unused*/ )
    -> bool {
  helpers = new valve::plugin_helpers { std::bit_cast< void * >(
      factory( "ISERVERPLUGINHELPERS001", nullptr ) ) };

  return true;
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

  player_context_t ctx = { };
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
        ctx.scaling[ iter->second ] = std::stof( future.get( ) );
    }

    cache.insert_or_assign( edict->unknown, ctx );
  } ).detach( );

  // only hook once, this could be done better (for example, in load)
  // but this is the easiest way of getting the vmt pointer
  static bool ran = false;
  if ( !ran ) {
    auto method =
        std::bit_cast< uintptr_t >( &( ( *std::bit_cast< uintptr_t ** >(
            edict->unknown ) )[ RUNCOMMAND_INDEX ] ) );

    auto *page =
        std::bit_cast< void * >( method & ~( sysconf( _SC_PAGE_SIZE ) - 1 ) );
    if ( mprotect( page, 4, PROT_READ | PROT_WRITE ) == 0 ) {
      original = *std::bit_cast< PlayerRunCommand_t * >( method );
      *std::bit_cast< void ** >( method ) =
          std::bit_cast< PlayerRunCommand_t * >( &hooked_run_command );
      mprotect( page, 4, PROT_READ );
    } else {
      perror( "dike: unable to perform run_command hook" );
    }
    ran = true;
  }
}

const dike_plugin plugin;
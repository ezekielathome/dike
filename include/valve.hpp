#pragma once

#include <bit>           // for bit_cast
#include <future>        // for promise, future, shared_future
#include <stddef.h>      // for size_t
#include <stdint.h>      // for uint32_t, uint16_t, uint8_t
#include <string>        // for string
#include <type_traits>   // for remove_reference<>::type
#include <unordered_map> // for unordered_map, _Map_base<>::mapped_type
#include <utility>       // for move

constexpr size_t CONVAR_TIMEOUT = 6;

namespace valve {
  typedef enum {
    result_continue = 0,
    result_override = 1,
    result_stop = 2
  } plugin_result;

  typedef enum {
    status_intact = 0,
    status_not_found = 1,
    status_command = 2,
    status_protected = 3
  } cvar_status;

  using create_interface = void *( * ) ( const char *, int * );

  class server_unknown {
  public:
    virtual void *ihandleentity_0( ) = 0;
    virtual void *ihandleentity_1( ) = 0;
    virtual void *ihandleentity_2( ) = 0;
    virtual void *get_collideable( ) = 0;
    virtual void *get_networkable( ) = 0;
    virtual void *get_base_entity( ) = 0;
  };

  struct edict {
    int state_flags;
    short edict_index;
    short network_sn;
    void *networkable;
    server_unknown *unknown;
  };

  class plugin_helpers {
  public:
    plugin_helpers( void *instance )
        : instance( instance ) { };

    auto query_convar( edict *entity, std::string convar )
        -> std::shared_future< std::string > {
      const auto cookie = this->internal_query( entity, convar.c_str( ) );
      std::promise< std::string > promise;

      auto future = promise.get_future( ).share( );
      promises.insert_or_assign( cookie, std::move( promise ) );
      return future;
    }

    auto handle_convar_callback( int cookie, std::string value ) -> void {
      if ( promises.contains( cookie ) )
        promises.at( cookie ).set_value( value );
    }

  private:
    auto internal_query( edict *player, const char *cvar ) -> int {
      // call vmt func #3
      return std::bit_cast< int ( * )( void *, void *, const char * ) >( (
          *std::bit_cast< void *** >( instance ) )[ 2 ] )( this, player, cvar );
    }

    void *instance;
    std::unordered_map< int, std::promise< std::string > > promises;
  };

  namespace plugin_callbacks {
    class v4 {
    public:
      // called when server loads the plugin
      virtual auto load(
          valve::create_interface factory, valve::create_interface /*unused*/ )
          -> bool {
        return true;
      }
      // called when the plugin should shutdown
      virtual auto unload( ) -> void {}
      // called when a plugin should pause
      virtual auto pause( ) -> void {}
      // called when a plugin should unpause execution
      virtual auto unpause( ) -> void {}
      // plugin description
      virtual auto description( ) -> const char * { return ""; }
      // called when a new level is started (includes level changes)
      virtual auto level_init( ) -> void {}
      // called when the server is about to activate
      virtual auto server_activate( ) -> void {}
      // called every game frame
      virtual auto game_frame( bool simulating ) -> void {}
      // called when a level is shutdown ( includes level changes )
      virtual auto level_shutdown( ) -> void {}
      // called when a client is going active
      virtual auto client_active( edict *edict ) -> void {}
      // called when a client has fully connected ( has received initial entity
      // baseline )
      virtual auto client_loaded( edict *edict ) -> void {}
      // called when a client is disconnecting from the server
      virtual auto client_disconnect( edict *edict ) -> void {}
      // called when a client is connected
      virtual auto client_connected( edict *edict, char const *name ) -> void {}

      // sets the client index for the client who typed the command into their
      // console
      virtual auto set_command_client( int idx ) -> void {}
      // called when a player changed replicated cvars
      virtual auto client_cvar_changed( edict *edict ) -> void {}

      // called when a client is connecting to the server
      virtual auto client_connect(
          bool *allow,
          edict *edict,
          const char *name,
          const char *address,
          char *reject,
          int reject_len ) -> plugin_result {
        return result_continue;
      }

      // called when a client runs a command
      virtual auto client_command( edict *edict, void *args ) -> plugin_result {
        return result_continue;
      }

      // called when a client has their NID validated
      virtual auto nid_validated( const char *name, const char *nid )
          -> plugin_result {
        return result_continue;
      }

      // called when a cvar query is finished
      virtual auto query_cvar_callback(
          int cookie,
          edict *edict,
          cvar_status status,
          const char *name,
          const char *value ) -> void {}

      // called when a new edict is allocated
      virtual auto on_edict_allocated( edict *edict ) -> void {}
      // called when an edict is about to be freed
      virtual auto on_edict_freed( const edict *edict ) -> void {}

      virtual auto crypt_required(
          uint32_t address,
          uint16_t port,
          uint32_t account_id,
          bool client_wants_crypt ) -> bool {
        return true;
      }
      virtual auto crypt_validate(
          uint32_t address,
          uint16_t port,
          uint32_t account_id,
          int key_index,
          int encrypted_bytes,
          uint8_t *buffer,
          uint8_t *plain_key ) -> bool {
        return true;
      }
    };
  } // namespace plugin_callbacks

  enum button_flags : int {
    IN_ATTACK = ( 1 << 0 ),
    IN_JUMP = ( 1 << 1 ),
    IN_DUCK = ( 1 << 2 ),
    IN_FORWARD = ( 1 << 3 ),
    IN_BACK = ( 1 << 4 ),
    IN_USE = ( 1 << 5 ),
    IN_CANCEL = ( 1 << 6 ),
    IN_LEFT = ( 1 << 7 ),
    IN_RIGHT = ( 1 << 8 ),
    IN_MOVELEFT = ( 1 << 9 ),
    IN_MOVERIGHT = ( 1 << 10 ),
    IN_ATTACK2 = ( 1 << 11 ),
    IN_RUN = ( 1 << 12 ),
    IN_RELOAD = ( 1 << 13 ),
    IN_ALT1 = ( 1 << 14 ),
    IN_ALT2 = ( 1 << 15 ),
    IN_SCORE = ( 1 << 16 ),
    IN_SPEED = ( 1 << 17 ),
    IN_WALK = ( 1 << 18 ),
    IN_ZOOM = ( 1 << 19 ),
    IN_WEAPON1 = ( 1 << 20 ),
    IN_WEAPON2 = ( 1 << 21 ),
    IN_BULLRUSH = ( 1 << 22 ),
    IN_GRENADE1 = ( 1 << 23 ),
    IN_GRENADE2 = ( 1 << 24 ),
    IN_ATTACK3 = ( 1 << 25 ),
  };

  struct user_cmd {
    void *vmt;
    int number;
    int tick_count;
    float view[ 3 ];
    float direction[ 3 ];
    float forward;
    float side;
    float up;
    int buttons;
    char impulse;
    int weapon_select;
    int weapon_subtype;
    int random_seed;
    int server_seed;
    short mx;
    short my;
  };
} // namespace valve

using valve::button_flags;
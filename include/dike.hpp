#pragma once

#include "eleos.hpp"  // for interface
#include "player.hpp" // for player_store_t
#include "valve.hpp"  // for create_interface, edict (ptr only), cvar_status
#include <cstddef>    // for size_t
#include <string>     // for allocator, string
#include <toml.hpp>


#define DEBUG_DETECTIONS

// max amount of deviation from rounded number
constexpr float DEVIATION_THRESHOLD = 0.25;
// lower threshold for mouse delta handling
constexpr float LOWER_THRESHOLD = 0.000001;
// upper threshold for mouse delta handling
constexpr float UPPER_THRESHOLD = 128.0;

// TODO: replace with pattern scanning to make game support easy
constexpr std::size_t RUNCOMMAND_INDEX = 479;

/* offset to GetFOV (is GetFOVForNetworking better?)
  .text:007C9290 55                      - push    ebp
  .text:007C9291 89 E5                   - mov     ebp, esp
  .text:007C9293 53                      - push    ebx
  .text:007C9294 83 EC 14                - sub     esp, 14h
  .text:007C9297 8B 5D 08                - mov     ebx, [ebp+arg_0]
  .text:007C929A 8B 03                   - mov     eax, [ebx]
  .text:007C929C 8B 80 24 05 00 00       - mov     eax, [eax+524h]
  .text:007C92A2 3D 10 FB 5C 00          - cmp     eax, offset sub_5CFB10
  .text:007C92A7 0F 85 0B 01 00 00       - jnz     loc_7C93B8
  .text:007C92AD 8B 93 48 0D 00 00       - mov     edx, [ebx+0D48h]
  .text:007C92B3 83 FA FF                - cmp     edx, 0FFFFFFFFh
  .text:007C92B6 74 1C                   - jz      short loc_7C92D4
  .text:007C92B8 8B 0D 00 94 6C 01       - mov     ecx, off_16C9400
  .text:007C92BE 0F B7 C2                - movzx   eax, dx
  .text:007C92C1 8D 04 40                - lea     eax, [eax+eax*2]
  .text:007C92C4 8D 04 C1                - lea     eax, [ecx+eax*8]
  .text:007C92C7 83 F8 FC                - cmp     eax, 0FFFFFFFCh
  .text:007C92CA 74 08                   - jz      short loc_7C92D4
  .text:007C92CC C1 EA 10                - shr     edx, 10h
  .text:007C92CF 39 50 08                - cmp     [eax+8], edx
  .text:007C92D2 74 7C                   - jz      short loc_7C9350
*/
constexpr std::size_t OFFSET_GET_FOV = 0x7c9290;

// offset to GetDefaultFOV
// search for "Usage: spec_goto"
constexpr std::size_t OFFSET_GET_DEFAULT_FOV = 0x61b330;

/* offset to GetFOVDistanceAdjustFactorForNetworking
xref to GetDefaultFov (sub+0xB)
  .text:007C9530 55                      - push    ebp
  .text:007C9531 89 E5                   - mov     ebp, esp
  .text:007C9533 53                      - push    ebx
  .text:007C9534 83 EC 20                - sub     esp, 20h
  .text:007C9537 8B 5D 08                - mov     ebx, [ebp+arg_0]
  .text:007C953A 53                      - push    ebx
  .text:007C953B E8 F0 1D E5 FF          - call    sub_61B330
  .text:007C9540 66 0F EF C9             - pxor    xmm1, xmm1
  .text:007C9544 89 1C 24                - mov     [esp], ebx
  .text:007C9547 F3 0F 2A C8             - cvtsi2ss xmm1, eax
  .text:007C954B F3 0F 11 4D F4          - movss   [ebp+var_C], xmm1
  .text:007C9550 E8 CB FE FF FF          - call    sub_7C9420
  .text:007C9555 66 0F EF C0             - pxor    xmm0, xmm0
  .text:007C9559 F3 0F 10 4D F4          - movss   xmm1, [ebp+var_C]
  .text:007C955E 83 C4 10                - add     esp, 10h
  .text:007C9561 F3 0F 2A C0             - cvtsi2ss xmm0, eax
  .text:007C9565 0F 2F C8                - comiss  xmm1, xmm0
  .text:007C9568 74 1E                   - jz      short loc_7C9588
  .text:007C956A 0F 2F 0D 70 F7 19 01    - comiss  xmm1, ds:dword_119F770
  .text:007C9571 72 15                   - jb      short loc_7C9588
  .text:007C9573 F3 0F 5E C1             - divss   xmm0, xmm1
  .text:007C9577 8B 5D FC                - mov     ebx, [ebp+var_4]
  .text:007C957A F3 0F 11 45 F4          - movss   [ebp+var_C], xmm0
  .text:007C957F D9 45 F4                - fld     [ebp+var_C]
  .text:007C9582 C9                      - leave
*/
constexpr std::size_t OFFSET_FOV_ADJUST = 0x7c9530;

/* both offsets are referenced in CWeaponCSBasegun::PrimaryAttack, search
cycletime_when_zoomed

  if ( (_BYTE)v1 )
  {
    if ( *(_BYTE *)(v2 + 5800) )
    {
      sub_4F32A0(v2, v2 + 5800);
      *(_BYTE *)(v2 + 5800) = 0;
    }
    if ( !*(_BYTE *)(v2 + 5802) )
    {
      sub_4F32A0(v2, v2 + 5802);
      *(_BYTE *)(v2 + 5802) = 1;
    }
    v20 = get_default_fov(v2);
    LOBYTE(v1) = set_fov(v2, v2, v20, 1028443341, 0);
    if ( lpsrc[667] )
    {
      LOBYTE(v1) = sub_4F32A0(lpsrc, lpsrc + 667);
      lpsrc[667] = 0;
    }
  }
*/
// offset to m_bIsScoped
constexpr std::size_t OFFSET_IS_SCOPED = 5800;
// offset to m_bResumeZoom
constexpr std::size_t OFFSET_RESUME_ZOOM = 5802;

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
  struct {
    toml::value data;
    toml::value section;
  } config;

  player_store_t store;
  valve::plugin_helpers *helpers;
};

// instantiate dike_plugin class
dike_plugin plugin;
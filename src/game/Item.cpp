// Copyright (C) 2004 Id Software, Inc.
//

#include "../idlib/precompiled.h"
#pragma hdrstop

#include "Game_local.h"

#define WEAPON_COUNT_STOP 2 // doomtrinity-dual weapon

/*
===============================================================================

  idItem

===============================================================================
*/

const idEventDef EV_DropToFloor( "<dropToFloor>" );
const idEventDef EV_RespawnItem( "respawn" );
const idEventDef EV_RespawnFx( "<respawnFx>" );
const idEventDef EV_GetPlayerPos( "<getplayerpos>" );
const idEventDef EV_HideObjective( "<hideobjective>", "e" );
const idEventDef EV_CamShot( "<camshot>" );

CLASS_DECLARATION( idEntity, idItem )
	EVENT( EV_DropToFloor,		idItem::Event_DropToFloor )
	EVENT( EV_Touch,			idItem::Event_Touch )
	EVENT( EV_Activate,			idItem::Event_Trigger )
	EVENT( EV_RespawnItem,		idItem::Event_Respawn )
	EVENT( EV_RespawnFx,		idItem::Event_RespawnFx )
END_CLASS


/*
================
idItem::idItem
================
*/
idItem::idItem() {
	spin = false;
	inView = false;
	inViewTime = 0;
	lastCycle = 0;
	lastRenderViewTime = -1;
	itemShellHandle = -1;
	shellMaterial = NULL;
	orgOrigin.Zero();
	canPickUp = true;
	fl.networkSync = true;

	noPickup = false;	// sikk - Item Management: Manual Item Pickup
}

/*
================
idItem::~idItem
================
*/
idItem::~idItem() {
	// remove the highlight shell
	if ( itemShellHandle != -1 ) {
		gameRenderWorld->FreeEntityDef( itemShellHandle );
	}
}

/*
================
idItem::Save
================
*/
void idItem::Save( idSaveGame *savefile ) const {

	savefile->WriteVec3( orgOrigin );
	savefile->WriteBool( spin );
	savefile->WriteBool( pulse );
	savefile->WriteBool( canPickUp );

	savefile->WriteMaterial( shellMaterial );

	savefile->WriteBool( inView );
	savefile->WriteInt( inViewTime );
	savefile->WriteInt( lastCycle );
	savefile->WriteInt( lastRenderViewTime );
}

/*
================
idItem::Restore
================
*/
void idItem::Restore( idRestoreGame *savefile ) {

	savefile->ReadVec3( orgOrigin );
	savefile->ReadBool( spin );
	savefile->ReadBool( pulse );
	savefile->ReadBool( canPickUp );

	savefile->ReadMaterial( shellMaterial );

	savefile->ReadBool( inView );
	savefile->ReadInt( inViewTime );
	savefile->ReadInt( lastCycle );
	savefile->ReadInt( lastRenderViewTime );

	itemShellHandle = -1;
}

/*
================
idItem::UpdateRenderEntity
================
*/
bool idItem::UpdateRenderEntity( renderEntity_s *renderEntity, const renderView_t *renderView ) const {

	if ( lastRenderViewTime == renderView->time ) {
		return false;
	}

	lastRenderViewTime = renderView->time;

	// check for glow highlighting if near the center of the view
	idVec3 dir = renderEntity->origin - renderView->vieworg;
	dir.Normalize();
	float d = dir * renderView->viewaxis[0];

	// two second pulse cycle
	float cycle = ( renderView->time - inViewTime ) / 2000.0f;

	if ( d > 0.94f ) {
		if ( !inView ) {
			inView = true;
			if ( cycle > lastCycle ) {
				// restart at the beginning
				inViewTime = renderView->time;
				cycle = 0.0f;
			}
		}
	} else {
		if ( inView ) {
			inView = false;
			lastCycle = ceil( cycle );
		}
	}

	// fade down after the last pulse finishes 
	if ( !inView && cycle > lastCycle ) {
		renderEntity->shaderParms[4] = 0.0f;
	} else {
		// pulse up in 1/4 second
		cycle -= (int)cycle;
		if ( cycle < 0.1f ) {
			renderEntity->shaderParms[4] = cycle * 10.0f;
		} else if ( cycle < 0.2f ) {
			renderEntity->shaderParms[4] = 1.0f;
		} else if ( cycle < 0.3f ) {
			renderEntity->shaderParms[4] = 1.0f - ( cycle - 0.2f ) * 10.0f;
		} else {
			// stay off between pulses
			renderEntity->shaderParms[4] = 0.0f;
		}
	}

	// update every single time this is in view
	return true;
}

/*
================
idItem::ModelCallback
================
*/
bool idItem::ModelCallback( renderEntity_t *renderEntity, const renderView_t *renderView ) {
	const idItem *ent;

	// this may be triggered by a model trace or other non-view related source
	if ( !renderView ) {
		return false;
	}

	ent = static_cast<idItem *>(gameLocal.entities[ renderEntity->entityNum ]);
	if ( !ent ) {
		gameLocal.Error( "idItem::ModelCallback: callback with NULL game entity" );
	}

	return ent->UpdateRenderEntity( renderEntity, renderView );
}

/*
================
idItem::Think
================
*/
void idItem::Think( void ) {
	if ( thinkFlags & TH_THINK ) {
		if ( spin ) {
			idAngles	ang;
			idVec3		org;

			ang.pitch = ang.roll = 0.0f;
			ang.yaw = ( gameLocal.time & 4095 ) * 360.0f / -4096.0f;
			SetAngles( ang );

			float scale = 0.005f + entityNumber * 0.00001f;
			
			org = orgOrigin;
			org.z += 4.0f + cos( ( gameLocal.time + 2000 ) * scale ) * 4.0f;
			SetOrigin( org );
		}
	}

	Present();
}

/*
================
idItem::Present
================
*/
void idItem::Present( void ) {
	idEntity::Present();

	if ( !fl.hidden && pulse ) {
		// also add a highlight shell model
		renderEntity_t	shell;

		shell = renderEntity;

		// we will mess with shader parms when the item is in view
		// to give the "item pulse" effect
		shell.callback = idItem::ModelCallback;
		shell.entityNum = entityNumber;
		shell.customShader = shellMaterial;
		if ( itemShellHandle == -1 ) {
			itemShellHandle = gameRenderWorld->AddEntityDef( &shell );
		} else {
			gameRenderWorld->UpdateEntityDef( itemShellHandle, &shell );
		}

	}
}

// sikk---> Item Management: Random Item Value
/*
================
idItem::GetRandomValue
================
*/
int idItem::GetRandomValue( const char* invName ) {
	int n = spawnArgs.GetInt( invName ) * ( 1.0f - g_itemValueFactor.GetFloat() * gameLocal.random.RandomFloat() );
	n = ( n < 1 ) ? 1 : n;
	return n;
}
// <---sikk

/*
================
idItem::Spawn
================
*/
void idItem::Spawn( void ) {
	idStr		giveTo;
	idEntity *	ent;
	float		tsize;

	if ( spawnArgs.GetBool( "dropToFloor" ) ) {
		PostEventMS( &EV_DropToFloor, 0 );
	}

	if ( spawnArgs.GetFloat( "triggersize", "0", tsize ) ) {
		GetPhysics()->GetClipModel()->LoadModel( idTraceModel( idBounds( vec3_origin ).Expand( tsize ) ) );
		GetPhysics()->GetClipModel()->Link( gameLocal.clip );
	}

	if ( spawnArgs.GetBool( "start_off" ) ) {
		GetPhysics()->SetContents( 0 );
		Hide();
	} else {
		GetPhysics()->SetContents( CONTENTS_TRIGGER );
	}

	giveTo = spawnArgs.GetString( "owner" );
	if ( giveTo.Length() ) {
		ent = gameLocal.FindEntity( giveTo );
		if ( !ent ) {
			gameLocal.Error( "Item couldn't find owner '%s'", giveTo.c_str() );
		}
		PostEventMS( &EV_Touch, 0, ent, NULL );
	}

	if ( spawnArgs.GetBool( "spin" ) || gameLocal.isMultiplayer ) {
		spin = true;
		BecomeActive( TH_THINK );
	}

	//pulse = !spawnArgs.GetBool( "nopulse" );
	//temp hack for tim
	pulse = false;
	orgOrigin = GetPhysics()->GetOrigin();

	canPickUp = !( spawnArgs.GetBool( "triggerFirst" ) || spawnArgs.GetBool( "no_touch" ) );

	inViewTime = -1000;
	lastCycle = -1;
	itemShellHandle = -1;
	shellMaterial = declManager->FindMaterial( "itemHighlightShell" );

// sikk---> Item Management: Random Item Value
	if ( g_itemValueFactor.GetFloat() ) {
		// random ammo values
		if ( spawnArgs.GetInt( "inv_ammo_shells" ) )
			spawnArgs.SetInt( "inv_ammo_shells", GetRandomValue( "inv_ammo_shells" ) );
		if ( spawnArgs.GetInt( "inv_ammo_bullets" ) )
			spawnArgs.SetInt( "inv_ammo_bullets", GetRandomValue( "inv_ammo_bullets" ) );
		if ( spawnArgs.GetInt( "inv_ammo_rockets" ) )
			spawnArgs.SetInt( "inv_ammo_rockets", GetRandomValue( "inv_ammo_rockets" ) );
		if ( spawnArgs.GetInt( "inv_ammo_cells" ) )
			spawnArgs.SetInt( "inv_ammo_cells", GetRandomValue( "inv_ammo_cells" ) );
		if ( spawnArgs.GetInt( "inv_ammo_grenades" ) )
			spawnArgs.SetInt( "inv_ammo_grenades", GetRandomValue( "inv_ammo_grenades" ) );
		if ( spawnArgs.GetInt( "inv_ammo_bfg" ) )
			spawnArgs.SetInt( "inv_ammo_bfg", GetRandomValue( "inv_ammo_bfg" ) );
		if ( spawnArgs.GetInt( "inv_ammo_clip" ) )
			spawnArgs.SetInt( "inv_ammo_clip", GetRandomValue( "inv_ammo_clip" ) );
		if ( spawnArgs.GetInt( "inv_ammo_belt" ) )
			spawnArgs.SetInt( "inv_ammo_belt", GetRandomValue( "inv_ammo_belt" ) );

		// random health values
		if ( spawnArgs.GetInt( "inv_health" ) )
			spawnArgs.SetInt( "inv_health", GetRandomValue( "inv_health" ) );

		// random armor values
		if ( spawnArgs.GetInt( "inv_armor" ) )
			spawnArgs.SetInt( "inv_armor", GetRandomValue( "inv_armor" ) );

		// random air values
		if ( spawnArgs.GetInt( "inv_air" ) )
			spawnArgs.SetInt( "inv_air", GetRandomValue( "inv_air" ) );
	}
// <---sikk
}

/*
================
idItem::GetAttributes
================
*/
void idItem::GetAttributes( idDict &attributes ) {
	int					i;
	const idKeyValue	*arg;

	for( i = 0; i < spawnArgs.GetNumKeyVals(); i++ ) {
		arg = spawnArgs.GetKeyVal( i );
		if ( arg->GetKey().Left( 4 ) == "inv_" ) {
			attributes.Set( arg->GetKey().Right( arg->GetKey().Length() - 4 ), arg->GetValue() );
		}
	}
}

/*
================
idItem::GiveToPlayer
================
*/
bool idItem::GiveToPlayer( idPlayer *player ) {
	int numPistols;		// doomtrinity-dual weapon->
	int numShotguns;	// doomtrinity-dual weapon->

	if ( player == NULL ) {
		return false;
	}

// sikk---> Item Management: Manual Item Pickup
	if ( noPickup )
		return false;
// <---sikk

	if ( spawnArgs.GetBool( "inv_carry" ) ) {
		return player->GiveInventoryItem( &spawnArgs );
	} 
// doomtrinity-dual weapon->
	numPistols = player->inventory.pistolInInventory;
	numShotguns = player->inventory.shotgunDoubleInInventory;

	if ( ( numPistols != WEAPON_COUNT_STOP ) || ( numShotguns != WEAPON_COUNT_STOP ) ) {
		idStr inv_weapon;
		inv_weapon = spawnArgs.GetString( va("inv_weapon") );
		if ( ( player->inventory.weapons & 2 ) && ( numPistols != WEAPON_COUNT_STOP ) && ( inv_weapon == "weapon_pistol" ) ) {
			player->inventory.pistolInInventory++;
			//gameLocal.Printf( "pistol # %i\n", player->inventory.pistolInInventory );
		} else if ( ( player->inventory.weapons & 8 ) && ( numShotguns != WEAPON_COUNT_STOP ) && ( inv_weapon == "weapon_shotgun_double" ) ) {
			player->inventory.shotgunDoubleInInventory++;
			//gameLocal.Printf( "shotgun # %i\n", player->inventory.shotgunDoubleInInventory );
		}
	}
// doomtrinity-dual weapon-<

	return player->GiveItem( this );
}

/*
================
idItem::Pickup
================
*/
bool idItem::Pickup( idPlayer *player ) {
	
	if ( !GiveToPlayer( player ) ) {
		return false;
	}

	if ( gameLocal.isServer ) {
		ServerSendEvent( EVENT_PICKUP, NULL, false, -1 );
	}

	// play pickup sound
	StartSound( "snd_acquire", SND_CHANNEL_ITEM, 0, false, NULL );

	// trigger our targets
	ActivateTargets( player );

	// clear our contents so the object isn't picked up twice
	GetPhysics()->SetContents( 0 );

	// hide the model
	Hide();

	// add the highlight shell
	if ( itemShellHandle != -1 ) {
		gameRenderWorld->FreeEntityDef( itemShellHandle );
		itemShellHandle = -1;
	}

	float respawn = spawnArgs.GetFloat( "respawn" );
	bool dropped = spawnArgs.GetBool( "dropped" );
	bool no_respawn = spawnArgs.GetBool( "no_respawn" );

	if ( gameLocal.isMultiplayer && respawn == 0.0f ) {
		respawn = 20.0f;
	}

	if ( respawn && !dropped && !no_respawn ) {
		const char *sfx = spawnArgs.GetString( "fxRespawn" );
		if ( sfx && *sfx ) {
			PostEventSec( &EV_RespawnFx, respawn - 0.5f );
		} 
		PostEventSec( &EV_RespawnItem, respawn );
	} else if ( !spawnArgs.GetBool( "inv_objective" ) && !no_respawn ) {
		// give some time for the pickup sound to play
		// FIXME: Play on the owner
		if ( !spawnArgs.GetBool( "inv_carry" ) ) {
			PostEventMS( &EV_Remove, 5000 );
		}
	}

	noPickup = true;	// sikk - Item Management: Manual Item Pickup

	BecomeInactive( TH_THINK );
	return true;
}

/*
================
idItem::ClientPredictionThink
================
*/
void idItem::ClientPredictionThink( void ) {
	// only think forward because the state is not synced through snapshots
	if ( !gameLocal.isNewFrame ) {
		return;
	}
	Think();
}

/*
================
idItem::WriteFromSnapshot
================
*/
void idItem::WriteToSnapshot( idBitMsgDelta &msg ) const {
	msg.WriteBits( IsHidden(), 1 );
}

/*
================
idItem::ReadFromSnapshot
================
*/
void idItem::ReadFromSnapshot( const idBitMsgDelta &msg ) {
	if ( msg.ReadBits( 1 ) ) {
		Hide();
	} else {
		Show();
	}
}

/*
================
idItem::ClientReceiveEvent
================
*/
bool idItem::ClientReceiveEvent( int event, int time, const idBitMsg &msg ) {
	switch( event ) {
		case EVENT_PICKUP: {

			// play pickup sound
			StartSound( "snd_acquire", SND_CHANNEL_ITEM, 0, false, NULL );

			// hide the model
			Hide();

			// remove the highlight shell
			if ( itemShellHandle != -1 ) {
				gameRenderWorld->FreeEntityDef( itemShellHandle );
				itemShellHandle = -1;
			}
			return true;
		}
		case EVENT_RESPAWN: {
			Event_Respawn();
			return true;
		}
		case EVENT_RESPAWNFX: {
			Event_RespawnFx();
			return true;
		}
		default: {
			return idEntity::ClientReceiveEvent( event, time, msg );
		}
	}
//	return false;	// sikk - warning C4702: unreachable code
}

/*
================
idItem::Event_DropToFloor
================
*/
void idItem::Event_DropToFloor( void ) {
	trace_t trace;

	// don't drop the floor if bound to another entity
	if ( GetBindMaster() != NULL && GetBindMaster() != this ) {
		return;
	}

	gameLocal.clip.TraceBounds( trace, renderEntity.origin, renderEntity.origin - idVec3( 0, 0, 64 ), renderEntity.bounds, MASK_SOLID | CONTENTS_CORPSE, this );
	SetOrigin( trace.endpos );
}

/*
================
idItem::Event_Touch
================
*/
void idItem::Event_Touch( idEntity *other, trace_t *trace ) {
	if ( !other->IsType( idPlayer::Type ) ) {
		return;
	}

	if ( !canPickUp ) {
		return;
	}

// sikk---> Manual Item Pickup
	if ( !g_itemPickupType.GetBool() || spawnArgs.GetBool( "autopickup" ) )
		Pickup( static_cast<idPlayer *>(other) );
// <---sikk
}

/*
================
idItem::Event_Trigger
================
*/
void idItem::Event_Trigger( idEntity *activator ) {

	if ( !canPickUp && spawnArgs.GetBool( "triggerFirst" ) ) {
		canPickUp = true;
		return;
	}

	if ( activator && activator->IsType( idPlayer::Type ) ) {
		Pickup( static_cast<idPlayer *>( activator ) );
	}
}

/*
================
idItem::Event_Respawn
================
*/
void idItem::Event_Respawn( void ) {
	if ( gameLocal.isServer ) {
		ServerSendEvent( EVENT_RESPAWN, NULL, false, -1 );
	}
	BecomeActive( TH_THINK );
	Show();
	inViewTime = -1000;
	lastCycle = -1;
	GetPhysics()->SetContents( CONTENTS_TRIGGER );
	SetOrigin( orgOrigin );
	StartSound( "snd_respawn", SND_CHANNEL_ITEM, 0, false, NULL );
	CancelEvents( &EV_RespawnItem ); // don't double respawn
}

/*
================
idItem::Event_RespawnFx
================
*/
void idItem::Event_RespawnFx( void ) {
	if ( gameLocal.isServer ) {
		ServerSendEvent( EVENT_RESPAWNFX, NULL, false, -1 );
	}
	const char *sfx = spawnArgs.GetString( "fxRespawn" );
	if ( sfx && *sfx ) {
		idEntityFx::StartFx( sfx, NULL, NULL, this, true );
	}
}

/*
===============================================================================

  idItemPowerup

===============================================================================
*/

/*
===============
idItemPowerup
===============
*/

CLASS_DECLARATION( idItem, idItemPowerup )
END_CLASS

/*
================
idItemPowerup::idItemPowerup
================
*/
idItemPowerup::idItemPowerup() {
	time = 0;
	type = 0;
}

/*
================
idItemPowerup::Save
================
*/
void idItemPowerup::Save( idSaveGame *savefile ) const {
	savefile->WriteInt( time );
	savefile->WriteInt( type );
}

/*
================
idItemPowerup::Restore
================
*/
void idItemPowerup::Restore( idRestoreGame *savefile ) {
	savefile->ReadInt( time );
	savefile->ReadInt( type );
}

/*
================
idItemPowerup::Spawn
================
*/
void idItemPowerup::Spawn( void ) {
	time = spawnArgs.GetInt( "time", "30" );
	type = spawnArgs.GetInt( "type", "0" );
}

/*
================
idItemPowerup::GiveToPlayer
================
*/
bool idItemPowerup::GiveToPlayer( idPlayer *player ) {
	if ( player->spectating ) {
		return false;
	}

// sikk---> Adrenaline Pack System
//	player->GivePowerUp( type, time * 1000 );
//	return true;
	return player->GivePowerUp( type, time * 1000 );
// <---sikk
}

/*
===============================================================================

  idObjective

===============================================================================
*/

CLASS_DECLARATION( idItem, idObjective )
	EVENT( EV_Activate,			idObjective::Event_Trigger )
	EVENT( EV_HideObjective,	idObjective::Event_HideObjective )
	EVENT( EV_GetPlayerPos,		idObjective::Event_GetPlayerPos )
	EVENT( EV_CamShot,			idObjective::Event_CamShot )
END_CLASS

/*
================
idObjective::idObjective
================
*/
idObjective::idObjective() {
	playerPos.Zero();
}

/*
================
idObjective::Save
================
*/
void idObjective::Save( idSaveGame *savefile ) const {
	savefile->WriteVec3( playerPos );
}

/*
================
idObjective::Restore
================
*/
void idObjective::Restore( idRestoreGame *savefile ) {
	savefile->ReadVec3( playerPos );
	PostEventMS( &EV_CamShot, 250 );
}

/*
================
idObjective::Spawn
================
*/
void idObjective::Spawn( void ) {
	Hide();
	PostEventMS( &EV_CamShot, 250 );
}

/*
================
idObjective::Event_Screenshot
================
*/
void idObjective::Event_CamShot( ) {
	const char *camName;
	idStr shotName = gameLocal.GetMapName();
	shotName.StripFileExtension();
	shotName += "/";
	shotName += spawnArgs.GetString( "screenshot" );
	shotName.SetFileExtension( ".tga" );
	if ( spawnArgs.GetString( "camShot", "", &camName ) ) {
		idEntity *ent = gameLocal.FindEntity( camName );
		if ( ent && ent->cameraTarget ) {
			const renderView_t *view = ent->cameraTarget->GetRenderView();
			renderView_t fullView = *view;
			fullView.width = SCREEN_WIDTH;
			fullView.height = SCREEN_HEIGHT;

// sikk---> Portal Sky Box
			// HACK : always draw sky-portal view if there is one in the map, this isn't real-time
			if ( gameLocal.portalSkyEnt.GetEntity() && g_enablePortalSky.GetBool() ) {
				renderView_t portalView = fullView;
				portalView.vieworg = gameLocal.portalSkyEnt.GetEntity()->GetPhysics()->GetOrigin();

				// setup global fixup projection vars
				int i, w, h;

				renderSystem->GetGLSettings( w, h );
				for ( i = 1; i < w; i <<= 1 ) {}
				fullView.shaderParms[4] = (float)w / (float)i;
				for ( i = 1; i < h; i <<= 1 ) {}
				fullView.shaderParms[5] = (float)h / (float)i;
				gameRenderWorld->RenderScene( &portalView );
				renderSystem->CaptureRenderToImage( "_currentRender" );
			}
// <---sikk

			// draw a view to a texture
			renderSystem->CropRenderSize( 256, 256, true );
			gameRenderWorld->RenderScene( &fullView );
			renderSystem->CaptureRenderToFile( shotName );
			renderSystem->UnCrop();
		}
	}
}

/*
================
idObjective::Event_Trigger
================
*/
void idObjective::Event_Trigger( idEntity *activator ) {
	idPlayer *player = gameLocal.GetLocalPlayer();
	if ( player ) {

		//Pickup( player );

		if ( spawnArgs.GetString( "inv_objective", NULL ) ) {
	 		if ( player && player->hud ) {
				idStr shotName = gameLocal.GetMapName();
				shotName.StripFileExtension();
				shotName += "/";
				shotName += spawnArgs.GetString( "screenshot" );
				shotName.SetFileExtension( ".tga" );
				player->hud->SetStateString( "screenshot", shotName );
				player->hud->SetStateString( "objective", "1" );
				player->hud->SetStateString( "objectivetext", spawnArgs.GetString( "objectivetext" ) );
				player->hud->SetStateString( "objectivetitle", spawnArgs.GetString( "objectivetitle" ) );
				player->GiveObjective( spawnArgs.GetString( "objectivetitle" ), spawnArgs.GetString( "objectivetext" ), shotName );

				// a tad slow but keeps from having to update all objectives in all maps with a name ptr
				for( int i = 0; i < gameLocal.num_entities; i++ ) {
					if ( gameLocal.entities[ i ] && gameLocal.entities[ i ]->IsType( idObjectiveComplete::Type ) ) {
						if ( idStr::Icmp( spawnArgs.GetString( "objectivetitle" ), gameLocal.entities[ i ]->spawnArgs.GetString( "objectivetitle" ) ) == 0 ){
							gameLocal.entities[ i ]->spawnArgs.SetBool( "objEnabled", true );
							break;
						}
					}
				}

				PostEventMS( &EV_GetPlayerPos, 2000 );
			}
		}
	}
}

/*
================
idObjective::Event_GetPlayerPos
================
*/
void idObjective::Event_GetPlayerPos() {
	idPlayer *player = gameLocal.GetLocalPlayer();
	if ( player ) {
		playerPos = player->GetPhysics()->GetOrigin();
		PostEventMS( &EV_HideObjective, 100, player );
	}
}

/*
================
idObjective::Event_HideObjective
================
*/
void idObjective::Event_HideObjective(idEntity *e) {
	idPlayer *player = gameLocal.GetLocalPlayer();
	if ( player ) {
		idVec3 v = player->GetPhysics()->GetOrigin() - playerPos;
		if ( v.Length() > 64.0f ) {
			player->HideObjective();
			PostEventMS( &EV_Remove, 0 );
		} else {
			PostEventMS( &EV_HideObjective, 100, player );
		}
	}
}

/*
===============================================================================

  idVideoCDItem

===============================================================================
*/

CLASS_DECLARATION( idItem, idVideoCDItem )
END_CLASS

/*
================
idVideoCDItem::Spawn
================
*/
void idVideoCDItem::Spawn( void ) {
}

/*
================
idVideoCDItem::GiveToPlayer
================
*/
bool idVideoCDItem::GiveToPlayer( idPlayer *player ) {
	idStr str = spawnArgs.GetString( "video" );
	if ( player && str.Length() ) {
		player->GiveVideo( str, &spawnArgs );
	}
	return true;
}

/*
===============================================================================

  idPDAItem

===============================================================================
*/

CLASS_DECLARATION( idItem, idPDAItem )
END_CLASS

/*
================
idPDAItem::GiveToPlayer
================
*/
bool idPDAItem::GiveToPlayer(idPlayer *player) {
	const char *str = spawnArgs.GetString( "pda_name" );
	if ( player ) {
		player->GivePDA( str, &spawnArgs );
	}
	return true;
}

/*
===============================================================================

  idMoveableItem
	
===============================================================================
*/

CLASS_DECLARATION( idItem, idMoveableItem )
	EVENT( EV_DropToFloor,	idMoveableItem::Event_DropToFloor )
	EVENT( EV_Gib,			idMoveableItem::Event_Gib )
END_CLASS

/*
================
idMoveableItem::idMoveableItem
================
*/
idMoveableItem::idMoveableItem() {
	trigger = NULL;
	smoke = NULL;
	smokeTime = 0;

	nextSoundTime = 0;	// sikk - Moveable Items Collision Sound
}

/*
================
idMoveableItem::~idMoveableItem
================
*/
idMoveableItem::~idMoveableItem() {
	if ( trigger ) {
		delete trigger;
	}
}

/*
================
idMoveableItem::Save
================
*/
void idMoveableItem::Save( idSaveGame *savefile ) const {
   	savefile->WriteStaticObject( physicsObj );

	savefile->WriteClipModel( trigger );

	savefile->WriteParticle( smoke );
	savefile->WriteInt( smokeTime );

	savefile->WriteInt( nextSoundTime );	// sikk - Moveable Items Collision Sound
}

/*
================
idMoveableItem::Restore
================
*/
void idMoveableItem::Restore( idRestoreGame *savefile ) {
	savefile->ReadStaticObject( physicsObj );
	RestorePhysics( &physicsObj );

	savefile->ReadClipModel( trigger );

	savefile->ReadParticle( smoke );
	savefile->ReadInt( smokeTime );

	savefile->ReadInt( nextSoundTime );	// sikk - Moveable Items Collision Sound
}

/*
================
idMoveableItem::Spawn
================
*/
void idMoveableItem::Spawn( void ) {
	idTraceModel trm;
	float density, friction, bouncyness, tsize;
	idStr clipModelName;
	idBounds bounds;

	// create a trigger for item pickup
	spawnArgs.GetFloat( "triggersize", "24.0", tsize );	// sikk - Increased default triggersize from 16 to 24
	trigger = new idClipModel( idTraceModel( idBounds( vec3_origin ).Expand( tsize ) ) );
	trigger->Link( gameLocal.clip, this, 0, GetPhysics()->GetOrigin(), GetPhysics()->GetAxis() );
	trigger->SetContents( CONTENTS_TRIGGER );

	// check if a clip model is set
	spawnArgs.GetString( "clipmodel", "", clipModelName );
	if ( !clipModelName[0] ) {
		clipModelName = spawnArgs.GetString( "model" );		// use the visual model
	}

	// load the trace model
	if ( !collisionModelManager->TrmFromModel( clipModelName, trm ) ) {
		gameLocal.Error( "idMoveableItem '%s': cannot load collision model %s", name.c_str(), clipModelName.c_str() );
		return;
	}

	// if the model should be shrinked
	if ( spawnArgs.GetBool( "clipshrink" ) ) {
		trm.Shrink( CM_CLIP_EPSILON );
	}

	// get rigid body properties
	spawnArgs.GetFloat( "density", "0.5", density );
	density = idMath::ClampFloat( 0.001f, 1000.0f, density );
	spawnArgs.GetFloat( "friction", "0.05", friction );
	friction = idMath::ClampFloat( 0.0f, 1.0f, friction );
	spawnArgs.GetFloat( "bouncyness", "0.6", bouncyness );
	bouncyness = idMath::ClampFloat( 0.0f, 1.0f, bouncyness );

// sikk---> Temp Fix for moveable items that spawn inside geometry
	idVec3 offset = idVec3( 0.0f, 0.0f, 0.0f );
	if ( !idStr::Icmp( spawnArgs.GetString( "bind" ), "" ) )
		offset = idVec3( 0.0f, 0.0f, 4.0f );
// <---sikk

	// setup the physics
	physicsObj.SetSelf( this );
	physicsObj.SetClipModel( new idClipModel( trm ), density );
	physicsObj.SetOrigin( GetPhysics()->GetOrigin() + offset );	// sikk - Temp Fix for moveable items that spawn inside geometry
	physicsObj.SetAxis( GetPhysics()->GetAxis() );
	physicsObj.SetBouncyness( bouncyness );
	physicsObj.SetFriction( 0.6f, 0.6f, friction );
	physicsObj.SetGravity( gameLocal.GetGravity() );
// sikk---> We want moveable items to clip with other items and we also want ragdolls to clip with items 
	//physicsObj.SetContents( CONTENTS_RENDERMODEL );
	//physicsObj.SetClipMask( MASK_SOLID | CONTENTS_MOVEABLECLIP );
	physicsObj.SetContents( CONTENTS_RENDERMODEL | CONTENTS_CORPSE );
	physicsObj.SetClipMask( MASK_SOLID | CONTENTS_CORPSE | CONTENTS_MOVEABLECLIP | CONTENTS_RENDERMODEL );
// <---sikk
	SetPhysics( &physicsObj );

	smoke = NULL;
	smokeTime = 0;

	nextSoundTime = 0;	// sikk - Moveable Items Collision Sound

	const char *smokeName = spawnArgs.GetString( "smoke_trail" );
	if ( *smokeName != '\0' ) {
		smoke = static_cast<const idDeclParticle *>( declManager->FindType( DECL_PARTICLE, smokeName ) );
		smokeTime = gameLocal.time;
		BecomeActive( TH_UPDATEPARTICLES );
	}
}

/*
================
idMoveableItem::Think
================
*/
void idMoveableItem::Think( void ) {

	RunPhysics();

	if ( thinkFlags & TH_PHYSICS ) {
		// update trigger position
		trigger->Link( gameLocal.clip, this, 0, GetPhysics()->GetOrigin(), mat3_identity );
	}
	
	if ( thinkFlags & TH_UPDATEPARTICLES ) {
		if ( !gameLocal.smokeParticles->EmitSmoke( smoke, smokeTime, gameLocal.random.CRandomFloat(), GetPhysics()->GetOrigin(), GetPhysics()->GetAxis() ) ) {
			smokeTime = 0;
			BecomeInactive( TH_UPDATEPARTICLES );
		}
	}

	Present();
}

// sikk---> Moveable Items Collision Sound
/*
=================
idMoveableItem::Collide
=================
*/
bool idMoveableItem::Collide( const trace_t &collision, const idVec3 &velocity ) {
	float v, f;

	v = -( velocity * collision.c.normal );
	if ( v > 80 && gameLocal.time > nextSoundTime ) {
		f = v > 200 ? 1.0f : idMath::Sqrt( v - 80 ) * 0.091f;
		if ( StartSound( "snd_bounce", SND_CHANNEL_ANY, 0, false, NULL ) ) {
			// don't set the volume unless there is a bounce sound as it overrides the entire channel
			// which causes footsteps on ai's to not honor their shader parms
			SetSoundVolume( f );
		}
		nextSoundTime = gameLocal.time + 500;
	}

	return false;
}
// <---sikk

/*
================
idMoveableItem::Pickup
================
*/
bool idMoveableItem::Pickup( idPlayer *player ) {
	bool ret = idItem::Pickup( player );
	if ( ret ) {
		trigger->SetContents( 0 );
	} 
	return ret;
}

/*
================
idMoveableItem::DropItem
================
*/
idEntity *idMoveableItem::DropItem( const char *classname, const idVec3 &origin, const idMat3 &axis, const idVec3 &velocity, int activateDelay, int removeDelay ) {
	idDict args;
	idEntity *item;

	args.Set( "classname", classname );
	args.Set( "dropped", "1" );

	// we sometimes drop idMoveables here, so set 'nodrop' to 1 so that it doesn't get put on the floor
	args.Set( "nodrop", "1" );

	if ( activateDelay ) {
		args.SetBool( "triggerFirst", true );
	}

	gameLocal.SpawnEntityDef( args, &item );
	if ( item ) {
		// set item position
		item->GetPhysics()->SetOrigin( origin );
		item->GetPhysics()->SetAxis( axis );
		item->GetPhysics()->SetLinearVelocity( velocity );
		item->UpdateVisuals();
		if ( activateDelay ) {
			item->PostEventMS( &EV_Activate, activateDelay, item );
		}
		if ( !removeDelay ) {
			removeDelay = 5 * 60 * 1000;
		}
		// always remove a dropped item after 5 minutes in case it dropped to an unreachable location
//		item->PostEventMS( &EV_Remove, removeDelay );	// sikk - Dropped moveable items are no longer removed
	}
	return item;
}

/*
================
idMoveableItem::DropItems

  The entity should have the following key/value pairs set:
	"def_drop<type>Item"			"item def"
	"drop<type>ItemJoint"			"joint name"
	"drop<type>ItemRotation"		"pitch yaw roll"
	"drop<type>ItemOffset"			"x y z"
	"skin_drop<type>"				"skin name"
  To drop multiple items the following key/value pairs can be used:
	"def_drop<type>Item<X>"			"item def"
	"drop<type>Item<X>Joint"		"joint name"
	"drop<type>Item<X>Rotation"		"pitch yaw roll"
	"drop<type>Item<X>Offset"		"x y z"
  where <X> is an aribtrary string.
================
*/
void idMoveableItem::DropItems( idAnimatedEntity  *ent, const char *type, idList<idEntity *> *list ) {
	const idKeyValue *kv;
	const char *skinName, *c, *jointName;
	idStr key, key2;
	idVec3 origin;
	idMat3 axis;
	idAngles angles;
	const idDeclSkin *skin;
	jointHandle_t joint;
	idEntity *item;

	// drop all items
	kv = ent->spawnArgs.MatchPrefix( va( "def_drop%sItem", type ), NULL );
	while ( kv ) {

		c = kv->GetKey().c_str() + kv->GetKey().Length();
		if ( idStr::Icmp( c - 5, "Joint" ) != 0 && idStr::Icmp( c - 8, "Rotation" ) != 0 ) {

			key = kv->GetKey().c_str() + 4;
			key2 = key;
			key += "Joint";
			key2 += "Offset";
			jointName = ent->spawnArgs.GetString( key );
			joint = ent->GetAnimator()->GetJointHandle( jointName );
			if ( !ent->GetJointWorldTransform( joint, gameLocal.time, origin, axis ) ) {
				gameLocal.Warning( "%s refers to invalid joint '%s' on entity '%s'\n", key.c_str(), jointName, ent->name.c_str() );
				origin = ent->GetPhysics()->GetOrigin();
				axis = ent->GetPhysics()->GetAxis();
			}
			if ( g_dropItemRotation.GetString()[0] ) {
				angles.Zero();
				sscanf( g_dropItemRotation.GetString(), "%f %f %f", &angles.pitch, &angles.yaw, &angles.roll );
			} else {
				key = kv->GetKey().c_str() + 4;
				key += "Rotation";
				ent->spawnArgs.GetAngles( key, "0 0 0", angles );
			}
			axis = angles.ToMat3() * axis;

			origin += ent->spawnArgs.GetVector( key2, "0 0 0" );

			item = DropItem( kv->GetValue(), origin, axis, vec3_origin, 0, 0 );
			if ( list && item ) {
				list->Append( item );
			}
		}

		kv = ent->spawnArgs.MatchPrefix( va( "def_drop%sItem", type ), kv );
	}

	// change the skin to hide all items
	skinName = ent->spawnArgs.GetString( va( "skin_drop%s", type ) );
	if ( skinName[0] ) {
		skin = declManager->FindSkin( skinName );
		ent->SetSkin( skin );
	}
}

/*
======================
idMoveableItem::WriteToSnapshot
======================
*/
void idMoveableItem::WriteToSnapshot( idBitMsgDelta &msg ) const {
	physicsObj.WriteToSnapshot( msg );
}

/*
======================
idMoveableItem::ReadFromSnapshot
======================
*/
void idMoveableItem::ReadFromSnapshot( const idBitMsgDelta &msg ) {
	physicsObj.ReadFromSnapshot( msg );
	if ( msg.HasChanged() ) {
		UpdateVisuals();
	}
}

/*
============
idMoveableItem::Gib
============
*/
void idMoveableItem::Gib( const idVec3 &dir, const char *damageDefName ) {
	// spawn smoke puff
	const char *smokeName = spawnArgs.GetString( "smoke_gib" );
	if ( *smokeName != '\0' ) {
		const idDeclParticle *smoke = static_cast<const idDeclParticle *>( declManager->FindType( DECL_PARTICLE, smokeName ) );
		gameLocal.smokeParticles->EmitSmoke( smoke, gameLocal.time, gameLocal.random.CRandomFloat(), renderEntity.origin, renderEntity.axis );
	}
	// remove the entity
	PostEventMS( &EV_Remove, 0 );
}

/*
================
idMoveableItem::Event_DropToFloor
================
*/
void idMoveableItem::Event_DropToFloor( void ) {
	// the physics will drop the moveable to the floor
}

/*
============
idMoveableItem::Event_Gib
============
*/
void idMoveableItem::Event_Gib( const char *damageDefName ) {
	Gib( idVec3( 0, 0, 1 ), damageDefName );
}

/*
===============================================================================

  idMoveablePDAItem

===============================================================================
*/

CLASS_DECLARATION( idMoveableItem, idMoveablePDAItem )
END_CLASS

/*
================
idMoveablePDAItem::GiveToPlayer
================
*/
bool idMoveablePDAItem::GiveToPlayer(idPlayer *player) {
	const char *str = spawnArgs.GetString( "pda_name" );
	if ( player ) {
		player->GivePDA( str, &spawnArgs );
	}
	return true;
}

// sikk---> Moveable Video CD
/*
===============================================================================

  idMoveableVideoCDItem

===============================================================================
*/

CLASS_DECLARATION( idMoveableItem, idMoveableVideoCDItem )
END_CLASS

/*
================
idMoveableVideoCDItem::Spawn
================
*/
void idMoveableVideoCDItem::Spawn( void ) {
}

/*
================
idMoveableVideoCDItem::GiveToPlayer
================
*/
bool idMoveableVideoCDItem::GiveToPlayer( idPlayer *player ) {
	idStr str = spawnArgs.GetString( "video" );
	if ( player && str.Length() ) {
		player->GiveVideo( str, &spawnArgs );
	}
	return true;
}
// <---sikk

// sikk---> Moveable Powerup
/*
===============================================================================

  idMoveableItemPowerup

===============================================================================
*/

/*
===============
idMoveableItemPowerup
===============
*/

CLASS_DECLARATION( idMoveableItem, idMoveableItemPowerup )
END_CLASS

/*
================
idMoveableItemPowerup::idMoveableItemPowerup
================
*/
idMoveableItemPowerup::idMoveableItemPowerup() {
	time = 0;
	type = 0;
}

/*
================
idMoveableItemPowerup::Save
================
*/
void idMoveableItemPowerup::Save( idSaveGame *savefile ) const {
	savefile->WriteInt( time );
	savefile->WriteInt( type );
}

/*
================
idMoveableItemPowerup::Restore
================
*/
void idMoveableItemPowerup::Restore( idRestoreGame *savefile ) {
	savefile->ReadInt( time );
	savefile->ReadInt( type );
}

/*
================
idMoveableItemPowerup::Spawn
================
*/
void idMoveableItemPowerup::Spawn( void ) {
	time = spawnArgs.GetInt( "time", "30" );
	type = spawnArgs.GetInt( "type", "0" );
}

/*
================
idMoveableItemPowerup::GiveToPlayer
================
*/
bool idMoveableItemPowerup::GiveToPlayer( idPlayer *player ) {
	if ( player->spectating ) {
		return false;
	}

// sikk---> Adrenaline Pack System
//	player->GivePowerUp( type, time * 1000 );
//	return true;
	return player->GivePowerUp( type, time * 1000 );
// <---sikk
}
// <---sikk

/*
===============================================================================

  idItemRemover

===============================================================================
*/

CLASS_DECLARATION( idEntity, idItemRemover )
	EVENT( EV_Activate,		idItemRemover::Event_Trigger )
END_CLASS

/*
================
idItemRemover::Spawn
================
*/
void idItemRemover::Spawn( void ) {
}

/*
================
idItemRemover::RemoveItem
================
*/
void idItemRemover::RemoveItem( idPlayer *player ) {
	const char *remove;
	
	remove = spawnArgs.GetString( "remove" );
	player->RemoveInventoryItem( remove );
}

/*
================
idItemRemover::Event_Trigger
================
*/
void idItemRemover::Event_Trigger( idEntity *activator ) {
	if ( activator->IsType( idPlayer::Type ) ) {
		RemoveItem( static_cast<idPlayer *>(activator) );
	}
}

/*
===============================================================================

  idObjectiveComplete

===============================================================================
*/

CLASS_DECLARATION( idItemRemover, idObjectiveComplete )
	EVENT( EV_Activate,			idObjectiveComplete::Event_Trigger )
	EVENT( EV_HideObjective,	idObjectiveComplete::Event_HideObjective )
	EVENT( EV_GetPlayerPos,		idObjectiveComplete::Event_GetPlayerPos )
END_CLASS

/*
================
idObjectiveComplete::idObjectiveComplete
================
*/
idObjectiveComplete::idObjectiveComplete() {
	playerPos.Zero();
}

/*
================
idObjectiveComplete::Save
================
*/
void idObjectiveComplete::Save( idSaveGame *savefile ) const {
	savefile->WriteVec3( playerPos );
}

/*
================
idObjectiveComplete::Restore
================
*/
void idObjectiveComplete::Restore( idRestoreGame *savefile ) {
	savefile->ReadVec3( playerPos );
}

/*
================
idObjectiveComplete::Spawn
================
*/
void idObjectiveComplete::Spawn( void ) {
	spawnArgs.SetBool( "objEnabled", false );
	Hide();
}

/*
================
idObjectiveComplete::Event_Trigger
================
*/
void idObjectiveComplete::Event_Trigger( idEntity *activator ) {
	if ( !spawnArgs.GetBool( "objEnabled" ) ) {
		return;
	}
	idPlayer *player = gameLocal.GetLocalPlayer();
	if ( player ) {
		RemoveItem( player );

		if ( spawnArgs.GetString( "inv_objective", NULL ) ) {
	 		if ( player->hud ) {
				player->hud->SetStateString( "objective", "2" );
				player->hud->SetStateString( "objectivetext", spawnArgs.GetString( "objectivetext" ) );
				player->hud->SetStateString( "objectivetitle", spawnArgs.GetString( "objectivetitle" ) );
				player->CompleteObjective( spawnArgs.GetString( "objectivetitle" ) );
				PostEventMS( &EV_GetPlayerPos, 2000 );
			}
		}
	}
}

/*
================
idObjectiveComplete::Event_GetPlayerPos
================
*/
void idObjectiveComplete::Event_GetPlayerPos() {
	idPlayer *player = gameLocal.GetLocalPlayer();
	if ( player ) {
		playerPos = player->GetPhysics()->GetOrigin();
		PostEventMS( &EV_HideObjective, 100, player );
	}
}

/*
================
idObjectiveComplete::Event_HideObjective
================
*/
void idObjectiveComplete::Event_HideObjective( idEntity *e ) {
	idPlayer *player = gameLocal.GetLocalPlayer();
	if ( player ) {
		idVec3 v = player->GetPhysics()->GetOrigin();
		v -= playerPos;
		if ( v.Length() > 64.0f ) {
			player->hud->HandleNamedEvent( "closeObjective" );
			PostEventMS( &EV_Remove, 0 );
		} else {
			PostEventMS( &EV_HideObjective, 100, player );
		}
	}
}

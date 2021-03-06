/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#include "lib/gml/gml.h"

#include "UnitDef.h"
#include "Unit.h"
#include "UnitHandler.h"
#include "UnitDefHandler.h"
#include "UnitLoader.h"
#include "UnitTypes/Building.h"
#include "UnitTypes/TransportUnit.h"
#include "Scripts/NullUnitScript.h"
#include "Scripts/UnitScriptFactory.h"
#include "Scripts/CobInstance.h" // for TAANG2RAD

#include "CommandAI/CommandAI.h"
#include "CommandAI/FactoryCAI.h"
#include "CommandAI/AirCAI.h"
#include "CommandAI/BuilderCAI.h"
#include "CommandAI/CommandAI.h"
#include "CommandAI/FactoryCAI.h"
#include "CommandAI/MobileCAI.h"
#include "CommandAI/TransportCAI.h"

#include "ExternalAI/EngineOutHandler.h"
#include "Game/GameHelper.h"
#include "Game/GameSetup.h"
#include "Game/GlobalUnsynced.h"
#include "Game/Player.h"
#include "Game/SelectedUnits.h"
#include "Lua/LuaRules.h"
#include "Map/Ground.h"
#include "Map/MapInfo.h"
#include "Map/ReadMap.h"

#include "Rendering/Models/IModelParser.h"
#include "Rendering/GroundFlash.h"

#include "Sim/Units/Groups/Group.h"
#include "Sim/Misc/AirBaseHandler.h"
#include "Sim/Features/Feature.h"
#include "Sim/Features/FeatureHandler.h"
#include "Sim/Misc/GlobalConstants.h"
#include "Sim/Misc/CollisionVolume.h"
#include "Sim/Misc/LosHandler.h"
#include "Sim/Misc/QuadField.h"
#include "Sim/Misc/RadarHandler.h"
#include "Sim/Misc/TeamHandler.h"
#include "Sim/Misc/Wind.h"
#include "Sim/Misc/ModInfo.h"
#include "Sim/MoveTypes/MoveDefHandler.h"
#include "Sim/MoveTypes/MoveType.h"
#include "Sim/MoveTypes/MoveTypeFactory.h"
#include "Sim/MoveTypes/ScriptMoveType.h"
#include "Sim/Projectiles/FlareProjectile.h"
#include "Sim/Projectiles/WeaponProjectiles/MissileProjectile.h"
#include "Sim/Weapons/Weapon.h"
#include "Sim/Weapons/WeaponDefHandler.h"
#include "Sim/Weapons/WeaponLoader.h"
#include "System/EventHandler.h"
#include "System/Log/ILog.h"
#include "System/Matrix44f.h"
#include "System/myMath.h"
#include "System/creg/STL_List.h"
#include "System/Sound/SoundChannels.h"
#include "System/Sync/SyncedPrimitive.h"
#include "System/Sync/SyncTracer.h"

#define PLAY_SOUNDS 1

// See end of source for member bindings
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

//! info: SlowUpdate runs each 16th GameFrame (:= twice per 32GameFrames) (a second has GAME_SPEED=30 gameframes!)
float CUnit::empDecline   = 2.0f * (float)UNIT_SLOWUPDATE_RATE / (float)GAME_SPEED / 40.0f;
bool  CUnit::spawnFeature = true;

float CUnit::expMultiplier  = 1.0f;
float CUnit::expPowerScale  = 1.0f;
float CUnit::expHealthScale = 0.7f;
float CUnit::expReloadScale = 0.4f;
float CUnit::expGrade       = 0.0f;


CUnit::CUnit() : CSolidObject(),
	unitDef(NULL),
	unitDefID(-1),
	featureDefID(-1),
	upright(true),
	travel(0.0f),
	travelPeriod(0.0f),
	power(100.0f),
	maxHealth(100.0f),
	paralyzeDamage(0.0f),
	captureProgress(0.0f),
	experience(0.0f),
	limExperience(0.0f),
	neutral(false),
	soloBuilder(NULL),
	beingBuilt(true),
	lastNanoAdd(gs->frameNum),
	repairAmount(0.0f),
	transporter(NULL),
	loadingTransportId(-1),
	buildProgress(0.0f),
	groundLevelled(true),
	terraformLeft(0.0f),
	realLosRadius(0),
	realAirLosRadius(0),
	losStatus(teamHandler->ActiveAllyTeams(), 0),
	inBuildStance(false),
	useHighTrajectory(false),
	dontUseWeapons(false),
	dontFire(false),
	deathScriptFinished(false),
	delayedWreckLevel(-1),
	restTime(0),
	outOfMapTime(0),
	shieldWeapon(NULL),
	stockpileWeapon(NULL),
	reloadSpeed(1.0f),
	maxRange(0.0f),
	haveTarget(false),
	haveManualFireRequest(false),
	lastMuzzleFlameSize(0.0f),
	lastMuzzleFlameDir(0.0f, 1.0f, 0.0f),
	armorType(0),
	category(0),
	los(NULL),
	tempNum(0),
	mapSquare(-1),
	losRadius(0),
	airLosRadius(0),
	lastLosUpdate(0),
	losHeight(0.0f),
	radarHeight(0.0f),
	radarRadius(0),
	sonarRadius(0),
	jammerRadius(0),
	sonarJamRadius(0),
	seismicRadius(0),
	seismicSignature(0.0f),
	hasRadarCapacity(false),
	oldRadarPos(0, 0),
	hasRadarPos(false),
	stealth(false),
	sonarStealth(false),
	moveType(NULL),
	prevMoveType(NULL),
	usingScriptMoveType(false),
	fpsControlPlayer(NULL),
	commandAI(NULL),
	group(NULL),
	localModel(NULL),
	script(NULL),
	condUseMetal(0.0f),
	condUseEnergy(0.0f),
	condMakeMetal(0.0f),
	condMakeEnergy(0.0f),
	uncondUseMetal(0.0f),
	uncondUseEnergy(0.0f),
	uncondMakeMetal(0.0f),
	uncondMakeEnergy(0.0f),
	metalUse(0.0f),
	energyUse(0.0f),
	metalMake(0.0f),
	energyMake(0.0f),
	metalUseI(0.0f),
	energyUseI(0.0f),
	metalMakeI(0.0f),
	energyMakeI(0.0f),
	metalUseold(0.0f),
	energyUseold(0.0f),
	metalMakeold(0.0f),
	energyMakeold(0.0f),
	energyTickMake(0.0f),
	metalExtract(0.0f),
	metalCost(100.0f),
	energyCost(0.0f),
	buildTime(100.0f),
	metalStorage(0.0f),
	energyStorage(0.0f),
	lastAttacker(NULL),
	lastAttackedPiece(NULL),
	lastAttackedPieceFrame(-1),
	lastAttackFrame(-200),
	lastFireWeapon(0),
	recentDamage(0.0f),
	attackTarget(NULL),
	attackPos(ZeroVector),
	userAttackGround(false),
	fireState(FIRESTATE_FIREATWILL),
	moveState(MOVESTATE_MANEUVER),
	activated(false),
	crashing(false),
	isDead(false),
	falling(false),
	fallSpeed(0.2f),
	inAir(false),
	inWater(false),
	flankingBonusMode(0),
	flankingBonusDir(1.0f, 0.0f, 0.0f),
	flankingBonusMobility(10.0f),
	flankingBonusMobilityAdd(0.01f),
	flankingBonusAvgDamage(1.4f),
	flankingBonusDifDamage(0.5f),
	armoredState(false),
	armoredMultiple(1.0f),
	curArmorMultiple(1.0f),
	posErrorVector(ZeroVector),
	posErrorDelta(ZeroVector),
	nextPosErrorUpdate(1),
	wantCloak(false),
	scriptCloak(0),
	cloakTimeout(128),
	curCloakTimeout(gs->frameNum),
	isCloaked(false),
	decloakDistance(0.0f),
	lastTerrainType(-1),
	curTerrainType(0),
	selfDCountdown(0),
	myTrack(NULL),
	myIcon(NULL),
	lastFlareDrop(0),
	currentFuel(0.0f),
	alphaThreshold(0.1f),
	cegDamage(1),
	noDraw(false),
	noMinimap(false),
	leaveTracks(false),
	isSelected(false),
	isIcon(false),
	iconRadius(0.0f),
	lodCount(0),
	currentLOD(0),

	lastDrawFrame(-30),
	lastUnitUpdate(0),

	stunned(false)
{
	GML::GetTicks(lastUnitUpdate);
}

CUnit::~CUnit()
{
	// clean up if we are still under movectrl here
	DisableScriptMoveType();

	if (delayedWreckLevel >= 0) {
		// NOTE: could also do this in Update() or even in CUnitKilledCB()
		// where we wouldn't need deathSpeed, but not in KillUnit() since
		// we have to wait for deathScriptFinished (but we want the delay
		// in frames between CUnitKilledCB() and the CreateWreckage() call
		// to be as short as possible to prevent position jumps)
		FeatureLoadParams params = {featureHandler->GetFeatureDefByID(featureDefID), unitDef, pos, deathSpeed, -1, team, -1, heading, buildFacing, 0};
		featureHandler->CreateWreckage(params, delayedWreckLevel - 1, true);
	}

	if (unitDef->isAirBase) {
		airBaseHandler->DeregisterAirBase(this);
	}

#ifdef TRACE_SYNC
	tracefile << "Unit died: ";
	tracefile << pos.x << " " << pos.y << " " << pos.z << " " << id << "\n";
#endif

	if (fpsControlPlayer != NULL) {
		fpsControlPlayer->StopControllingUnit();
		assert(fpsControlPlayer == NULL);
	}

	if (activated && unitDef->targfac) {
		radarhandler->radarErrorSize[allyteam] *= radarhandler->targFacEffect;
	}

	SetMetalStorage(0);
	SetEnergyStorage(0);

	delete commandAI;       commandAI       = NULL;
	delete moveType;        moveType        = NULL;
	delete prevMoveType;    prevMoveType    = NULL;

	// not all unit deletions run through KillUnit(),
	// but we always want to call this for ourselves
	UnBlock();

	// Remove us from our group, if we were in one
	SetGroup(NULL);

	if (script != &CNullUnitScript::value) {
		delete script;
		script = NULL;
	}
	// ScriptCallback may reference weapons, so delete the script first
	for (std::vector<CWeapon*>::const_iterator wi = weapons.begin(); wi != weapons.end(); ++wi) {
		delete *wi;
	}

	quadField->RemoveUnit(this);
	loshandler->DelayedFreeInstance(los);
	los = NULL;
	radarhandler->RemoveUnit(this);

	modelParser->DeleteLocalModel(localModel);
}


void CUnit::SetMetalStorage(float newStorage)
{
	teamHandler->Team(team)->metalStorage -= metalStorage;
	metalStorage = newStorage;
	teamHandler->Team(team)->metalStorage += metalStorage;
}


void CUnit::SetEnergyStorage(float newStorage)
{
	teamHandler->Team(team)->energyStorage -= energyStorage;
	energyStorage = newStorage;
	teamHandler->Team(team)->energyStorage += energyStorage;
}



void CUnit::PreInit(const UnitLoadParams& params)
{
	// if this is < 0, we get a random ID from UnitHandler
	id = params.unitID;
	unitDefID = (params.unitDef)->id;
	featureDefID = -1;

	objectDef = params.unitDef;
	unitDef = params.unitDef;

	{
		const FeatureDef* wreckFeatureDef = featureHandler->GetFeatureDef(unitDef->wreckName);

		if (wreckFeatureDef != NULL) {
			featureDefID = wreckFeatureDef->id;
		}
	}

	team = params.teamID;
	allyteam = teamHandler->AllyTeam(team);

	buildFacing = std::abs(params.facing) % NUM_FACINGS;
	xsize = ((buildFacing & 1) == 0) ? unitDef->xsize : unitDef->zsize;
	zsize = ((buildFacing & 1) == 1) ? unitDef->xsize : unitDef->zsize;

	// copy the UnitDef volume instance
	// NOTE: gets deleted in ~CSolidObject
	model = unitDef->LoadModel();
	localModel = new LocalModel(model);
	collisionVolume = new CollisionVolume(unitDef->collisionVolume);
	modelParser->CreateLocalModel(localModel);

	if (collisionVolume->DefaultToSphere())
		collisionVolume->InitSphere(model->radius);
	if (collisionVolume->DefaultToFootPrint())
		collisionVolume->InitBox(float3(xsize * SQUARE_SIZE, model->height, zsize * SQUARE_SIZE));

	speed = params.speed;
	mapSquare = ground->GetSquare((params.pos).cClampInMap());

	heading  = GetHeadingFromFacing(buildFacing);
	frontdir = GetVectorFromHeading(heading);
	updir    = UpVector;
	rightdir = frontdir.cross(updir);
	upright  = unitDef->upright;

	Move3D((params.pos).cClampInMap(), false);
	SetMidAndAimPos(model->relMidPos, model->relMidPos, true);
	SetRadiusAndHeight(model);
	UpdateDirVectors(!upright);
	UpdateMidAndAimPos();

	unitHandler->AddUnit(this);
	quadField->MovedUnit(this);

	hasRadarPos = false;

	losStatus[allyteam] =
		LOS_ALL_MASK_BITS |
		LOS_INLOS         |
		LOS_INRADAR       |
		LOS_PREVLOS       |
		LOS_CONTRADAR;

	posErrorVector = gs->randVector();
	posErrorVector.y *= 0.2f;

#ifdef TRACE_SYNC
	tracefile << "[" << __FUNCTION__ << "] id: " << id << ", name: " << unitDef->name << " ";
	tracefile << "pos: <" << pos.x << ", " << pos.y << ", " << pos.z << ">\n";
#endif

	ASSERT_SYNCED(pos);

	blockMap = (unitDef->GetYardMap().empty())? NULL: &unitDef->GetYardMap()[0];
	footprint = int2(unitDef->xsize, unitDef->zsize);

	beingBuilt = params.beingBuilt;
	mass = (beingBuilt)? mass: unitDef->mass;
	crushResistance = unitDef->crushResistance;
	power = unitDef->power;
	maxHealth = unitDef->health;
	health = beingBuilt? 0.1f: unitDef->health;
	losHeight = unitDef->losHeight;
	radarHeight = unitDef->radarHeight;
	metalCost = unitDef->metal;
	energyCost = unitDef->energy;
	buildTime = unitDef->buildTime;
	currentFuel = unitDef->maxFuel;
	armoredMultiple = std::max(0.0001f, unitDef->armoredMultiple); // armored multiple of 0 will crash spring
	armorType = unitDef->armorType;
	category = unitDef->category;
	leaveTracks = unitDef->decalDef.leaveTrackDecals;

	tooltip = unitDef->humanName + " - " + unitDef->tooltip;


	// sensor parameters
	realLosRadius = int(unitDef->losRadius);
	realAirLosRadius = int(unitDef->airLosRadius);

	radarRadius      = unitDef->radarRadius    / (SQUARE_SIZE * 8);
	sonarRadius      = unitDef->sonarRadius    / (SQUARE_SIZE * 8);
	jammerRadius     = unitDef->jammerRadius   / (SQUARE_SIZE * 8);
	sonarJamRadius   = unitDef->sonarJamRadius / (SQUARE_SIZE * 8);
	seismicRadius    = unitDef->seismicRadius  / (SQUARE_SIZE * 8);
	seismicSignature = unitDef->seismicSignature;
	hasRadarCapacity =
		(radarRadius   > 0.0f) || (sonarRadius    > 0.0f) ||
		(jammerRadius  > 0.0f) || (sonarJamRadius > 0.0f) ||
		(seismicRadius > 0.0f);
	stealth = unitDef->stealth;
	sonarStealth = unitDef->sonarStealth;
	decloakDistance = unitDef->decloakDistance;
	cloakTimeout = unitDef->cloakTimeout;

	// <inWater> and friends are still false at this point
	if (unitDef->floatOnWater && (pos.y <= 0.0f)) {
		Move1D(-unitDef->waterline, 1, false);
	}

	flankingBonusMode        = unitDef->flankingBonusMode;
	flankingBonusDir         = unitDef->flankingBonusDir;
	flankingBonusMobility    = unitDef->flankingBonusMobilityAdd * 1000;
	flankingBonusMobilityAdd = unitDef->flankingBonusMobilityAdd;
	flankingBonusAvgDamage   = (unitDef->flankingBonusMax + unitDef->flankingBonusMin) * 0.5f;
	flankingBonusDifDamage   = (unitDef->flankingBonusMax - unitDef->flankingBonusMin) * 0.5f;

	useHighTrajectory = (unitDef->highTrajectoryType == 1);

	energyTickMake =
		unitDef->energyMake +
		unitDef->tidalGenerator * mapInfo->map.tidalStrength;

	moveType = MoveTypeFactory::GetMoveType(this, unitDef);
	script = CUnitScriptFactory::CreateScript("scripts/" + unitDef->scriptName, this);
}


void CUnit::PostInit(const CUnit* builder)
{
	weaponLoader->LoadWeapons(this);
	// Call initializing script functions
	script->Create();

	if (moveDef != NULL) {
		switch (moveDef->moveFamily) {
			case MoveDef::Hover: {
				physicalState = CSolidObject::Hovering;
			} break;
			case MoveDef::Ship: {
				physicalState = CSolidObject::Floating;
			} break;
			default: {
				physicalState = CSolidObject::OnGround;
			} break;
		}
	} else {
		physicalState = (unitDef->floatOnWater)?
			CSolidObject::Floating:
			CSolidObject::OnGround;
	}

	// all units are blocking (ie. register on the blocking-map
	// when not flying) except mines, since their position would
	// be given away otherwise by the PF, etc.
	// NOTE: this does mean that mines can be stacked indefinitely
	// (an extra yardmap character would be needed to prevent this)
	immobile = unitDef->IsImmobileUnit();
	blocking = unitDef->blocking;
	blocking &= !(immobile && unitDef->canKamikaze);

	if (blocking) {
		Block();
	}

	if (unitDef->windGenerator > 0.0f) {
		wind.AddUnit(this);
	}

	UpdateTerrainType();

	if (unitDef->canmove || unitDef->builder) {
		if (unitDef->moveState <= FIRESTATE_NONE) {
			if (builder != NULL) {
				// always inherit our builder's movestate
				// if none, set CUnit's default (maneuver)
				moveState = builder->moveState;
			}
		} else {
			// use our predefined movestate
			moveState = unitDef->moveState;
		}

		Command c(CMD_MOVE_STATE, 0, moveState);
		commandAI->GiveCommand(c);
	}

	if (commandAI->CanChangeFireState()) {
		if (unitDef->fireState <= MOVESTATE_NONE) {
			if (builder != NULL && dynamic_cast<CFactoryCAI*>(builder->commandAI) != NULL) {
				// inherit our builder's firestate (if it is a factory)
				// if no builder, CUnit's default (fire-at-will) is set
				fireState = builder->fireState;
			}
		} else {
			// use our predefined firestate
			fireState = unitDef->fireState;
		}

		Command c(CMD_FIRE_STATE, 0, fireState);
		commandAI->GiveCommand(c);
	}

	// Lua might call SetUnitHealth within UnitCreated
	// and trigger FinishedBuilding before we get to it
	const bool preBeingBuilt = beingBuilt;

	// these must precede UnitFinished from FinishedBuilding
	eventHandler.UnitCreated(this, builder);
	eoh->UnitCreated(*this, builder);

	if (!preBeingBuilt && !beingBuilt) {
		// skip past the gradual build-progression
		FinishedBuilding(true);
	}
}




void CUnit::ForcedMove(const float3& newPos)
{
	if (blocking) {
		UnBlock();
	}

	Move3D(newPos - pos, true);

	eventHandler.UnitMoved(this);

	if (blocking) {
		Block();
	}

	quadField->MovedUnit(this);
	loshandler->MoveUnit(this, false);
	radarhandler->MoveUnit(this);
}


void CUnit::ForcedSpin(const float3& newDir)
{
	assert(math::fabsf(newDir.SqLength() - 1.0f) <= float3::NORMALIZE_EPS);

	updir = UpVector;
	if (updir == newDir) {
		//FIXME perhaps save the old right,up,front directions, so we can
		// reconstruct the old upvector and generate a better assumption for updir
		updir -= GetVectorFromHeading(heading);
	}
	frontdir = newDir;
	rightdir = newDir.cross(updir).Normalize();
	updir    = rightdir.cross(newDir);
	heading  = GetHeadingFromVector(newDir.x, newDir.z);

	UpdateMidAndAimPos();
	ForcedMove(pos);
}


void CUnit::SetHeadingFromDirection()
{
	heading = GetHeadingFromVector(frontdir.x, frontdir.z);
}

void CUnit::SetDirVectors(const CMatrix44f& matrix) {
	rightdir.x = -matrix[ 0];
	rightdir.y = -matrix[ 1];
	rightdir.z = -matrix[ 2];
	updir.x    =  matrix[ 4];
	updir.y    =  matrix[ 5];
	updir.z    =  matrix[ 6];
	frontdir.x =  matrix[ 8];
	frontdir.y =  matrix[ 9];
	frontdir.z =  matrix[10];
}

// NOTE: movetypes call this directly
void CUnit::UpdateDirVectors(bool useGroundNormal, bool isGroundUnit)
{
	if (isGroundUnit && inAir)
		return;

	updir    = useGroundNormal? ground->GetSmoothNormal(pos.x, pos.z): UpVector;
	frontdir = GetVectorFromHeading(heading);
	rightdir = (frontdir.cross(updir)).Normalize();
	frontdir = updir.cross(rightdir);
}



void CUnit::Drop(const float3& parentPos, const float3& parentDir, CUnit* parent)
{
	// drop unit from position
	fallSpeed = unitDef->unitFallSpeed > 0 ? unitDef->unitFallSpeed : parent->unitDef->fallSpeed;
	falling = true;

	speed.y = 0.0f;
	frontdir = parentDir;
	frontdir.y = 0.0f;

	Move1D(parentPos.y - height, 1, false);
	UpdateMidAndAimPos();
	// start parachute animation
	script->Falling();
}


void CUnit::EnableScriptMoveType()
{
	if (usingScriptMoveType) {
		return;
	}
	prevMoveType = moveType;
	moveType = new CScriptMoveType(this);
	usingScriptMoveType = true;
}

void CUnit::DisableScriptMoveType()
{
	if (!usingScriptMoveType) {
		return;
	}

	delete moveType;
	moveType = prevMoveType;
	prevMoveType = NULL;
	usingScriptMoveType = false;

	// FIXME: prevent the issuing of extra commands ?
	if (moveType) {
		moveType->oldPos = pos;
		moveType->SetGoal(pos);
		moveType->StopMoving();
	}
}


void CUnit::Update()
{
	ASSERT_SYNCED(pos);

	// UnitScript only applies piece-space transforms so
	// we apply the forward kinematics update separately
	// (only if we have any dirty pieces)
	// TODO: move this to UnitScript::Tick?
	localModel->UpdatePieceMatrices();

	{
		const bool oldInAir   = inAir;
		const bool oldInWater = inWater;

		inWater = (pos.y <= 0.0f);
		inAir   = (!inWater) && ((pos.y - ground->GetHeightAboveWater(pos.x, pos.z)) > 1.0f);
		isUnderWater = ((pos.y + ((moveDef == NULL || !moveDef->subMarine) * model->height)) < 0.0f);

		if (inAir != oldInAir) {
			if (inAir) {
				eventHandler.UnitEnteredAir(this);
			} else {
				eventHandler.UnitLeftAir(this);
			}
		}
		if (inWater != oldInWater) {
			if (inWater) {
				eventHandler.UnitEnteredWater(this);
			} else {
				eventHandler.UnitLeftWater(this);
			}
		}
	}

	residualImpulse *= impulseDecayRate;
	posErrorVector += posErrorDelta;

	if (beingBuilt)
		return;

	if (travelPeriod != 0.0f) {
		travel += speed.Length();
		travel = math::fmod(travel, travelPeriod);
	}

	recentDamage *= 0.9f;
	flankingBonusMobility += flankingBonusMobilityAdd;

	if (IsStunned()) {
		// leave the pad if reserved
		moveType->UnreservePad(moveType->GetReservedPad());

		// paralyzed weapons shouldn't reload
		for (std::vector<CWeapon*>::iterator wi = weapons.begin(); wi != weapons.end(); ++wi) {
			++(*wi)->reloadStatus;
		}
		return;
	}

	restTime++;
	outOfMapTime = (pos.IsInBounds())? 0: outOfMapTime + 1;

	if (!dontUseWeapons) {
		for (std::vector<CWeapon*>::iterator wi = weapons.begin(); wi != weapons.end(); ++wi) {
			(*wi)->Update();
		}
	}
}


void CUnit::UpdateResources()
{
	metalMake  = metalMakeI  + metalMakeold;
	metalUse   = metalUseI   + metalUseold;
	energyMake = energyMakeI + energyMakeold;
	energyUse  = energyUseI  + energyUseold;

	metalMakeold  = metalMakeI;
	metalUseold   = metalUseI;
	energyMakeold = energyMakeI;
	energyUseold  = energyUseI;

	metalMakeI = metalUseI = energyMakeI = energyUseI = 0.0f;
}

void CUnit::SetLosStatus(int at, unsigned short newStatus)
{
	const unsigned short currStatus = losStatus[at];
	const unsigned short diffBits = (currStatus ^ newStatus);

	// add to the state before running the callins
	//
	// note that is not symmetric: UnitEntered* and
	// UnitLeft* are after-the-fact events, yet the
	// Left* call-ins would still see the old state
	// without first clearing the IN{LOS, RADAR} bit
	losStatus[at] |= newStatus;

	if (diffBits) {
		if (diffBits & LOS_INLOS) {
			if (newStatus & LOS_INLOS) {
				eventHandler.UnitEnteredLos(this, at);
				eoh->UnitEnteredLos(*this, at);
			} else {
				// clear before sending the event
				losStatus[at] &= ~LOS_INLOS;

				eventHandler.UnitLeftLos(this, at);
				eoh->UnitLeftLos(*this, at);
			}
		}

		if (diffBits & LOS_INRADAR) {
			if (newStatus & LOS_INRADAR) {
				eventHandler.UnitEnteredRadar(this, at);
				eoh->UnitEnteredRadar(*this, at);
			} else {
				// clear before sending the event
				losStatus[at] &= ~LOS_INRADAR;

				eventHandler.UnitLeftRadar(this, at);
				eoh->UnitLeftRadar(*this, at);
			}
		}
	}

	// remove from the state after running the callins
	losStatus[at] &= newStatus;
}


unsigned short CUnit::CalcLosStatus(int at)
{
	const unsigned short currStatus = losStatus[at];

	unsigned short newStatus = currStatus;
	unsigned short mask = ~(currStatus >> 8);

	if (loshandler->InLos(this, at)) {
		if (!beingBuilt) {
			newStatus |= (mask & (LOS_INLOS   | LOS_INRADAR |
			                      LOS_PREVLOS | LOS_CONTRADAR));
		} else {
			// we are being built, do not set LOS_PREVLOS
			// since we do not want ghosts for nanoframes
			newStatus |=  (mask & (LOS_INLOS   | LOS_INRADAR));
			newStatus &= ~(mask & (LOS_PREVLOS | LOS_CONTRADAR));
		}
	}
	else if (radarhandler->InRadar(this, at)) {
		newStatus |=  (mask & LOS_INRADAR);
		newStatus &= ~(mask & LOS_INLOS);
	}
	else {
		newStatus &= ~(mask & (LOS_INLOS | LOS_INRADAR | LOS_CONTRADAR));
	}

	return newStatus;
}


inline void CUnit::UpdateLosStatus(int at)
{
	const unsigned short currStatus = losStatus[at];
	if ((currStatus & LOS_ALL_MASK_BITS) == LOS_ALL_MASK_BITS) {
		return; // no need to update, all changes are masked
	}
	SetLosStatus(at, CalcLosStatus(at));
}


void CUnit::SetStunned(bool stun) {
	stunned = stun;

	if (moveType->progressState == AMoveType::Active) {
		if (stunned) {
			script->StopMoving();
		} else {
			script->StartMoving();
		}
	}
}


void CUnit::SlowUpdate()
{
	--nextPosErrorUpdate;
	if (nextPosErrorUpdate == 0) {
		float3 newPosError(gs->randVector());
		newPosError.y *= 0.2f;
		if (posErrorVector.dot(newPosError) < 0.0f) {
			newPosError = -newPosError;
		}
		posErrorDelta = (newPosError - posErrorVector) * (1.0f / 256.0f);
		nextPosErrorUpdate = 16;
	}

	for (int at = 0; at < teamHandler->ActiveAllyTeams(); ++at) {
		UpdateLosStatus(at);
	}

	DoWaterDamage();

	if (health < 0.0f) {
		KillUnit(NULL, false, true);
		return;
	}

	repairAmount = 0.0f;

	if (paralyzeDamage > 0.0f) {
		// NOTE: the paralysis degradation-rate has to vary, because
		// when units are paralyzed based on their current health (in
		// DoDamage) we potentially start decaying from a lower damage
		// level and would otherwise be de-paralyzed more quickly than
		// specified by <paralyzeTime>
		paralyzeDamage -= ((modInfo.paralyzeOnMaxHealth? maxHealth: health) * 0.5f * CUnit::empDecline);
		paralyzeDamage = std::max(paralyzeDamage, 0.0f);
	}

	UpdateResources();

	if (IsStunned()) {
		// call this because we can be pushed into a different quad while stunned
		// which would make us invulnerable to most non-/small-AOE weapon impacts
		static_cast<AMoveType*>(moveType)->SlowUpdate();

		const bool b0 = (paralyzeDamage <= (modInfo.paralyzeOnMaxHealth? maxHealth: health));
		const bool b1 = (transporter == NULL || transporter->unitDef->isFirePlatform);

		// de-stun only if we are not (still) inside a non-firebase transport
		if (b0 && b1) {
			SetStunned(false);
		}

		SlowUpdateCloak(true);
		return;
	}

	if (selfDCountdown > 0) {
		if ((selfDCountdown -= 1) == 0) {
			// avoid unfinished buildings making an explosion
			if (!beingBuilt) {
				KillUnit(NULL, true, false);
			} else {
				KillUnit(NULL, false, true);
			}
			return;
		}
		if ((selfDCountdown & 1) && (team == gu->myTeam) && !gu->spectating) {
			LOG("%s: self-destruct in %is", unitDef->humanName.c_str(), (selfDCountdown >> 1) + 1);
		}
	}

	if (beingBuilt) {
		if (modInfo.constructionDecay && (lastNanoAdd < (gs->frameNum - modInfo.constructionDecayTime))) {
			float buildDecay = buildTime * modInfo.constructionDecaySpeed;

			buildDecay = 1.0f / std::max(0.001f, buildDecay);
			buildDecay = std::min(buildProgress, buildDecay);

			health         = std::max(0.0f, health - maxHealth * buildDecay);
			buildProgress -= buildDecay;

			AddMetal(metalCost * buildDecay, false);

			if (health <= 0.0f || buildProgress <= 0.0f) {
				KillUnit(NULL, false, true);
			}
		}

		ScriptDecloak(false);
		return;
	}

	// below is stuff that should not be run while being built
	commandAI->SlowUpdate();
	moveType->SlowUpdate();

	// FIXME: scriptMakeMetal ...?
	AddMetal(uncondMakeMetal);
	AddEnergy(uncondMakeEnergy);
	UseMetal(uncondUseMetal);
	UseEnergy(uncondUseEnergy);
	if (activated) {
		if (UseMetal(condUseMetal)) {
			AddEnergy(condMakeEnergy);
		}
		if (UseEnergy(condUseEnergy)) {
			AddMetal(condMakeMetal);
		}
	}

	AddMetal(unitDef->metalMake * 0.5f);
	if (activated) {
		if (UseEnergy(unitDef->energyUpkeep * 0.5f)) {
			AddMetal(unitDef->makesMetal * 0.5f);
			if (unitDef->extractsMetal > 0.0f) {
				AddMetal(metalExtract * 0.5f);
			}
		}
		UseMetal(unitDef->metalUpkeep * 0.5f);

		if (unitDef->windGenerator > 0.0f) {
			if (wind.GetCurrentStrength() > unitDef->windGenerator) {
 				AddEnergy(unitDef->windGenerator * 0.5f);
			} else {
 				AddEnergy(wind.GetCurrentStrength() * 0.5f);
			}
		}
	}
	AddEnergy(energyTickMake * 0.5f);

	if (health < maxHealth) {
		if (restTime > unitDef->idleTime) {
			health += unitDef->idleAutoHeal;
		}

		health += unitDef->autoHeal;
		health = std::min(health, maxHealth);
	}

	SlowUpdateCloak(false);

	if (unitDef->canKamikaze) {
		if (fireState >= FIRESTATE_FIREATWILL) {
			std::vector<int> nearbyUnits;
			if (unitDef->kamikazeUseLOS) {
				CGameHelper::GetEnemyUnits(pos, unitDef->kamikazeDist, allyteam, nearbyUnits);
			} else {
				CGameHelper::GetEnemyUnitsNoLosTest(pos, unitDef->kamikazeDist, allyteam, nearbyUnits);
			}

			for (std::vector<int>::const_iterator it = nearbyUnits.begin(); it != nearbyUnits.end(); ++it) {
				const CUnit* victim = unitHandler->GetUnitUnsafe(*it);
				const float3 dif = pos - victim->pos;

				if (dif.SqLength() < Square(unitDef->kamikazeDist)) {
					if (victim->speed.dot(dif) <= 0) {
						//! self destruct when we start moving away from the target, this should maximize the damage
						KillUnit(NULL, true, false);
						return;
					}
				}
			}
		}

		if (
			   (attackTarget     && (attackTarget->pos.SqDistance(pos) < Square(unitDef->kamikazeDist)))
			|| (userAttackGround && (attackPos.SqDistance(pos))  < Square(unitDef->kamikazeDist))
		) {
			KillUnit(NULL, true, false);
			return;
		}
	}

	SlowUpdateWeapons();

	if (moveType->progressState == AMoveType::Active) {
		if (seismicSignature && !GetTransporter()) {
			DoSeismicPing((int)seismicSignature);
		}
	}

	CalculateTerrainType();
	UpdateTerrainType();
}

void CUnit::SlowUpdateWeapons() {
	if (weapons.empty()) {
		return;
	}

	haveTarget = false;

	if (!dontFire) {
		for (vector<CWeapon*>::iterator wi = weapons.begin(); wi != weapons.end(); ++wi) {
			CWeapon* w = *wi;

			w->SlowUpdate();

			// NOTE:
			//     pass w->haveUserTarget so we do not interfere with
			//     user targets; w->haveUserTarget can only be true if
			//     either 1) ::AttackUnit was called with a (non-NULL)
			//     target-unit which the CAI did *not* auto-select, or
			//     2) ::AttackGround was called with any user-selected
			//     position and all checks succeeded
			if ((haveManualFireRequest == (unitDef->canManualFire && w->weaponDef->manualfire))) {
				if (attackTarget != NULL) {
					w->AttackUnit(attackTarget, w->haveUserTarget);
				} else if (userAttackGround) {
					// this implies a user-order
					w->AttackGround(attackPos, true);
				}
			}

			if (lastAttacker == NULL)
				continue;
			if ((lastAttackFrame + 200) <= gs->frameNum)
				continue;
			if (w->targetType != Target_None)
				continue;
			if (fireState == FIRESTATE_HOLDFIRE)
				continue;

			// return fire at our last attacker if allowed
			w->AttackUnit(lastAttacker, false);
		}
	}
}



float CUnit::GetFlankingDamageBonus(const float3& attackDir)
{
	float flankingBonus = 1.0f;

	if (flankingBonusMode <= 0)
		return flankingBonus;

	if (flankingBonusMode == 1) {
		// mode 1 = global coordinates, mobile
		flankingBonusDir += (attackDir * flankingBonusMobility);
		flankingBonusDir.Normalize();
		flankingBonusMobility = 0.0f;
		flankingBonus = (flankingBonusAvgDamage - attackDir.dot(flankingBonusDir) * flankingBonusDifDamage);
	} else {
		float3 adirRelative;
		adirRelative.x = attackDir.dot(rightdir);
		adirRelative.y = attackDir.dot(updir);
		adirRelative.z = attackDir.dot(frontdir);

		if (flankingBonusMode == 2) {
			// mode 2 = unit coordinates, mobile
			flankingBonusDir += (adirRelative * flankingBonusMobility);
			flankingBonusDir.Normalize();
			flankingBonusMobility = 0.0f;
		}

		// modes 2 and 3 both use this; 3 is unit coordinates, immobile
		flankingBonus = (flankingBonusAvgDamage - adirRelative.dot(flankingBonusDir) * flankingBonusDifDamage);
	}

	return flankingBonus;
}

void CUnit::DoWaterDamage()
{
	if (mapInfo->water.damage <= 0.0f)
		return;
	if (!pos.IsInBounds())
		return;
	if (pos.y > 0.0f)
		return;

	const int  px            = pos.x / (SQUARE_SIZE * 2);
	const int  pz            = pos.z / (SQUARE_SIZE * 2);
	const bool isFloating    = (physicalState == CSolidObject::Floating);
	const bool onGround      = (physicalState == CSolidObject::OnGround);
	const bool isWaterSquare = (readmap->GetMIPHeightMapSynced(1)[pz * gs->hmapx + px] <= 0.0f);

	if (!isWaterSquare)
		return;
	if (!isFloating && !onGround)
		return;

	DoDamage(DamageArray(mapInfo->water.damage), ZeroVector, NULL, -DAMAGE_EXTSOURCE_WATER, -1);
}

void CUnit::DoDamage(const DamageArray& damages, const float3& impulse, CUnit* attacker, int weaponDefID, int projectileID)
{
	if (isDead || crashing) {
		return;
	}

	float damage = damages[armorType];

	if (damage > 0.0f) {
		if (attacker) {
			SetLastAttacker(attacker);

			// FIXME -- not the impulse direction?
			damage *= GetFlankingDamageBonus((attacker->pos - pos).SafeNormalize());
		}

		damage *= curArmorMultiple;
		restTime = 0; // bleeding != resting
	}

	{
		float3 hitDir = -impulse;
		hitDir.y = 0.0f;
		hitDir.SafeNormalize();

		script->HitByWeapon(hitDir, weaponDefID, /*inout*/ damage);
	}

	float experienceMod = expMultiplier;
	float newDamage = damage;
	float impulseMult = 1.0f;

	const int paralyzeTime = damages.paralyzeDamageTime;
	const bool isParalyzer = (paralyzeTime != 0);

	if (luaRules && luaRules->UnitPreDamaged(this, attacker, damage, weaponDefID, projectileID, isParalyzer, &newDamage, &impulseMult)) {
		damage = newDamage;
	}

	StoreImpulse((impulse * impulseMult) / mass);

	if (!isParalyzer) { // real damage
		if (damage > 0.0f) {
			// Do not log overkill damage (so nukes etc do not inflate values)
			const float statsdamage = std::max(0.0f, std::min(maxHealth - health, damage));
			if (attacker) {
				teamHandler->Team(attacker->team)->currentStats->damageDealt += statsdamage;
			}
			teamHandler->Team(team)->currentStats->damageReceived += statsdamage;
			health -= damage;
		} else { // healing
			health -= damage;
			health = std::min(health, maxHealth);

			if (health > paralyzeDamage && !modInfo.paralyzeOnMaxHealth) {
				SetStunned(false);
			}
		}
	} else {
		// paralyzation damage (adds reduced experience for the attacker)
		experienceMod *= 0.1f;

		// paralyzeDamage may not get higher than baseHealth * (paralyzeTime + 1),
		// which means the unit will be destunned after <paralyzeTime> seconds.
		// (maximum paralyzeTime of all paralyzer weapons which recently hit it ofc)
		//
		// rate of paralysis-damage reduction is lower if the unit has less than
		// maximum health to ensure stun-time is always equal to <paralyzeTime>
		const float baseHealth = (modInfo.paralyzeOnMaxHealth? maxHealth: health);
		const float paralysisDecayRate = baseHealth * CUnit::empDecline;
		const float sumParalysisDamage = paralysisDecayRate * paralyzeTime;
		const float maxParalysisDamage = baseHealth + sumParalysisDamage - paralyzeDamage;

		if (damage > 0.0f) {
			// clamp the dealt paralysis-damage to [0, maxParalysisDamage]
			damage = std::min(damage, std::max(0.0f, maxParalysisDamage));

			if (IsStunned()) {
				// no attacker gains experience from a stunned target
				experienceMod = 0.0f;
			}

			// increase the current level of paralysis-damage
			paralyzeDamage += damage;

			if (paralyzeDamage >= baseHealth) {
				SetStunned(true);
			}
		} else {
			if (paralyzeDamage <= 0.0f) {
				// no experience from healing a non-stunned target
				experienceMod = 0.0f;
			}

			// decrease ("heal") the current level of paralysis-damage
			paralyzeDamage += damage;
			paralyzeDamage = std::max(paralyzeDamage, 0.0f);

			if (paralyzeDamage <= baseHealth) {
				SetStunned(false);
			}
		}
	}

	recentDamage += damage;

	eventHandler.UnitDamaged(this, attacker, damage, weaponDefID, projectileID, isParalyzer);
	eoh->UnitDamaged(*this, attacker, damage, weaponDefID, projectileID, isParalyzer);

#ifdef TRACE_SYNC
	tracefile << "Damage: ";
	tracefile << id << " " << damage << "\n";
#endif

	if (damage > 0.0f) {
		if ((attacker != NULL) && !teamHandler->Ally(allyteam, attacker->allyteam)) {
			const float scaledExpMod = 0.1f * experienceMod * (power / attacker->power);
			const float scaledDamage = std::max(0.0f, (damage + std::min(0.0f, health))) / maxHealth;
			// alternative
			// scaledDamage = (max(healthPreDamage, 0) - max(health, 0)) / maxHealth

			// FIXME: why is experience added a second time when health <= 0.0f?
			attacker->AddExperience(scaledExpMod * scaledDamage);
		}
	}

	if (health <= 0.0f) {
		KillUnit(attacker, false, false);

		if (!isDead) { return; }
		if (beingBuilt) { return; }
		if (attacker == NULL) { return; }

		if (!teamHandler->Ally(allyteam, attacker->allyteam)) {
			attacker->AddExperience(expMultiplier * 0.1f * (power / attacker->power));
			teamHandler->Team(attacker->team)->currentStats->unitsKilled++;
		}
	}
}



void CUnit::StoreImpulse(const float3& impulse, float newImpulseDecayRate) {
	CSolidObject::StoreImpulse(impulse, newImpulseDecayRate);
}

void CUnit::StoreImpulse(const float3& impulse) {
	const float3& groundNormal = ground->GetNormal(pos.x, pos.z);
	const float groundImpulseScale = std::min(0.0f, residualImpulse.dot(groundNormal));

	CSolidObject::StoreImpulse(impulse - (groundNormal * groundImpulseScale * (1 - inAir)));

	if (moveType->CanApplyImpulse(impulse)) {
		CSolidObject::ApplyImpulse();
	}
}



/******************************************************************************/
/******************************************************************************/

CMatrix44f CUnit::GetTransformMatrix(const bool synced, const bool error) const
{
	float3 interPos = synced ? pos : drawPos;

	if (error && !synced && !gu->spectatingFullView) {
		interPos += CGameHelper::GetUnitErrorPos(this, gu->myAllyTeam) - midPos;
	}

	return CMatrix44f(interPos, -rightdir, updir, frontdir);
}



/******************************************************************************/
/******************************************************************************/

void CUnit::ChangeSensorRadius(int* valuePtr, int newValue)
{
	radarhandler->RemoveUnit(this);

	*valuePtr = newValue;

	if (newValue != 0) {
		hasRadarCapacity = true;
	} else if (hasRadarCapacity) {
		hasRadarCapacity = (radarRadius   > 0.0f) || (jammerRadius   > 0.0f) ||
		                   (sonarRadius   > 0.0f) || (sonarJamRadius > 0.0f) ||
		                   (seismicRadius > 0.0f);
	}

	radarhandler->MoveUnit(this);
}


void CUnit::AddExperience(float exp)
{
	const float oldExp = experience;
	experience += exp;

	const float oldLimExp = limExperience;
	limExperience = experience / (experience + 1.0f);

	if (expGrade != 0.0f) {
		const int oldGrade = (int)(oldLimExp     / expGrade);
		const int newGrade = (int)(limExperience / expGrade);
		if (oldGrade != newGrade) {
			eventHandler.UnitExperience(this, oldExp);
		}
	}

	if (expPowerScale > 0.0f) {
		power = unitDef->power * (1.0f + (limExperience * expPowerScale));
	}
	if (expReloadScale > 0.0f) {
		reloadSpeed = (1.0f + (limExperience * expReloadScale));
	}
	if (expHealthScale > 0.0f) {
		const float oldMaxHealth = maxHealth;

		maxHealth = std::max(0.1f, unitDef->health * (1.0f + (limExperience * expHealthScale)));
		health = health * (maxHealth / oldMaxHealth);
	}
}


void CUnit::DoSeismicPing(float pingSize)
{
	float rx = gs->randFloat();
	float rz = gs->randFloat();

	const float* errorScale = &radarhandler->radarErrorSize[0];
	if (!(losStatus[gu->myAllyTeam] & LOS_INLOS) &&
	    radarhandler->InSeismicDistance(this, gu->myAllyTeam)) {
		const float3 err(errorScale[gu->myAllyTeam] * (0.5f - rx), 0.0f,
		                 errorScale[gu->myAllyTeam] * (0.5f - rz));

		new CSeismicGroundFlash(pos + err, 30, 15, 0, pingSize, 1, float3(0.8f, 0.0f, 0.0f));
	}
	for (int a = 0; a < teamHandler->ActiveAllyTeams(); ++a) {
		if (radarhandler->InSeismicDistance(this, a)) {
			const float3 err(errorScale[a] * (0.5f - rx), 0.0f,
			                 errorScale[a] * (0.5f - rz));
			const float3 pingPos = (pos + err);
			eventHandler.UnitSeismicPing(this, a, pingPos, pingSize);
			eoh->SeismicPing(a, *this, pingPos, pingSize);
		}
	}
}


void CUnit::ChangeLos(int losRad, int airRad)
{
	loshandler->FreeInstance(los);
	los = NULL;
	losRadius = losRad;
	airLosRadius = airRad;
	loshandler->MoveUnit(this, false);
}


bool CUnit::ChangeTeam(int newteam, ChangeType type)
{
	if (isDead) {
		return false;
	}

	// do not allow unit count violations due to team swapping
	// (this includes unit captures)
	if (unitHandler->unitsByDefs[newteam][unitDef->id].size() >= unitDef->maxThisUnit) {
		return false;
	}

	const bool capture = (type == ChangeCaptured);
	if (luaRules && !luaRules->AllowUnitTransfer(this, newteam, capture)) {
		return false;
	}

	// do not allow old player to keep controlling the unit
	if (fpsControlPlayer != NULL) {
		fpsControlPlayer->StopControllingUnit();
		assert(fpsControlPlayer == NULL);
	}

	const int oldteam = team;

	selectedUnits.RemoveUnit(this);
	SetGroup(NULL);

	eventHandler.UnitTaken(this, oldteam, newteam);
	eoh->UnitCaptured(*this, oldteam, newteam);

	quadField->RemoveUnit(this);
	quads.clear();
	loshandler->FreeInstance(los);
	los = 0;
	losStatus[allyteam] = 0;
	radarhandler->RemoveUnit(this);

	if (unitDef->isAirBase) {
		airBaseHandler->DeregisterAirBase(this);
	}

	if (type == ChangeGiven) {
		teamHandler->Team(oldteam)->RemoveUnit(this, CTeam::RemoveGiven);
		teamHandler->Team(newteam)->AddUnit(this,    CTeam::AddGiven);
	} else {
		teamHandler->Team(oldteam)->RemoveUnit(this, CTeam::RemoveCaptured);
		teamHandler->Team(newteam)->AddUnit(this,    CTeam::AddCaptured);
	}

	if (!beingBuilt) {
		teamHandler->Team(oldteam)->metalStorage  -= metalStorage;
		teamHandler->Team(oldteam)->energyStorage -= energyStorage;

		teamHandler->Team(newteam)->metalStorage  += metalStorage;
		teamHandler->Team(newteam)->energyStorage += energyStorage;
	}


	team = newteam;
	allyteam = teamHandler->AllyTeam(newteam);
	neutral = false;

	unitHandler->unitsByDefs[oldteam][unitDef->id].erase(this);
	unitHandler->unitsByDefs[newteam][unitDef->id].insert(this);

	for (int at = 0; at < teamHandler->ActiveAllyTeams(); ++at) {
		UpdateLosStatus(at);
	}

	loshandler->MoveUnit(this, false);
	quadField->MovedUnit(this);
	radarhandler->MoveUnit(this);

	if (unitDef->isAirBase) {
		airBaseHandler->RegisterAirBase(this);
	}

	eventHandler.UnitGiven(this, oldteam, newteam);
	eoh->UnitGiven(*this, oldteam, newteam);

	// reset states and clear the queues
	if (!teamHandler->AlliedTeams(oldteam, newteam))
		ChangeTeamReset();

	return true;
}


void CUnit::ChangeTeamReset()
{
	// stop friendly units shooting at us
	const CObject::TDependenceMap& listeners = GetAllListeners();
	std::vector<CUnit *> alliedunits;
	for (CObject::TDependenceMap::const_iterator li = listeners.begin(); li != listeners.end(); ++li) {
		for (CObject::TSyncSafeSet::const_iterator di = li->second.begin(); di != li->second.end(); ++di) {
			CUnit* u = dynamic_cast<CUnit*>(*di);
			if (u != NULL && teamHandler->AlliedTeams(team, u->team))
				alliedunits.push_back(u);
		}
	}
	for (std::vector<CUnit*>::const_iterator ui = alliedunits.begin(); ui != alliedunits.end(); ++ui) {
		(*ui)->StopAttackingAllyTeam(allyteam);
	}
	// and stop shooting at friendly ally teams
	for (int t = 0; t < teamHandler->ActiveAllyTeams(); ++t) {
		if (teamHandler->Ally(t, allyteam))
			StopAttackingAllyTeam(t);
	}

	// clear the commands (newUnitCommands for factories)
	Command c(CMD_STOP);
	commandAI->GiveCommand(c);

	// clear the build commands for factories
	CFactoryCAI* facAI = dynamic_cast<CFactoryCAI*>(commandAI);
	if (facAI) {
		const unsigned char options = RIGHT_MOUSE_KEY; // clear option
		CCommandQueue& buildCommands = facAI->commandQue;
		CCommandQueue::iterator it;
		std::vector<Command> clearCommands;
		clearCommands.reserve(buildCommands.size());
		for (it = buildCommands.begin(); it != buildCommands.end(); ++it) {
			clearCommands.push_back(Command(it->GetID(), options));
		}
		for (int i = 0; i < (int)clearCommands.size(); i++) {
			facAI->GiveCommand(clearCommands[i]);
		}
	}

	//FIXME reset to unitdef defaults

	// deactivate to prevent the old give metal maker trick
	// TODO remove, because it is *A specific
	c = Command(CMD_ONOFF, 0, 0); // always off
	commandAI->GiveCommand(c);

	// reset repeat state
	c = Command(CMD_REPEAT, 0, 0);
	commandAI->GiveCommand(c);

	// reset cloak state
	if (unitDef->canCloak) {
		c = Command(CMD_CLOAK, 0, 0); // always off
		commandAI->GiveCommand(c);
	}
	// reset move state
	if (unitDef->canmove || unitDef->builder) {
		c = Command(CMD_MOVE_STATE, 0, MOVESTATE_MANEUVER);
		commandAI->GiveCommand(c);
	}
	// reset fire state
	if (commandAI->CanChangeFireState()) {
		c = Command(CMD_FIRE_STATE, 0, FIRESTATE_FIREATWILL);
		commandAI->GiveCommand(c);
	}
	// reset trajectory state
	if (unitDef->highTrajectoryType > 1) {
		c = Command(CMD_TRAJECTORY, 0, 0);
		commandAI->GiveCommand(c);
	}
}



bool CUnit::AttackUnit(CUnit* targetUnit, bool isUserTarget, bool wantManualFire, bool fpsMode)
{
	bool ret = false;

	haveManualFireRequest = wantManualFire;
	userAttackGround = false;

	if (attackTarget != NULL) {
		DeleteDeathDependence(attackTarget, DEPENDENCE_TARGET);
	}

	attackPos = ZeroVector;
	attackTarget = targetUnit;

	if (targetUnit != NULL) {
		AddDeathDependence(targetUnit, DEPENDENCE_TARGET);
	}

	for (std::vector<CWeapon*>::iterator wi = weapons.begin(); wi != weapons.end(); ++wi) {
		CWeapon* w = *wi;

		// isUserTarget is true if this target was selected by the
		// user as opposed to automatically by the unit's commandAI
		//
		// NOTE: "&&" because we have a separate userAttackGround (!)
		w->targetType = Target_None;
		w->haveUserTarget = (targetUnit != NULL && isUserTarget);

		if (targetUnit == NULL)
			continue;

		if ((wantManualFire == (unitDef->canManualFire && w->weaponDef->manualfire)) || fpsMode) {
			ret |= (w->AttackUnit(targetUnit, isUserTarget));
		}
	}

	return ret;
}

bool CUnit::AttackGround(const float3& pos, bool isUserTarget, bool wantManualFire, bool fpsMode)
{
	bool ret = false;

	// remember whether this was a user-order for SlowUpdateWeapons
	// (because CCommandAI does not keep calling us, but ::SUW does)
	haveManualFireRequest = wantManualFire;
	userAttackGround = isUserTarget;

	if (attackTarget != NULL) {
		DeleteDeathDependence(attackTarget, DEPENDENCE_TARGET);
	}

	attackPos = pos;
	attackTarget = NULL;

	for (std::vector<CWeapon*>::iterator wi = weapons.begin(); wi != weapons.end(); ++wi) {
		CWeapon* w = *wi;

		w->targetType = Target_None;
		w->haveUserTarget = false; // this should be false for ground-attack commands

		if ((wantManualFire == (unitDef->canManualFire && w->weaponDef->manualfire)) || fpsMode) {
			ret |= (w->AttackGround(pos, isUserTarget));
		}
	}

	return ret;
}



void CUnit::SetLastAttacker(CUnit* attacker)
{
	assert(attacker != NULL);

	if (teamHandler->AlliedTeams(team, attacker->team)) {
		return;
	}
	if (lastAttacker) {
		DeleteDeathDependence(lastAttacker, DEPENDENCE_ATTACKER);
	}

	lastAttackFrame = gs->frameNum;
	lastAttacker = attacker;

	AddDeathDependence(attacker, DEPENDENCE_ATTACKER);
}

void CUnit::DependentDied(CObject* o)
{
	if (o == attackTarget) { attackTarget = NULL; }
	if (o == soloBuilder)  { soloBuilder  = NULL; }
	if (o == transporter)  { transporter  = NULL; }
	if (o == lastAttacker) { lastAttacker = NULL; }

	incomingMissiles.remove(static_cast<CMissileProjectile*>(o));

	CSolidObject::DependentDied(o);
}



void CUnit::UpdateTerrainType()
{
	if (curTerrainType != lastTerrainType) {
		script->SetSFXOccupy(curTerrainType);
		lastTerrainType = curTerrainType;
	}
}

void CUnit::CalculateTerrainType()
{
	enum {
		SFX_TERRAINTYPE_NONE    = 0,
		SFX_TERRAINTYPE_WATER_A = 1,
		SFX_TERRAINTYPE_WATER_B = 2,
		SFX_TERRAINTYPE_LAND    = 4,
	};

	// optimization: there's only about one unit that actually needs this information
	// ==> why are we even bothering with it? the callin parameter barely makes sense
	if (!script->HasSetSFXOccupy())
		return;

	if (GetTransporter() != NULL) {
		curTerrainType = SFX_TERRAINTYPE_NONE;
		return;
	}

	const float height = ground->GetApproximateHeight(pos.x, pos.z);

	// water
	if (height < -5.0f) {
		if (upright)
			curTerrainType = SFX_TERRAINTYPE_WATER_B;
		else
			curTerrainType = SFX_TERRAINTYPE_WATER_A;
	}
	// shore
	else if (height < 0.0f) {
		if (upright)
			curTerrainType = SFX_TERRAINTYPE_WATER_A;
	}
	// land (or air)
	else {
		curTerrainType = SFX_TERRAINTYPE_LAND;
	}
}


bool CUnit::SetGroup(CGroup* newGroup, bool fromFactory)
{
	GML_RECMUTEX_LOCK(grpsel); // SetGroup

	// factory is not necessarily selected
	if (fromFactory && !selectedUnits.AutoAddBuiltUnitsToFactoryGroup())
		return false;

	if (group != NULL) {
		group->RemoveUnit(this);
	}

	group = newGroup;

	if (group) {
		if (!group->AddUnit(this)){
			// group did not accept us
			group = NULL;
			return false;
		} else {
			// add unit to the set of selected units iff its new group is already selected
			// and (user wants the unit to be auto-selected or the unit is not newly built)
			if (selectedUnits.IsGroupSelected(group->id) && (selectedUnits.AutoAddBuiltUnitsToSelectedGroup() || !fromFactory)) {
				selectedUnits.AddUnit(this);
			}
		}
	}

	return true;
}


#if defined(USE_GML) && defined(__GNUC__) && (__GNUC__ == 4)
// This is supposed to fix some GCC crashbug related to threading
// The MOVAPS SSE instruction is otherwise getting misaligned data
__attribute__ ((force_align_arg_pointer))
#endif
bool CUnit::AddBuildPower(float amount, CUnit* builder)
{
	// stop decaying on building AND reclaim
	lastNanoAdd = gs->frameNum;

	if (amount >= 0.0f) { //  build / repair
		if (!beingBuilt && (health >= maxHealth)) {
			return false;
		}

		if (beingBuilt) { //build
			const float part = std::min(amount / buildTime, 1.0f - buildProgress);
			const float metalCostPart  = metalCost  * part;
			const float energyCostPart = energyCost * part;

			if ((teamHandler->Team(builder->team)->metal  >= metalCostPart) &&
			    (teamHandler->Team(builder->team)->energy >= energyCostPart) &&
			    (!luaRules || luaRules->AllowUnitBuildStep(builder, this, part))) {

				if (builder->UseMetal(metalCostPart)) {
					// just because we checked doesn't mean they were deducted since upkeep can prevent deduction
					// FIXME luaRules->AllowUnitBuildStep() may have changed the storages!!! so the checks can be invalid!
					// TODO add a builder->UseResources(SResources(metalCostPart, energyCostPart))
					if (builder->UseEnergy(energyCostPart)) {
						health += (maxHealth * part);
						health = std::min(health, maxHealth);
						buildProgress += part;

						if (buildProgress >= 1.0f) {
							FinishedBuilding(false);
						}
					} else {
						// refund the metal if the energy cannot be deducted
						builder->UseMetal(-metalCostPart);
					}
				}
				return true;
			} else {
				// update the energy and metal required counts
				teamHandler->Team(builder->team)->metalPull  += metalCostPart;
				teamHandler->Team(builder->team)->energyPull += energyCostPart;
			}
			return false;
		}
		else if (health < maxHealth) { //repair
			const float part = std::min(amount / buildTime, 1.0f - (health / maxHealth));
			const float energyUse = (energyCost * part);
			const float energyUseScaled = energyUse * modInfo.repairEnergyCostFactor;

	  		if (!builder->UseEnergy(energyUseScaled)) {
				teamHandler->Team(builder->team)->energyPull += energyUseScaled;
				return false;
			}

			repairAmount += amount;
			health += (maxHealth * part);
			health = std::min(health, maxHealth);

			return true;
		}
	} else { // reclaim
		if (isDead || crashing) {
			return false;
		}

		const float part = std::max(amount / buildTime, -buildProgress);
		const float energyRefundPart = energyCost * part;
		const float metalRefundPart = metalCost * part;
		const float metalRefundPartScaled = metalRefundPart * modInfo.reclaimUnitEfficiency;
		const float energyRefundPartScaled = energyRefundPart * modInfo.reclaimUnitEnergyCostFactor;

		if (luaRules && !luaRules->AllowUnitBuildStep(builder, this, part)) {
			return false;
		}

		restTime = 0;

		if (!AllowedReclaim(builder)) {
			builder->DependentDied(this);
			return false;
		}

		if (!builder->UseEnergy(-energyRefundPartScaled)) {
			teamHandler->Team(builder->team)->energyPull += energyRefundPartScaled;
			return false;
		}

		health += (maxHealth * part);
		buildProgress += (part * int(beingBuilt) * int(modInfo.reclaimUnitMethod == 0));

		if (modInfo.reclaimUnitMethod == 0) {
			// gradual reclamation of invested metal
			builder->AddMetal(-metalRefundPartScaled, false);
			// turn reclaimee into nanoframe (even living units)
			beingBuilt = true;
		} else {
			// lump reclamation of invested metal
			// NOTE:
			//   because the nanoframe is also decaying on its OWN
			//   (which reduces buildProgress and returns resources)
			//   *while* we are reclaiming it, the final lump has to
			//   subtract the amount already returned through decay
			//
			//   this also means that in lump-reclaim mode only health
			//   is reduced since we need buildProgress to calculate a
			//   proper refund
			if (buildProgress <= 0.0f || health <= 0.0f) {
				builder->AddMetal((metalCost * buildProgress) * modInfo.reclaimUnitEfficiency, false);
			}
		}

		if (beingBuilt) {
			if (buildProgress <= 0.0f || health <= 0.0f) {
				KillUnit(NULL, false, true);
				return false;
			}
		} else {
			if (health <= 0.0f) {
				KillUnit(NULL, false, true);
				return false;
			}
		}
		return true;
	}
	return false;
}


void CUnit::FinishedBuilding(bool postInit)
{
	if (!beingBuilt && !postInit) {
		return;
	}

	beingBuilt = false;
	buildProgress = 1.0f;
	mass = unitDef->mass;

	if (soloBuilder) {
		DeleteDeathDependence(soloBuilder, DEPENDENCE_BUILDER);
		soloBuilder = NULL;
	}

	ChangeLos(realLosRadius, realAirLosRadius);

	if (unitDef->activateWhenBuilt) {
		Activate();
	}
	SetMetalStorage(unitDef->metalStorage);
	SetEnergyStorage(unitDef->energyStorage);


	// Sets the frontdir in sync with heading.
	frontdir = GetVectorFromHeading(heading) + float3(0, frontdir.y, 0);

	if (unitDef->isAirBase) {
		airBaseHandler->RegisterAirBase(this);
	}

	eventHandler.UnitFinished(this);
	eoh->UnitFinished(*this);

	if (unitDef->startCloaked) {
		wantCloak = true;
	}

	if (unitDef->isFeature && CUnit::spawnFeature) {
		FeatureLoadParams p = {featureHandler->GetFeatureDefByID(featureDefID), NULL, pos, ZeroVector, -1, team, allyteam, heading, buildFacing, 0};
		CFeature* f = featureHandler->CreateWreckage(p, 0, false);

		if (f) {
			f->blockHeightChanges = true;
		}

		UnBlock();
		KillUnit(NULL, false, true);
	}
}


void CUnit::KillUnit(CUnit* attacker, bool selfDestruct, bool reclaimed, bool showDeathSequence)
{
	if (isDead) { return; }
	if (crashing && !beingBuilt) { return; }

	isDead = true;
	deathSpeed = speed;

	eventHandler.UnitDestroyed(this, attacker);
	eoh->UnitDestroyed(*this, attacker);
	// Will be called in the destructor again, but this can not hurt
	this->SetGroup(NULL);

	blockHeightChanges = false;

	if (unitDef->windGenerator > 0.0f) {
		wind.DelUnit(this);
	}

	if (showDeathSequence && (!reclaimed && !beingBuilt)) {
		const WeaponDef* wd = (selfDestruct)? unitDef->selfdExpWeaponDef: unitDef->deathExpWeaponDef;

		if (wd != NULL) {
			CGameHelper::ExplosionParams params = {
				pos,
				ZeroVector,
				wd->damages,
				wd,
				this,                              // owner
				NULL,                              // hitUnit
				NULL,                              // hitFeature
				wd->craterAreaOfEffect,
				wd->damageAreaOfEffect,
				wd->edgeEffectiveness,
				wd->explosionSpeed,
				wd->damages[0] > 500? 1.0f: 2.0f,  // gfxMod
				false,                             // impactOnly
				false,                             // ignoreOwner
				true,                              // damageGround
				-1u                                // projectileID
			};

			helper->Explosion(params);
		}

		if (selfDestruct) {
			recentDamage += (maxHealth * 2.0f);
		}

		// start running the unit's kill-script
		script->Killed();
	} else {
		deathScriptFinished = true;
	}

	if (!deathScriptFinished) {
		// put the unit in a pseudo-zombie state until Killed finishes
		speed = ZeroVector;
		SetStunned(true);
		paralyzeDamage = 100.0f * maxHealth;
		health = std::max(health, 0.0f);
	}
}

bool CUnit::AllowedReclaim(CUnit* builder) const
{
	// Don't allow the reclaim if the unit is finished and we arent allowed to reclaim it
	if (!beingBuilt) {
		if (allyteam == builder->allyteam) {
			if ((team != builder->team) && (!modInfo.reclaimAllowAllies)) return false;
		} else {
			if (!modInfo.reclaimAllowEnemies) return false;
		}
	}

	return true;
}

bool CUnit::UseMetal(float metal)
{
	if (metal < 0) {
		AddMetal(-metal);
		return true;
	}
	teamHandler->Team(team)->metalPull += metal;
	bool canUse = teamHandler->Team(team)->UseMetal(metal);
	if (canUse)
		metalUseI += metal;
	return canUse;
}


void CUnit::AddMetal(float metal, bool useIncomeMultiplier)
{
	if (metal < 0) {
		UseMetal(-metal);
		return;
	}
	metalMakeI += metal;
	teamHandler->Team(team)->AddMetal(metal, useIncomeMultiplier);
}


bool CUnit::UseEnergy(float energy)
{
	if (energy < 0) {
		AddEnergy(-energy);
		return true;
	}
	teamHandler->Team(team)->energyPull += energy;
	bool canUse = teamHandler->Team(team)->UseEnergy(energy);
	if (canUse)
		energyUseI += energy;
	return canUse;
}


void CUnit::AddEnergy(float energy, bool useIncomeMultiplier)
{
	if (energy < 0) {
		UseEnergy(-energy);
		return;
	}
	energyMakeI += energy;
	teamHandler->Team(team)->AddEnergy(energy, useIncomeMultiplier);
}



void CUnit::Activate()
{
	if (activated)
		return;

	activated = true;
	script->Activate();

	if (unitDef->targfac) {
		radarhandler->radarErrorSize[allyteam] /= radarhandler->targFacEffect;
	}

	radarhandler->MoveUnit(this);

	#if (PLAY_SOUNDS == 1)
	if (losStatus[gu->myAllyTeam] & LOS_INLOS) {
		Channels::General.PlayRandomSample(unitDef->sounds.activate, this);
	}
	#endif
}

void CUnit::Deactivate()
{
	if (!activated)
		return;

	activated = false;
	script->Deactivate();

	if (unitDef->targfac) {
		radarhandler->radarErrorSize[allyteam] *= radarhandler->targFacEffect;
	}

	radarhandler->RemoveUnit(this);

	#if (PLAY_SOUNDS == 1)
	if (losStatus[gu->myAllyTeam] & LOS_INLOS) {
		Channels::General.PlayRandomSample(unitDef->sounds.deactivate, this);
	}
	#endif
}



void CUnit::UpdateWind(float x, float z, float strength)
{
	const float windHeading = ClampRad(GetHeadingFromVectorF(-x, -z) - heading * TAANG2RAD);
	const float windStrength = std::min(strength, unitDef->windGenerator);

	script->WindChanged(windHeading, windStrength);
}


void CUnit::IncomingMissile(CMissileProjectile* missile)
{
	if (unitDef->canDropFlare) {
		incomingMissiles.push_back(missile);
		AddDeathDependence(missile, DEPENDENCE_INCOMING);

		if (lastFlareDrop < (gs->frameNum - unitDef->flareReloadTime * 30)) {
			new CFlareProjectile(pos, speed, this, (int) (gs->frameNum + unitDef->flareDelay * (1 + gs->randFloat()) * 15));
			lastFlareDrop = gs->frameNum;
		}
	}
}



void CUnit::TempHoldFire()
{
	dontFire = true;
	AttackUnit(NULL, false, false);
}

void CUnit::ReleaseTempHoldFire()
{
	dontFire = false;
}



void CUnit::PostLoad()
{
	//HACK:Initializing after load
	unitDef = unitDefHandler->GetUnitDefByID(unitDefID); // strange. creg should handle this by itself already, but it doesn't
	objectDef = unitDef;
	model = unitDef->LoadModel();
	localModel = new LocalModel(model);
	modelParser->CreateLocalModel(localModel);
	blockMap = (unitDef->GetYardMap().empty())? NULL: &unitDef->GetYardMap()[0];

	SetMidAndAimPos(model->relMidPos, model->relMidPos, true);
	SetRadiusAndHeight(model);
	UpdateDirVectors(!upright);
	UpdateMidAndAimPos();

	// FIXME: how to handle other script types (e.g. Lua) here?
	script = CUnitScriptFactory::CreateScript("scripts/" + unitDef->scriptName, this);

	// Call initializing script functions
	script->Create();
	script->SetSFXOccupy(curTerrainType);

	if (unitDef->windGenerator > 0.0f) {
		wind.AddUnit(this);
	}

	if (activated) {
		script->Activate();
	}

	(eventBatchHandler->GetUnitCreatedDestroyedBatch()).enqueue(EventBatchHandler::UD(this, isCloaked));

}


void CUnit::StopAttackingAllyTeam(int ally)
{
	if (lastAttacker != NULL && lastAttacker->allyteam == ally) {
		DeleteDeathDependence(lastAttacker, DEPENDENCE_ATTACKER);
		lastAttacker = NULL;
	}
	if (attackTarget != NULL && attackTarget->allyteam == ally)
		AttackUnit(NULL, false, false);

	commandAI->StopAttackingAllyTeam(ally);
	for (std::vector<CWeapon*>::iterator it = weapons.begin(); it != weapons.end(); ++it) {
		(*it)->StopAttackingAllyTeam(ally);
	}
}


bool CUnit::GetNewCloakState(bool stunCheck) {
	if (stunCheck) {
		if (IsStunned() && isCloaked && scriptCloak <= 3) {
			return false;
		}

		return isCloaked;
	}

	if (scriptCloak >= 3) {
		return true;
	}

	if (wantCloak || (scriptCloak >= 1)) {
		const CUnit* closestEnemy = CGameHelper::GetClosestEnemyUnitNoLosTest(NULL, midPos, decloakDistance, allyteam, unitDef->decloakSpherical, false);
		const float cloakCost = (speed.SqLength() > 0.2f)? unitDef->cloakCostMoving: unitDef->cloakCost;

		if (decloakDistance > 0.0f && closestEnemy != NULL) {
			curCloakTimeout = gs->frameNum + cloakTimeout;
			return false;
		}

		if (isCloaked || (gs->frameNum >= curCloakTimeout)) {
			return ((scriptCloak >= 2) || UseEnergy(cloakCost * 0.5f));
		}
	}

	return false;
}
void CUnit::SlowUpdateCloak(bool stunCheck)
{
	const bool oldCloak = isCloaked;
	const bool newCloak = GetNewCloakState(stunCheck);

	if (oldCloak != newCloak) {
		if (newCloak) {
			eventHandler.UnitCloaked(this);
		} else {
			eventHandler.UnitDecloaked(this);
		}
	}

	isCloaked = newCloak;
}

void CUnit::ScriptDecloak(bool updateCloakTimeOut)
{
	if (scriptCloak <= 2) {
		if (isCloaked) {
			isCloaked = false;
			eventHandler.UnitDecloaked(this);
		}

		if (updateCloakTimeOut) {
			curCloakTimeout = gs->frameNum + cloakTimeout;
		}
	}
}

#ifdef USE_GML
	#define LOD_MUTEX CR_MEMBER_UN(lodmutex),
#else
	#define LOD_MUTEX
#endif

CR_BIND_DERIVED(CUnit, CSolidObject, );
CR_REG_METADATA(CUnit, (
	CR_MEMBER(unitDef),
	CR_MEMBER(unitDefID),
	CR_MEMBER(featureDefID),

	CR_MEMBER(modParams),
	CR_MEMBER(modParamsMap),

	CR_MEMBER(upright),

	CR_MEMBER(deathSpeed),

	CR_MEMBER(travel),
	CR_MEMBER(travelPeriod),

	CR_MEMBER(power),

	CR_MEMBER(maxHealth),
	CR_MEMBER(paralyzeDamage),
	CR_MEMBER(captureProgress),
	CR_MEMBER(experience),
	CR_MEMBER(limExperience),

	CR_MEMBER(neutral),

	CR_MEMBER(soloBuilder),
	CR_MEMBER(beingBuilt),
	CR_MEMBER(lastNanoAdd),
	CR_MEMBER(repairAmount),
	CR_MEMBER(transporter),
	CR_MEMBER(loadingTransportId),
	CR_MEMBER(buildProgress),
	CR_MEMBER(groundLevelled),
	CR_MEMBER(terraformLeft),
	CR_MEMBER(realLosRadius),
	CR_MEMBER(realAirLosRadius),

	CR_MEMBER(losStatus),

	CR_MEMBER(inBuildStance),
	CR_MEMBER(useHighTrajectory),

	CR_MEMBER(dontUseWeapons),
	CR_MEMBER(dontFire),

	CR_MEMBER(deathScriptFinished),
	CR_MEMBER(delayedWreckLevel),

	CR_MEMBER(restTime),
	CR_MEMBER(outOfMapTime),

	CR_MEMBER(weapons),
	CR_MEMBER(shieldWeapon),
	CR_MEMBER(stockpileWeapon),
	CR_MEMBER(reloadSpeed),
	CR_MEMBER(maxRange),

	CR_MEMBER(haveTarget),
	CR_MEMBER(haveManualFireRequest),

	CR_MEMBER(lastMuzzleFlameSize),
	CR_MEMBER(lastMuzzleFlameDir),

	CR_MEMBER(armorType),
	CR_MEMBER(category),

	CR_MEMBER(quads),
	CR_MEMBER(los),

	CR_MEMBER(tempNum),

	CR_MEMBER(mapSquare),

	CR_MEMBER(losRadius),
	CR_MEMBER(airLosRadius),
	CR_MEMBER(lastLosUpdate),

	CR_MEMBER(losHeight),
	CR_MEMBER(radarHeight),

	CR_MEMBER(radarRadius),
	CR_MEMBER(sonarRadius),
	CR_MEMBER(jammerRadius),
	CR_MEMBER(sonarJamRadius),
	CR_MEMBER(seismicRadius),
	CR_MEMBER(seismicSignature),
	CR_MEMBER(hasRadarCapacity),
	CR_MEMBER(radarSquares),
	CR_MEMBER(oldRadarPos),
	CR_MEMBER(hasRadarPos),
	CR_MEMBER(stealth),
	CR_MEMBER(sonarStealth),

	CR_MEMBER(moveType),
	CR_MEMBER(prevMoveType),
	CR_MEMBER(usingScriptMoveType),

	// CR_MEMBER(fpsControlPlayer),
	CR_MEMBER(commandAI),
	CR_MEMBER(group),


	//CR_MEMBER(localModel), //
	// CR_MEMBER(script),

	CR_MEMBER(condUseMetal),
	CR_MEMBER(condUseEnergy),
	CR_MEMBER(condMakeMetal),
	CR_MEMBER(condMakeEnergy),
	CR_MEMBER(uncondUseMetal),
	CR_MEMBER(uncondUseEnergy),
	CR_MEMBER(uncondMakeMetal),
	CR_MEMBER(uncondMakeEnergy),

	CR_MEMBER(metalUse),
	CR_MEMBER(energyUse),
	CR_MEMBER(metalMake),
	CR_MEMBER(energyMake),

	CR_MEMBER(metalUseI),
	CR_MEMBER(energyUseI),
	CR_MEMBER(metalMakeI),
	CR_MEMBER(energyMakeI),
	CR_MEMBER(metalUseold),
	CR_MEMBER(energyUseold),
	CR_MEMBER(metalMakeold),
	CR_MEMBER(energyMakeold),
	CR_MEMBER(energyTickMake),

	CR_MEMBER(metalExtract),

	CR_MEMBER(metalCost),
	CR_MEMBER(energyCost),
	CR_MEMBER(buildTime),

	CR_MEMBER(metalStorage),
	CR_MEMBER(energyStorage),

	CR_MEMBER(lastAttacker),
	CR_MEMBER(lastAttackedPiece),
	CR_MEMBER(lastAttackedPieceFrame),
	CR_MEMBER(lastAttackFrame),
	CR_MEMBER(lastFireWeapon),
	CR_MEMBER(recentDamage),

	CR_MEMBER(attackTarget),
	CR_MEMBER(attackPos),

	CR_MEMBER(userAttackGround),

	CR_MEMBER(fireState),
	CR_MEMBER(moveState),

	CR_MEMBER(activated),

	CR_MEMBER(crashing),
	CR_MEMBER(isDead),
	CR_MEMBER(falling),
	CR_MEMBER(fallSpeed),

	CR_MEMBER(inAir),
	CR_MEMBER(inWater),

	CR_MEMBER(flankingBonusMode),
	CR_MEMBER(flankingBonusDir),
	CR_MEMBER(flankingBonusMobility),
	CR_MEMBER(flankingBonusMobilityAdd),
	CR_MEMBER(flankingBonusAvgDamage),
	CR_MEMBER(flankingBonusDifDamage),

	CR_MEMBER(armoredState),
	CR_MEMBER(armoredMultiple),
	CR_MEMBER(curArmorMultiple),

	CR_MEMBER(posErrorVector),
	CR_MEMBER(posErrorDelta),
	CR_MEMBER(nextPosErrorUpdate),

	CR_MEMBER(wantCloak),
	CR_MEMBER(scriptCloak),
	CR_MEMBER(cloakTimeout),
	CR_MEMBER(curCloakTimeout),
	CR_MEMBER(isCloaked),
	CR_MEMBER(decloakDistance),

	CR_MEMBER(lastTerrainType),
	CR_MEMBER(curTerrainType),

	CR_MEMBER(selfDCountdown),

	CR_IGNORED(myTrack),
	CR_IGNORED(myIcon),

	CR_MEMBER(incomingMissiles),
	CR_MEMBER(lastFlareDrop),

	CR_MEMBER(currentFuel),

	CR_MEMBER(alphaThreshold),
	CR_MEMBER(cegDamage),

	CR_MEMBER_UN(noDraw),
	CR_MEMBER_UN(noMinimap),
	CR_MEMBER_UN(leaveTracks),

	CR_MEMBER_UN(isSelected),
	CR_MEMBER_UN(isIcon),
	CR_MEMBER(iconRadius),

	CR_MEMBER_UN(lodCount),
	CR_MEMBER_UN(currentLOD),
	CR_MEMBER_UN(lastDrawFrame),
	CR_MEMBER(lastUnitUpdate),

	LOD_MUTEX

	CR_MEMBER_UN(tooltip),

	CR_MEMBER(stunned),

//	CR_MEMBER(expMultiplier),
//	CR_MEMBER(expPowerScale),
//	CR_MEMBER(expHealthScale),
//	CR_MEMBER(expReloadScale),
//	CR_MEMBER(expGrade),

//	CR_MEMBER(empDecline),
//	CR_MEMBER(spawnFeature),

//	CR_MEMBER(model),

	CR_POSTLOAD(PostLoad)
));

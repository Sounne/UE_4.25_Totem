// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_PHYSX

#include "Modules/ModuleManager.h"
#include "PhysXCooking.h"

/**
 * FRuntimePhysXCooking. Cooks physics data at runtime
**/
class FRuntimePhysXCooking : public FPhysXCooking
{
	//This is just a wrapper for the engine module, but we are able to use it dynamically via plugin
};


/**
 * Module for PhysX cooking at runtime
 */

class FRuntimePhysXPlatformModule : public FPhysXPlatformModule
{
	//This is just a wrapper for the engine module, but we are able to use it dynamically via plugin
	
};

IMPLEMENT_MODULE(FRuntimePhysXPlatformModule, RuntimePhysXCooking );

#else

class FRuntimePhysXPlatformModule : public IModuleInterface
{

};

IMPLEMENT_MODULE(FRuntimePhysXPlatformModule, RuntimePhysXCooking);

#endif

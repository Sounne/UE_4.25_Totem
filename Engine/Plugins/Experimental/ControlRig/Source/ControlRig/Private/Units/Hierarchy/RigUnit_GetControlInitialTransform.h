// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Units/RigUnit.h"
#include "RigUnit_GetControlInitialTransform.generated.h"

/**
 * GetControlTransform is used to retrieve a single transform from a hierarchy.
 */
USTRUCT(meta=(DisplayName="Get Control Initial Transform", Category="Controls", DocumentationPolicy = "Strict", Keywords="GetControlInitialTransform"))
struct FRigUnit_GetControlInitialTransform : public FRigUnit
{
	GENERATED_BODY()

	FRigUnit_GetControlInitialTransform()
		: Space(EBoneGetterSetterMode::LocalSpace)
		, CachedControlIndex(INDEX_NONE)
	{}

	virtual FString GetUnitLabel() const override;

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	/**
	 * The name of the Control to retrieve the transform for.
	 */
	UPROPERTY(meta = (Input, CustomWidget = "ControlName", Constant))
	FName Control;

	/**
	 * Defines if the Control's transform should be retrieved
	 * in local or global space.
	 */ 
	UPROPERTY(meta = (Input))
	EBoneGetterSetterMode Space;

	// The current transform of the given bone - or identity in case it wasn't found.
	UPROPERTY(meta=(Output))
	FTransform Transform;

	// Used to cache the internally used bone index
	UPROPERTY()
	int32 CachedControlIndex;
};

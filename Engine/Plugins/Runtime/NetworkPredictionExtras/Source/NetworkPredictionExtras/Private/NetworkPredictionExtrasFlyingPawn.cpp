// Copyright Epic Games, Inc. All Rights Reserved.

#include "NetworkPredictionExtrasFlyingPawn.h"
#include "Components/InputComponent.h"
#include "Movement/FlyingMovement.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "GameFramework/SpringArmComponent.h"
#include "DrawDebugHelpers.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"

#include "Misc/AssertionMacros.h"
#include "HAL/PlatformStackWalk.h"
#include "HAL/ThreadHeartBeat.h"
#include "MockAbilitySimulation.h"

namespace FlyingPawnCVars
{

int32 CameraStyle = 0;
FAutoConsoleVariableRef CVarPenetrationPullbackDistance(TEXT("NetworkPredictionExtras.FlyingPawn.CameraSyle"),
	CameraStyle,
	TEXT("Sets camera mode style in ANetworkPredictionExtrasFlyingPawn \n")
	TEXT("0=camera fixed behind pawn. 1-3 are variations of a free camera system (Gamepad recommended)."),
	ECVF_Default);

static int32 BindAutomatically = 1;
static FAutoConsoleVariableRef CVarBindAutomatically(TEXT("NetworkPredictionExtras.FlyingPawn.BindAutomatically"),
	BindAutomatically, TEXT("Binds local input and mispredict commands to 5 and 6 respectively"), ECVF_Default);
}

const FName Name_FlyingMovementComponent(TEXT("FlyingMovementComponent"));

ANetworkPredictionExtrasFlyingPawn::ANetworkPredictionExtrasFlyingPawn(const FObjectInitializer& ObjectInitializer)
{
	FlyingMovementComponent = CreateDefaultSubobject<UFlyingMovementComponent>(Name_FlyingMovementComponent);
	ensure(FlyingMovementComponent);
}

void ANetworkPredictionExtrasFlyingPawn::BeginPlay()
{
	Super::BeginPlay();

	if (UWorld* World = GetWorld())
	{
		if (ensure(FlyingMovementComponent))
		{
			FlyingMovementComponent->ProduceInputDelegate.BindUObject(this, &ANetworkPredictionExtrasFlyingPawn::ProduceInput);
		}

		// Binds 0 and 9 to the debug hud commands. This is just a convenience for the extras plugin. Real projects should bind this themselves
		if (FlyingPawnCVars::BindAutomatically > 0)
		{
			if (ULocalPlayer* LocalPlayer = World->GetFirstLocalPlayerFromController())
			{
				LocalPlayer->Exec(World, TEXT("setbind Nine nms.Debug.LocallyControlledPawn"), *GLog);
				LocalPlayer->Exec(World, TEXT("setbind Zero nms.Debug.ToggleContinous"), *GLog);
			}
		}
	}
}

UNetConnection* ANetworkPredictionExtrasFlyingPawn::GetNetConnection() const
{
	UNetConnection* SuperNetConnection = Super::GetNetConnection();
	if (SuperNetConnection)
	{
		return SuperNetConnection;
	}

	if (bFakeAutonomousProxy)
	{
		if (GetLocalRole() == ROLE_Authority)
		{
			for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
			{
				APlayerController* PC = Iterator->Get();
				if (PC->GetNetConnection() && PC->GetPawn())
				{
					return PC->GetNetConnection();
				}
			}
		}
		if (GetLocalRole() == ROLE_AutonomousProxy && GetWorld()->GetFirstPlayerController() && GetWorld()->GetFirstPlayerController()->GetPawn())
		{
			return GetWorld()->GetFirstPlayerController()->GetNetConnection();
		}
	}

	return nullptr;
}

void ANetworkPredictionExtrasFlyingPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Setup some bindings.
	//
	// This also is not necessary. This would usually be defined in DefaultInput.ini.
	// But the nature of NetworkPredictionExtras is to not assume or require anything of the project its being run in.
	// So in order to make this nice and self contained, manually do input bindings here.
	if (APlayerController* PC = Cast<APlayerController>(Controller))
	{
		if (PC->PlayerInput)
		{
			//Gamepad
			PC->PlayerInput->AddAxisMapping(FInputAxisKeyMapping(TEXT("MoveRight"), EKeys::Gamepad_LeftX));
			PC->PlayerInput->AddAxisMapping(FInputAxisKeyMapping(TEXT("MoveForward"), EKeys::Gamepad_LeftY));
			PC->PlayerInput->AddAxisMapping(FInputAxisKeyMapping(TEXT("LookYaw"), EKeys::Gamepad_RightX));
			PC->PlayerInput->AddAxisMapping(FInputAxisKeyMapping(TEXT("LookPitch"), EKeys::Gamepad_RightY));
			PC->PlayerInput->AddAxisMapping(FInputAxisKeyMapping(TEXT("LeftTriggerAxis"), EKeys::Gamepad_LeftTriggerAxis));
			PC->PlayerInput->AddAxisMapping(FInputAxisKeyMapping(TEXT("RightTriggerAxis"), EKeys::Gamepad_RightTriggerAxis));
			PC->PlayerInput->AddActionMapping(FInputActionKeyMapping(TEXT("LeftShoulder"), EKeys::Gamepad_LeftShoulder));
			PC->PlayerInput->AddActionMapping(FInputActionKeyMapping(TEXT("RightShoulder"), EKeys::Gamepad_RightShoulder));

			// Keyboard
			PC->PlayerInput->AddAxisMapping(FInputAxisKeyMapping(TEXT("MoveRight"), EKeys::D, 1.f));
			PC->PlayerInput->AddAxisMapping(FInputAxisKeyMapping(TEXT("MoveRight"), EKeys::A, -1.f));
			PC->PlayerInput->AddAxisMapping(FInputAxisKeyMapping(TEXT("MoveForward"), EKeys::W, 1.f));
			PC->PlayerInput->AddAxisMapping(FInputAxisKeyMapping(TEXT("MoveForward"), EKeys::S, -1.f));
			PC->PlayerInput->AddAxisMapping(FInputAxisKeyMapping(TEXT("LeftTriggerAxis"), EKeys::C, 1.f));
			PC->PlayerInput->AddAxisMapping(FInputAxisKeyMapping(TEXT("RightTriggerAxis"), EKeys::LeftControl, 1.f));

			// Mouse
			PC->PlayerInput->AddAxisMapping(FInputAxisKeyMapping(TEXT("LookYaw"), EKeys::MouseX));
			PC->PlayerInput->AddAxisMapping(FInputAxisKeyMapping(TEXT("LookPitch"), EKeys::MouseY));
		}
	}

	PlayerInputComponent->BindAxis(TEXT("MoveForward"), this,	&ThisClass::InputAxis_MoveForward);
	PlayerInputComponent->BindAxis(TEXT("MoveRight"), this,		&ThisClass::InputAxis_MoveRight);
	PlayerInputComponent->BindAxis(TEXT("LookYaw"), this,		&ThisClass::InputAxis_LookYaw);
	PlayerInputComponent->BindAxis(TEXT("LookPitch"), this,		&ThisClass::InputAxis_LookPitch);
	PlayerInputComponent->BindAxis(TEXT("RightTriggerAxis"), this,		&ThisClass::InputAxis_MoveUp);
	PlayerInputComponent->BindAxis(TEXT("LeftTriggerAxis"), this,		&ThisClass::InputAxis_MoveDown);

	PlayerInputComponent->BindAction(TEXT("LeftShoulder"), IE_Pressed, this, &ThisClass::Action_LeftShoulder_Pressed);
	PlayerInputComponent->BindAction(TEXT("LeftShoulder"), IE_Released, this, &ThisClass::Action_LeftShoulder_Released);
	PlayerInputComponent->BindAction(TEXT("RightShoulder"), IE_Pressed, this, &ThisClass::Action_RightShoulder_Pressed);
	PlayerInputComponent->BindAction(TEXT("RightShoulder"), IE_Released, this, &ThisClass::Action_RightShoulder_Released);
}

void ANetworkPredictionExtrasFlyingPawn::InputAxis_MoveForward(float Value)
{
	CachedMoveInput.X = FMath::Clamp(Value, -1.0f, 1.0f);
}

void ANetworkPredictionExtrasFlyingPawn::InputAxis_MoveRight(float Value)
{
	CachedMoveInput.Y = FMath::Clamp(Value, -1.0f, 1.0f);
}

void ANetworkPredictionExtrasFlyingPawn::InputAxis_LookYaw(float Value)
{
	CachedLookInput.X = FMath::Clamp(Value, -1.0f, 1.0f);
}

void ANetworkPredictionExtrasFlyingPawn::InputAxis_LookPitch(float Value)
{
	CachedLookInput.Y = FMath::Clamp(Value, -1.0f, 1.0f);
}

void ANetworkPredictionExtrasFlyingPawn::InputAxis_MoveUp(float Value)
{
	CachedMoveInput.Z = FMath::Clamp(CachedMoveInput.Z + Value, -1.0f, 1.0f);
}

void ANetworkPredictionExtrasFlyingPawn::InputAxis_MoveDown(float Value)
{
	CachedMoveInput.Z = FMath::Clamp(CachedMoveInput.Z - Value, -1.0f, 1.0f);
}

void ANetworkPredictionExtrasFlyingPawn::Tick( float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// Do whatever you want here. By now we have the latest movement state and latest input processed.

	if (bFakeAutonomousProxy && GetLocalRole() == ROLE_Authority)
	{
		if (GetRemoteRole() != ROLE_AutonomousProxy)
		{
			SetAutonomousProxy(true);
		}
	}
}

void ANetworkPredictionExtrasFlyingPawn::PrintDebug()
{
	UE_LOG(LogTemp, Warning, TEXT("======== ANetworkPredictionExtrasFlyingPawn::PrintDebug ========"));

	FSlowHeartBeatScope SuspendHeartBeat;
	FDisableHitchDetectorScope SuspendGameThreadHitch;

	PrintScriptCallstack();

	const SIZE_T StackTraceSize = 65535;
	ANSICHAR* StackTrace = (ANSICHAR*)FMemory::SystemMalloc(StackTraceSize);
	if (StackTrace != nullptr)
	{
		StackTrace[0] = 0;
		// Walk the stack and dump it to the allocated memory.
		FPlatformStackWalk::StackWalkAndDump(StackTrace, StackTraceSize, 1);
		UE_LOG(LogTemp, Log, TEXT("Call Stack:\n%s"), ANSI_TO_TCHAR(StackTrace));
		FMemory::SystemFree(StackTrace);
	}
}

float ANetworkPredictionExtrasFlyingPawn::GetMaxMoveSpeed() const
{
	if (const FFlyingMovementAuxState* AuxState = GetAuxStateRead())
	{
		return AuxState->MaxSpeed;
	}
	return 0;
}

void ANetworkPredictionExtrasFlyingPawn::SetMaxMoveSpeed(float NewMaxMoveSpeed)
{
	if (FFlyingMovementAuxState* AuxState = GetAuxStateWrite())
	{
		AuxState->MaxSpeed = NewMaxMoveSpeed;
	}
}

void ANetworkPredictionExtrasFlyingPawn::AddMaxMoveSpeed(float AdditiveMaxMoveSpeed)
{
	if (FFlyingMovementAuxState* AuxState = GetAuxStateWrite())
	{
		//UE_LOG(LogTemp, Warning, TEXT("MaxSpeed: %.2f += %.2f"), AuxState->MaxSpeed, AdditiveMaxMoveSpeed);
		AuxState->MaxSpeed += AdditiveMaxMoveSpeed;
	}
}

const FFlyingMovementAuxState* ANetworkPredictionExtrasFlyingPawn::GetAuxStateRead() const
{
	if (ensure(FlyingMovementComponent))
	{
		return FlyingMovementComponent->MovementAuxState.GetStateRead();
	}
	return nullptr;
}

FFlyingMovementAuxState* ANetworkPredictionExtrasFlyingPawn::GetAuxStateWrite()
{
	if (ensure(FlyingMovementComponent))
	{
		return FlyingMovementComponent->MovementAuxState.GetStateWrite(GetLocalRole() == ROLE_Authority);
	}
	return nullptr;
}

int32 ANetworkPredictionExtrasFlyingPawn::GetPendingFrame() const
{
	// TODO
	/*
	if (ensure(FlyingMovementComponent))
	{
		return FlyingMovementComponent->MovementSyncState->GetPendingFrame();
	}
	*/
	return 0;
}

void ANetworkPredictionExtrasFlyingPawn::ProduceInput(const FNetworkSimTime SimTime, FFlyingMovementInputCmd& Cmd)
{
	// Generate user commands. Called right before the flying movement simulation will tick (for a locally controlled pawn)
	// This isn't meant to be the best way of doing a camera system. It is just meant to show a couple of ways it may be done
	// and to make sure we can keep distinct the movement, rotation, and view angles.
	// Change with CVar NetworkPredictionExtras.FlyingPawn.CameraSyle. Styles 1-3 are really meant to be used with a gamepad.
	//
	// Its worth calling out: the code that happens here is happening *outside* of the flying movement simulation. All we are doing
	// is generating the input being fed into that simulation. That said, this means that A) the code below does not run on the server
	// (and non controlling clients) and B) the code is not rerun during reconcile/resimulates. Use this information guide any
	// decisions about where something should go (such as aim assist, lock on targeting systems, etc): it is hard to give absolute
	// answers and will depend on the game and its specific needs. In general, at this time, I'd recommend aim assist and lock on 
	// targeting systems to happen /outside/ of the system, i.e, here. But I can think of scenarios where that may not be ideal too.

	if (InputPreset == ENetworkPredictionExtrasFlyingInputPreset::Forward)
	{
		Cmd.MovementInput = FVector(1.f, 0.f, 0.f);
		Cmd.RotationInput = FRotator::ZeroRotator;
		return;
	}


	if (Controller == nullptr)
	{
		// We don't have a local controller so we can't run the code below. This is ok. Simulated proxies will just use previous input when extrapolating
		return;
	}
	
	if (USpringArmComponent* SpringComp = FindComponentByClass<USpringArmComponent>())
	{
		// This is not best practice: do not search for component every frame
		SpringComp->bUsePawnControlRotation = true;
	}

	// Simple input scaling. A real game will probably map this to an acceleration curve
	static float LookRateYaw = 150.f;
	static float LookRatePitch = 150.f;

	static float ControllerLookRateYaw = 1.5f;
	static float ControllerLookRatePitch = 1.5f;

	// Zero out input structs in case each path doesnt set each member. This is really all we are filling out here.
	Cmd.MovementInput = FVector::ZeroVector;
	Cmd.RotationInput = FRotator::ZeroRotator;

	const float DeltaTimeSeconds = SimTime.ToRealTimeSeconds();

	switch (FlyingPawnCVars::CameraStyle)
	{
		case 0:
		{
			// Fixed camera
			if (USpringArmComponent* SpringComp = FindComponentByClass<USpringArmComponent>())
			{
				// Only this camera mode has to set this to false
				SpringComp->bUsePawnControlRotation = false;
			}

			Cmd.RotationInput.Yaw = CachedLookInput.X * LookRateYaw;
			Cmd.RotationInput.Pitch = CachedLookInput.Y * LookRatePitch;
			Cmd.RotationInput.Roll = 0;
					
			Cmd.MovementInput = CachedMoveInput;
			break;
		}
		case 1:
		{
			// Free camera Restricted 2D movement on XY plane.
			APlayerController* PC = Cast<APlayerController>(Controller);
			if (ensure(PC)) // Requires player controller for now
			{
				// Camera yaw rotation
				PC->AddYawInput(CachedLookInput.X * ControllerLookRateYaw );
				PC->AddPitchInput(CachedLookInput.Y * ControllerLookRatePitch );

				static float RotationMagMin = (1e-3);
						
				float RotationInputYaw = 0.f;
				if (CachedMoveInput.Size() >= RotationMagMin)
				{
					FVector DesiredMovementDir = PC->GetControlRotation().RotateVector(CachedMoveInput);

					// 2D xy movement, relative to camera
					{
						FVector DesiredMoveDir2D = FVector(DesiredMovementDir.X, DesiredMovementDir.Y, 0.f);
						DesiredMoveDir2D.Normalize();

						const float DesiredYaw = DesiredMoveDir2D.Rotation().Yaw;
						const float DeltaYaw = DesiredYaw - GetActorRotation().Yaw;
						Cmd.RotationInput.Yaw = DeltaYaw / DeltaTimeSeconds;
					}
				}

				Cmd.MovementInput = FVector(FMath::Clamp<float>( CachedMoveInput.Size2D(), -1.f, 1.f ), 0.0f, CachedMoveInput.Z);
			}
			break;
		}
		case 2:
		{
			// Free camera on yaw and pitch, camera-relative movement.
			APlayerController* PC = Cast<APlayerController>(Controller);
			if (ensure(PC)) // Requires player controller for now
			{
				// Camera yaw rotation
				PC->AddYawInput(CachedLookInput.X * ControllerLookRateYaw );
				PC->AddPitchInput(CachedLookInput.Y * ControllerLookRatePitch );

				// Rotational movement: orientate us towards our camera-relative desired velocity (unless we are upside down, then flip it)
				static float RotationMagMin = (1e-3);
				const float MoveInputMag = CachedMoveInput.Size();
						
				float RotationInputYaw = 0.f;
				float RotationInputPitch = 0.f;
				float RotationInputRoll = 0.f;

				if (MoveInputMag >= RotationMagMin)
				{								
					FVector DesiredMovementDir = PC->GetControlRotation().RotateVector(CachedMoveInput);

					const float DesiredYaw = DesiredMovementDir.Rotation().Yaw;
					const float DeltaYaw = DesiredYaw - GetActorRotation().Yaw;

					const float DesiredPitch = DesiredMovementDir.Rotation().Pitch;
					const float DeltaPitch = DesiredPitch - GetActorRotation().Pitch;

					const float DesiredRoll = DesiredMovementDir.Rotation().Roll;
					const float DeltaRoll = DesiredRoll - GetActorRotation().Roll;

					// Kind of gross but because we want "instant" turning we must factor in delta time so that it gets factored out inside the simulation
					RotationInputYaw = DeltaYaw / DeltaTimeSeconds;
					RotationInputPitch = DeltaPitch / DeltaTimeSeconds;
					RotationInputRoll = DeltaRoll / DeltaTimeSeconds;
								
				}

				Cmd.RotationInput.Yaw = RotationInputYaw;
				Cmd.RotationInput.Pitch = RotationInputPitch;
				Cmd.RotationInput.Roll = RotationInputRoll;
						
				Cmd.MovementInput = FVector(FMath::Clamp<float>( MoveInputMag, -1.f, 1.f ), 0.0f, 0.0f); // Not the best way but simple
			}
			break;
		}
		case 3:
		{
			// Free camera on the yaw, camera-relative motion
			APlayerController* PC = Cast<APlayerController>(Controller);
			if (ensure(PC)) // Requires player controller for now
			{
				// Camera yaw rotation
				PC->AddYawInput(CachedLookInput.X * ControllerLookRateYaw );

				// Rotational movement: orientate us towards our camera-relative desired velocity (unless we are upside down, then flip it)
				static float RotationMagMin = (1e-3);
				const float MoveInputMag = CachedMoveInput.Size2D();
						
				float RotationInputYaw = 0.f;
				if (MoveInputMag >= RotationMagMin)
				{
					FVector DesiredMovementDir = PC->GetControlRotation().RotateVector(CachedMoveInput);

					const bool bIsUpsideDown = FVector::DotProduct(FVector(0.f, 0.f, 1.f), GetActorQuat().GetUpVector() ) < 0.f;
					if (bIsUpsideDown)
					{
						DesiredMovementDir *= -1.f;
					}							

					const float DesiredYaw = DesiredMovementDir.Rotation().Yaw;
					const float DeltaYaw = DesiredYaw - GetActorRotation().Yaw;

					// Kind of gross but because we want "instant" turning we must factor in delta time so that it gets factored out inside the simulation
					RotationInputYaw = DeltaYaw / DeltaTimeSeconds;
				}

				Cmd.RotationInput.Yaw = RotationInputYaw;
				Cmd.RotationInput.Pitch = CachedLookInput.Y * LookRatePitch; // Just pitch like normal
				Cmd.RotationInput.Roll = 0;
						
				Cmd.MovementInput = FVector(FMath::Clamp<float>( MoveInputMag, -1.f, 1.f ), 0.0f, CachedMoveInput.Z); // Not the best way but simple
			}
			break;
		}
	}
				

	CachedMoveInput = FVector::ZeroVector;
	CachedLookInput = FVector2D::ZeroVector;
}

// ------------------------------------------------------------------------

ANetworkPredictionExtrasFlyingPawn_MockAbility::ANetworkPredictionExtrasFlyingPawn_MockAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UMockFlyingAbilityComponent>(Name_FlyingMovementComponent))
{

}

UMockFlyingAbilityComponent* ANetworkPredictionExtrasFlyingPawn_MockAbility::GetMockFlyingAbilityComponent()
{
	return Cast<UMockFlyingAbilityComponent>(FlyingMovementComponent);
}

const UMockFlyingAbilityComponent* ANetworkPredictionExtrasFlyingPawn_MockAbility::GetMockFlyingAbilityComponent() const
{
	return Cast<UMockFlyingAbilityComponent>(FlyingMovementComponent);
}

void ANetworkPredictionExtrasFlyingPawn_MockAbility::ProduceInput(const FNetworkSimTime SimTime, FMockAbilityInputCmd& Cmd)
{
	Super::ProduceInput(SimTime, Cmd);

	switch(AbilityInputPreset)
	{
	case ENetworkPredictionExtrasMockAbilityInputPreset::None:
		Cmd.bSprintPressed = bSprintPressed;
		Cmd.bDashPressed = bDashPressed;
		Cmd.bBlinkPressed = bBlinkPressed;
		break;
	case ENetworkPredictionExtrasMockAbilityInputPreset::Sprint:
		Cmd.bSprintPressed = true;
		Cmd.bDashPressed = false;
		Cmd.bBlinkPressed = false;
		break;
	case ENetworkPredictionExtrasMockAbilityInputPreset::Dash:
		Cmd.bSprintPressed = false;
		Cmd.bDashPressed = true;
		Cmd.bBlinkPressed = false;
		break;
	case ENetworkPredictionExtrasMockAbilityInputPreset::Blink:
		Cmd.bSprintPressed = false;
		Cmd.bDashPressed = false;
		Cmd.bBlinkPressed = true;
		break;
	};
}

void ANetworkPredictionExtrasFlyingPawn_MockAbility::BeginPlay()
{
	Super::BeginPlay();

	if (UWorld* World = GetWorld())
	{
		if (UMockFlyingAbilityComponent* FlyingAbilityComponent = GetMockFlyingAbilityComponent())
		{
			FlyingAbilityComponent->ProduceInputDelegate.BindUObject(this, &ANetworkPredictionExtrasFlyingPawn_MockAbility::ProduceInput);
		}
	}
}

float ANetworkPredictionExtrasFlyingPawn_MockAbility::GetStamina() const
{
	if (const UMockFlyingAbilityComponent* FlyingAbilityComponent = GetMockFlyingAbilityComponent())
	{
		if (const FMockAbilitySyncState* SyncState = FlyingAbilityComponent->AbilitySyncState.GetStateRead())
		{
			return SyncState->Stamina;
		}
	}
	return 0.f;
}

float ANetworkPredictionExtrasFlyingPawn_MockAbility::GetMaxStamina() const
{
	if (const UMockFlyingAbilityComponent* FlyingAbilityComponent = GetMockFlyingAbilityComponent())
	{
		if (const FMockAbilityAuxState* AuxState = FlyingAbilityComponent->AbilityAuxState.GetStateRead())
		{
			return AuxState->MaxStamina;
		}
	}
	return 0.f;
}

void ANetworkPredictionExtrasFlyingPawn_MockAbility::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (APlayerController* PC = Cast<APlayerController>(Controller))
	{
		if (PC->PlayerInput)
		{
			//Gamepad
			PC->PlayerInput->AddActionMapping(FInputActionKeyMapping(TEXT("Sprint"), EKeys::Gamepad_LeftThumbstick));
			PC->PlayerInput->AddActionMapping(FInputActionKeyMapping(TEXT("Dash"), EKeys::Gamepad_FaceButton_Left));
			PC->PlayerInput->AddActionMapping(FInputActionKeyMapping(TEXT("Blink"), EKeys::Gamepad_FaceButton_Top));

			// Keyboard
			PC->PlayerInput->AddActionMapping(FInputActionKeyMapping(TEXT("Sprint"), EKeys::LeftShift));
			PC->PlayerInput->AddActionMapping(FInputActionKeyMapping(TEXT("Dash"), EKeys::Q));
			PC->PlayerInput->AddActionMapping(FInputActionKeyMapping(TEXT("Blink"), EKeys::E));
		}
	}

	PlayerInputComponent->BindAction(TEXT("Sprint"), IE_Pressed, this, &ThisClass::Action_Sprint_Pressed);
	PlayerInputComponent->BindAction(TEXT("Sprint"), IE_Released, this, &ThisClass::Action_Sprint_Released);

	PlayerInputComponent->BindAction(TEXT("Dash"), IE_Pressed, this, &ThisClass::Action_Dash_Pressed);
	PlayerInputComponent->BindAction(TEXT("Dash"), IE_Released, this, &ThisClass::Action_Dash_Released);

	PlayerInputComponent->BindAction(TEXT("Blink"), IE_Pressed, this, &ThisClass::Action_Blink_Pressed);
	PlayerInputComponent->BindAction(TEXT("Blink"), IE_Released, this, &ThisClass::Action_Blink_Released);
}

void ANetworkPredictionExtrasFlyingPawn_MockAbility::Action_Sprint_Pressed()
{
	bSprintPressed = true;
}

void ANetworkPredictionExtrasFlyingPawn_MockAbility::Action_Sprint_Released()
{
	bSprintPressed = false;
}

void ANetworkPredictionExtrasFlyingPawn_MockAbility::Action_Dash_Pressed()
{
	bDashPressed = true;
}

void ANetworkPredictionExtrasFlyingPawn_MockAbility::Action_Dash_Released()
{
	bDashPressed = false;
}

void ANetworkPredictionExtrasFlyingPawn_MockAbility::Action_Blink_Pressed()
{
	bBlinkPressed = true;
}

void ANetworkPredictionExtrasFlyingPawn_MockAbility::Action_Blink_Released()
{
	bBlinkPressed = false;
}
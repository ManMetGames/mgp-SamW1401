// Copyright Epic Games, Inc. All Rights Reserved.

#include "MGP_2526Character.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "CollisionQueryParams.h"
#include "DrawDebugHelpers.h"
#include "MGP_2526.h"

AMGP_2526Character::AMGP_2526Character()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// Create  and configure the wall run collider
	wallRunBox = CreateDefaultSubobject<UBoxComponent>(TEXT("Wall Run Check Trigger"));
	wallRunBox->SetupAttachment(RootComponent);
	wallRunBox->SetBoxExtent(FVector(1.f, 1.f, 1.f),true);


		
	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 500.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)
}

void AMGP_2526Character::Tick(float DeltaTime)
{
	// check yu need to enable ticking of the object
	Super::Tick(DeltaTime);














	// ------------------------------------------- Line Tracing -------------------------------------------

	// Get the starting position for the Linetraces
	traceStartingPosition = GetActorLocation();

	// Offset the trace height to not capture stairs, or cover etc. as often
	traceStartingPosition.Z += 25.f;

	// Get the right vector to use for finding the side offset for the endpoints on the traces
	FVector rightVector = GetActorRightVector();
	endPointRight = traceStartingPosition + (rightVector * WallRunCheckDistance);
	// Subtract from the right to find the left, left does not have a get vector
	endPointLeft = traceStartingPosition - (rightVector * WallRunCheckDistance);

	DetectWallsLineTrace();

	// For debugging purposes ( the float is lifetime )
	DrawDebugLine(GetWorld(), traceStartingPosition, endPointRight, FColor::Red, false, 0.01f);
	DrawDebugLine(GetWorld(), traceStartingPosition, endPointLeft, FColor::Red, false, 0.01f);

	// -----------------------------------------------------------------------------------------------------
}

void AMGP_2526Character::BeginPlay()
{
	Super::BeginPlay();

	// Variables Used for wall detection via trace
	WallRunCheckDistance = 75;
}

void AMGP_2526Character::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {
		
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &AMGP_2526Character::DoJumpStart);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &AMGP_2526Character::DoJumpEnd);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AMGP_2526Character::Move);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &AMGP_2526Character::Look);

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AMGP_2526Character::Look);
	}
	else
	{
		UE_LOG(LogMGP_2526, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

// --------------------------------------------------------------------- WallRun Detection Line Trace ------------------------------------------------------------------------------
void AMGP_2526Character::DetectWallsLineTrace()
{
	// Do Both line traces then put wether or not they hit into a boolean. Check both booleans, if either are true, wallrun on it, if both, check closest, if none, do nothing.


	// I know this should be done in the header, but the Unreal Header Tool really didnt like me putting it in there for some reason
	FCollisionQueryParams ignoreLinetraceParameters;
	// Ignore the player for the trace
	ignoreLinetraceParameters.AddIgnoredActor(this);

	// ******* MIGHT CHANGE ECC_VISIBILITY TO ONLY HIT WALLS AT A LATER DATE, KINDA LIKE TAGS IN UNITY, PROBABLY THE BEST BET TO NOT WALL RUN ON STAIRS OR SOMETHING *******

	bool traceHitRight = GetWorld()->LineTraceSingleByChannel(rayHitRight, traceStartingPosition,endPointRight,ECC_Visibility, ignoreLinetraceParameters);
	bool traceHitLeft = GetWorld()->LineTraceSingleByChannel(rayHitLeft, traceStartingPosition, endPointLeft, ECC_Visibility, ignoreLinetraceParameters);

	if (traceHitRight && traceHitLeft)
	{
		UE_LOG(LogTemp, Log, TEXT("Both Traces hit"));
	}

	else if (traceHitRight)
	{
		UE_LOG(LogTemp, Log, TEXT("Right Trace hit"));
	}

	else if (traceHitLeft)
	{
		UE_LOG(LogTemp, Log, TEXT("Left Trace hit"));
	}
}

void AMGP_2526Character::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	// route the input
	DoMove(MovementVector.X, MovementVector.Y);
}

void AMGP_2526Character::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	// route the input
	DoLook(LookAxisVector.X, LookAxisVector.Y);
}

void AMGP_2526Character::DoMove(float Right, float Forward)
{
	if (GetController() != nullptr)
	{
		// find out which way is forward
		const FRotator Rotation = GetController()->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

		// get right vector 
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// add movement 
		AddMovementInput(ForwardDirection, Forward);
		AddMovementInput(RightDirection, Right);
	}
}

void AMGP_2526Character::DoLook(float Yaw, float Pitch)
{
	if (GetController() != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void AMGP_2526Character::DoJumpStart()
{
	// signal the character to jump
	Jump();
}

void AMGP_2526Character::DoJumpEnd()
{
	// signal the character to stop jumping
	StopJumping();

	// Reset the boolean to unblock certain processes
}

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

	// Find the Velocity and Speed to determine wether or not the player has the speed for wallrunning, or to cancel a current wallrun
	currentVelocity = GetCharacterMovement()->Velocity;
	currentSpeed = currentVelocity.Size();

	// ------------------------------------------- Wall Running -------------------------------------------
	DetectWallsLineTrace();
	// -----------------------------------------------------------------------------------------------------
}

void AMGP_2526Character::BeginPlay()
{
	Super::BeginPlay();

	WallRunCheckDistance = 50;

	// Set the minimum speed high enough that you need to be moving sideways and cant just jump straight up
	wallRunMinSpeed = 175;

	// Assign charMove to this actors Movement controller, saves me writing the function every time and makes it more efficient cause it doesnt have to run the function EVERY TIME
	charMove = GetCharacterMovement();

	// Grounded Friction - Base Values
	// A factor that all friction coefficients are multiplied by to find the actual friction multiplier, lower = less
	charMove->BrakingFrictionFactor = 0.1f;
	charMove->GroundFriction = 2.5f;

	// Air Friction - Base Values
	charMove->FallingLateralFriction = 0.01f;
	charMove->BrakingDecelerationFalling = 0.1f;

	// Jumping Strength
	charMove->JumpZVelocity = 650.f;

	// Max Speed
	charMove->MaxWalkSpeed  = 700.f;

	isWallrunning = false;
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

	// Get Starting positions
	traceStartingPosition = GetActorLocation();

	// Copy the starting position and just move it down for grounded detection
	FVector groundedEndVector = traceStartingPosition;
	groundedEndVector.Z -= 100.f;

	traceStartingPosition.Z += 25.f;

	FVector rightVector = GetActorRightVector();
	endPointRight = traceStartingPosition + (rightVector * WallRunCheckDistance);
	// Subtract from the right to find the left, left does not have a get vector
	endPointLeft = traceStartingPosition - (rightVector * WallRunCheckDistance);

	// I know this should be done in the header, but the Unreal Header Tool really didnt like me putting it in there for some reason
	FCollisionQueryParams ignoreLinetraceParameters;
	// Ignore the player for the trace
	ignoreLinetraceParameters.AddIgnoredActor(this);

	traceHitGrounded = GetWorld()->LineTraceSingleByChannel(rayHitGrounded, traceStartingPosition, groundedEndVector, ECC_Visibility, ignoreLinetraceParameters);

	//Check when not grounded, if not grounded check if theres a wall to run on
	if (!traceHitGrounded&&currentSpeed>wallRunMinSpeed)
	{
		// Game Trace channel 2 is the custom trace channel "WallRunTraceChannel"
		traceHitRight = GetWorld()->LineTraceSingleByChannel(rayHitRight, traceStartingPosition, endPointRight, ECC_GameTraceChannel2, ignoreLinetraceParameters);
		traceHitLeft = GetWorld()->LineTraceSingleByChannel(rayHitLeft, traceStartingPosition, endPointLeft, ECC_GameTraceChannel2, ignoreLinetraceParameters);
	}
	else
	{
		// This catches the times when the player hugs a wall and lands, keeping it set to true after landing no matter if theres a wall or not
		traceHitRight = false;
		traceHitLeft = false;
	}



	// *** Temporary ***
	FColor bot = FColor::Red;
	FColor right = FColor::Red;
	FColor left = FColor::Red;
	if (traceHitGrounded)
	{
		// *** Temporary ***
		bot = FColor::Green;
	}

	if ((traceHitRight && traceHitLeft)&& !isWallrunning)
	{
		// *** Temporary ***
		right = FColor::Cyan;
		left = FColor::Cyan;

		// Ray Hit results contain a variable called time, showing how far along they were when they collided. the smaller time would be the closer wall so if both collide, use the smaller time when passing it into the WallRunVector function
		// Now Comes the complicated maths bit YAY
		// pass the normal of the collided object, think of it like a vector that defines the plane of the wall and always points perpendicular to it ( Like always up )
		if (rayHitRight.Time > rayHitLeft.Time)
		{
			StartWallRun(rayHitRight.ImpactNormal);
		}
		else
		{
			StartWallRun(rayHitLeft.ImpactNormal);
		}
	}

	else if (traceHitRight&& !isWallrunning)
	{
		// *** Temporary ***
		right = FColor::Green;
		StartWallRun(rayHitRight.ImpactNormal);
	}

	else if (traceHitLeft&&!isWallrunning)
	{
		// *** Temporary ***
		left = FColor::Green;
		StartWallRun(rayHitLeft.ImpactNormal);
	}
	// *** Temporary ***
	// Could make this a debug option, might be nicer
	// For debugging purposes ( the float is lifetime )
	DrawDebugLine(GetWorld(), traceStartingPosition, endPointRight, right, false, 0.01f);
	DrawDebugLine(GetWorld(), traceStartingPosition, endPointLeft, left, false, 0.01f);
	DrawDebugLine(GetWorld(), traceStartingPosition, groundedEndVector, bot, false, 0.01f);
}

void AMGP_2526Character::StartWallRun(FVector wallNormal)
{
	// Makes sure theres no 0's that break the division or multiplication
	FVector safeWallNormal = wallNormal.GetSafeNormal();
	// Right, So first we do the DotProduct of the current player velocity vector and wall normal vector to give us the vector of how alligned we are to the wall, and then minusing the current velocity from all of this gives only the movement along the wall, this is applied to the player and
	// this leaves us with only the velocity parralel to the wall. After we apply that velicty to the player I can also apply a force to them to give it a quick boost along the wall
	wallRunVelocity = currentVelocity - FVector::DotProduct(currentVelocity, safeWallNormal) * safeWallNormal;
	charMove->Velocity = wallRunVelocity;
	// Do all of this once, then move onto the function to keep the player moving in that direction and reset jumps etc
}

void AMGP_2526Character::EndWallRun()
{

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

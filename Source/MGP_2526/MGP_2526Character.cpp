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
	Super::Tick(DeltaTime);

	// Check if they run off of the wall and end it
	if (isWallrunning)
	{
		FCollisionQueryParams ignoreLinetraceParameters;
		ignoreLinetraceParameters.AddIgnoredActor(this);
		// Find the ending position based on where the wall normal is
		FVector traceEndPosition = traceStartingPosition - (currentWallNormal * WallRunCheckDistance);
		FHitResult wallStillHit;
		bool isWallStillThere = GetWorld()->LineTraceSingleByChannel(wallStillHit, traceStartingPosition, traceEndPosition, ECC_GameTraceChannel2, ignoreLinetraceParameters);

		// If there is no longer a wall
		if (!isWallStillThere)
		{
			EndWallRun();
		}
	}


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

	previousWallName = "start";

	WallRunCheckDistance = 75;

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

	launchStrength = 500.f;
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
	groundedEndVector.Z -= 93.f;

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

	if (traceHitGrounded)
	{
		EndWallRun();
		previousWallName = "none";
	}

	//Check when not grounded, if not grounded check if theres a wall to run on
	else if (!traceHitGrounded&&!isWallrunning)
	{
		// Game Trace channel 2 is the custom trace channel "WallRunTraceChannel"
		traceHitRight = GetWorld()->LineTraceSingleByChannel(rayHitRight, traceStartingPosition, endPointRight, ECC_GameTraceChannel2, ignoreLinetraceParameters);
		traceHitLeft = GetWorld()->LineTraceSingleByChannel(rayHitLeft, traceStartingPosition, endPointLeft, ECC_GameTraceChannel2, ignoreLinetraceParameters);


	}

	if ((traceHitRight && traceHitLeft) && !isWallrunning)
	{
		// Ray Hit results contain a variable called time, showing how far along they were when they collided. the smaller time would be the closer wall so if both collide, use the smaller time when passing it into the WallRunVector function
		// Now Comes the complicated maths bit YAY
		// pass the normal of the collided object, think of it like a vector that defines the plane of the wall and always points perpendicular to it ( Like always up )
		if (rayHitRight.Time > rayHitLeft.Time)
		{
			currentWallObject = rayHitRight.GetActor();
			WallRun(rayHitRight.ImpactNormal);
		}
		else if (rayHitLeft.Time>rayHitRight.Time)
		{
			currentWallObject = rayHitLeft.GetActor();
			WallRun(rayHitLeft.ImpactNormal);
		}
	}
	else if (traceHitRight&&!isWallrunning)
	{
		currentWallObject = rayHitRight.GetActor();
		WallRun(rayHitRight.ImpactNormal);
	}
	else if (traceHitLeft&&!isWallrunning)
	{
		currentWallObject = rayHitLeft.GetActor();
		WallRun(rayHitLeft.ImpactNormal);
	}
}




// +------------------------------------------------------------------------------------------------------------------------------+



void AMGP_2526Character::WallRun(FVector wallNormal)
{
	// Check if the wall is still there

	// Only do the wall run when the wall is not the same as the previous wall
	if (previousWallName != currentWallObject->GetName())
	{
		// Set the current walls normal to whatever one was collided with
		currentWallNormal = wallNormal.GetSafeNormal();

		// Get the velocity without movement into the wall
		FVector tangentVelocity = FVector::VectorPlaneProject(currentVelocity, currentWallNormal);

		// Get the cross product to figure out which direction along the normal we're moving
		FVector tangentCrossProduct = FVector::CrossProduct(currentWallNormal, FVector::UpVector).GetSafeNormal();

		if (FVector::DotProduct(tangentCrossProduct, tangentVelocity) < 0)
		{
			// Flip it if needed
			tangentCrossProduct *= -1.f;
		}


		// Take the velocity of the cross product which is now pointing the same direction as us and add a boost
		FVector forwardSpeedBoost = tangentCrossProduct * 900.f + FVector::UpVector * 250.f;

		// Rotate the player to face the direction of movement based on where we WILL be
		FRotator newRotation = (tangentCrossProduct + forwardSpeedBoost).GetSafeNormal().Rotation();
		newRotation.Pitch = 0.f;
		newRotation.Roll = 0.f;
		SetActorRotation(newRotation);

		// Ensure the player doesnt carry any of their momentum into the wall run, makes sure they all feel the same
		charMove->Velocity = FVector(0.f, 0.f, 0.f);
		// Apply the boost to the velocity along the tangent of the wall
		charMove->Velocity = tangentVelocity + forwardSpeedBoost;

		isWallrunning = true;
		// Set the prior walls name to the current wall, Used to make sure you cant re-run on the same wall
		previousWallName = currentWallObject->GetName();
	}
	else
	{
		EndWallRun();
	}
}

void AMGP_2526Character::EndWallRun()
{
	// Reset all variables to allow another wall run to commence
	isWallrunning = false;
	charMove->bOrientRotationToMovement = true;
	charMove->GravityScale = 1.f;
}

void AMGP_2526Character::WallJump(FVector wallNormal)
{
	// Move the player along the normal so they dont recollide with the same wall
	SetActorLocation(GetActorLocation()+currentWallNormal*10);
	FVector launchDirection = currentVelocity.GetClampedToMaxSize(1) + (wallNormal + FVector::UpVector * 1.f);
	charMove->Velocity = FVector (0,0,0);
	LaunchCharacter(launchDirection*launchStrength, false, false);
	EndWallRun();
}



// +------------------------------------------------------------------------------------------------------------------------------+



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
	if (!isWallrunning)
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
	if (isWallrunning)
	{
		WallJump(currentWallNormal);
	}



	Jump();
}

void AMGP_2526Character::DoJumpEnd()
{
	// signal the character to stop jumping
	StopJumping();
}

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "CollisionQueryParams.h"
#include "MGP_2526Character.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

/**
 *  A simple player-controllable third person character
 *  Implements a controllable orbiting camera
 */
UCLASS(abstract)
class AMGP_2526Character : public ACharacter
{
	GENERATED_BODY()

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;



	// ----------------------------------------------- Custom Variables --------------------------------------------------------- //
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Character Controller Components", meta = (AllowPrivateAccess = "true"))
	UCharacterMovementComponent* charMove;
	FVector currentWallNormal;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wall Trackers", meta = (AllowPrivateAccess = "true"))
	FString previousWallName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wall Trackers", meta = (AllowPrivateAccess = "true"))
	AActor* currentWallObject;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Wall Run Variables", meta = (AllowPrivateAccess = "true"))
	int WallRunCheckDistance;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Wall Run Variables", meta = (AllowPrivateAccess = "true"))
	float launchStrength;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Wall Run Variables", meta = (AllowPrivateAccess = "true"))
	FVector currentVelocity;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Wall Run Variables", meta = (AllowPrivateAccess = "true"))
	FVector wallRunVelocity;



	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wall Run Variables", meta = (AllowPrivateAccess = "true"))
	bool isWallrunning;




	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trace Variables", meta = (AllowPrivateAccess = "true"))
	FVector traceStartingPosition;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trace Variables", meta = (AllowPrivateAccess = "true"))
	FVector endPointRight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trace Variables", meta = (AllowPrivateAccess = "true"))
	FVector endPointLeft;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trace Variables", meta = (AllowPrivateAccess = "true"))
	FHitResult rayHitRight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trace Variables", meta = (AllowPrivateAccess = "true"))
	FHitResult rayHitLeft;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trace Variables", meta = (AllowPrivateAccess = "true"))
	FHitResult rayHitGrounded;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trace Variables", meta = (AllowPrivateAccess = "true"))
	bool traceHitRight;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trace Variables", meta = (AllowPrivateAccess = "true"))
	bool traceHitLeft;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trace Variables", meta = (AllowPrivateAccess = "true"))
	bool traceHitGrounded;
	// ------------------------------------------------------------------------------------------------------------------------------ //
	
protected:

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* LookAction;

	/** Mouse Look Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MouseLookAction;

public:

	/** Constructor */
	AMGP_2526Character();	

protected:

	// --- My Added Functions ---

	// Called for Wall Detection
	virtual void DetectWallsLineTrace();

	// Pass in the Normal Vector of the collided plane that the player will run across, locks movement to only along that plane
	virtual void WallRun(FVector wallNormal);

	// Add a launch force to the player away from the wall
	virtual void WallJump(FVector wallNormal);

	// Update
	virtual void Tick(float DeltaTime) override;

	// Start
	virtual void BeginPlay() override;

	//Reset all the variables for ending the wall run
	virtual void EndWallRun();







	/** Initialize input action bindings */
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

protected:

	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	/** Called for looking input */
	void Look(const FInputActionValue& Value);



public:

	/** Handles move inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	/** Handles look inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoLook(float Yaw, float Pitch);

	/** Handles jump pressed inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpStart();

	/** Handles jump pressed inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpEnd();

public:

	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }
};


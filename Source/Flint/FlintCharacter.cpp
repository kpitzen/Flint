// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Flint.h"
#include "FlintCharacter.h"
#include "PaperFlipbookComponent.h"
#include "Components/TextRenderComponent.h"



DEFINE_LOG_CATEGORY_STATIC(SideScrollerCharacter, Log, All);
//////////////////////////////////////////////////////////////////////////
// AFlintCharacter

AFlintCharacter::AFlintCharacter()
{


	// Use only Yaw from the controller and ignore the rest of the rotation.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = false;

	// Set the size of our collision capsule.
	GetCapsuleComponent()->SetCapsuleHalfHeight(96.0f);
	GetCapsuleComponent()->SetCapsuleRadius(40.0f);

	// Create a camera boom attached to the root (capsule)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 500.0f;
	CameraBoom->SocketOffset = FVector(0.0f, 0.0f, 75.0f);
	CameraBoom->bAbsoluteRotation = true;
	CameraBoom->bDoCollisionTest = false;
	CameraBoom->RelativeRotation = FRotator(0.0f, -90.0f, 0.0f);
	

	// Create an orthographic camera (no perspective) and attach it to the boom
	SideViewCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("SideViewCamera"));
	SideViewCameraComponent->ProjectionMode = ECameraProjectionMode::Orthographic;
	SideViewCameraComponent->OrthoWidth = 2048.0f;
	SideViewCameraComponent->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);

	// Prevent all automatic rotation behavior on the camera, character, and camera component
	CameraBoom->bAbsoluteRotation = true;
	SideViewCameraComponent->bUsePawnControlRotation = false;
	SideViewCameraComponent->bAutoActivate = true;
	GetCharacterMovement()->bOrientRotationToMovement = false;

	// Configure character movement
	GetCharacterMovement()->GravityScale = 2.0f;
	GetCharacterMovement()->AirControl = 0.80f;
	GetCharacterMovement()->JumpZVelocity = 1000.f;
	GetCharacterMovement()->GroundFriction = 3.0f;
	GetCharacterMovement()->MaxWalkSpeed = 600.0f;
	GetCharacterMovement()->MaxFlySpeed = 600.0f;
	GetCharacterMovement()->bUseSeparateBrakingFriction = true;
	GetCharacterMovement()->BrakingFriction = 5.0f;

	// Lock character motion onto the XZ plane, so the character can't move in or out of the screen
	GetCharacterMovement()->bConstrainToPlane = true;
	GetCharacterMovement()->SetPlaneConstraintNormal(FVector(0.0f, -1.0f, 0.0f));

	// Behave like a traditional 2D platformer character, with a flat bottom instead of a curved capsule bottom
	// Note: This can cause a little floating when going up inclines; you can choose the tradeoff between better
	// behavior on the edge of a ledge versus inclines by setting this to true or false
	GetCharacterMovement()->bUseFlatBaseForFloorChecks = true;

// 	TextComponent = CreateDefaultSubobject<UTextRenderComponent>(TEXT("IncarGear"));
// 	TextComponent->SetRelativeScale3D(FVector(3.0f, 3.0f, 3.0f));
// 	TextComponent->SetRelativeLocation(FVector(35.0f, 5.0f, 20.0f));
// 	TextComponent->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));
// 	TextComponent->SetupAttachment(RootComponent);

	// Enable replication on the Sprite component so animations show up when networked
	GetSprite()->SetIsReplicated(true);
	bReplicates = true;


	// Set dash speed
	DashMultiplier = 4.0f;

	// Set dash cooldown
	bDashCooldown = false;

	// Set the dash cooldown time
	DashCooldownTime = 2.0f;

	// Set the jump variables
	MaxJumpCount = 3;
	CurrentJumpCount = 0;
	MaxJumpTime = 2.0f;
}

//////////////////////////////////////////////////////////////////////////
// Animation

void AFlintCharacter::UpdateAnimation()
{
	const FVector PlayerVelocity = GetVelocity();
	const float PlayerSpeedSqr = PlayerVelocity.SizeSquared();

	// Are we moving or standing still?
	UPaperFlipbook* DesiredAnimation = (PlayerSpeedSqr > 0.0f) ? RunningAnimation : IdleAnimation;
	if( GetSprite()->GetFlipbook() != DesiredAnimation 	)
	{
		GetSprite()->SetFlipbook(DesiredAnimation);
	}
}

void AFlintCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	
	UpdateCharacter();
}


//////////////////////////////////////////////////////////////////////////
// Input

void AFlintCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	// Note: the 'Jump' action and the 'MoveRight' axis are bound to actual keys/buttons/sticks in DefaultInput.ini (editable from Project Settings..Input)
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);
	PlayerInputComponent->BindAxis("MoveRight", this, &AFlintCharacter::MoveRight);

	PlayerInputComponent->BindAction("Dash", IE_Pressed, this, &AFlintCharacter::Dash);

	PlayerInputComponent->BindTouch(IE_Pressed, this, &AFlintCharacter::TouchStarted);
	PlayerInputComponent->BindTouch(IE_Released, this, &AFlintCharacter::TouchStopped);
}

void AFlintCharacter::MoveRight(float Value)
{
	/*UpdateChar();*/

	// Apply the input to the character motion
	AddMovementInput(FVector(1.0f, 0.0f, 0.0f), Value);
}

void AFlintCharacter::Dash()
{
	//UE_LOG(LogTemp, Warning, TEXT("You tried to dash!"));
	// Cause the character to suddenly move in the direction it's currently traveling
	const FVector PlayerVelocity = GetVelocity();
	const float MaxWalkSpeed = GetCharacterMovement()->MaxWalkSpeed;
	const float DashSpeed = MaxWalkSpeed * (1.0f + AFlintCharacter::DashMultiplier);
	const float PlayerDirection = PlayerVelocity.X / fabs(PlayerVelocity.X);
	const FVector DashImpulse = FVector(PlayerDirection * MaxWalkSpeed * (1.0f + AFlintCharacter::DashMultiplier), 0.0, 0.0);
	//UE_LOG(LogTemp, Warning, TEXT("You tried to dash like this: %f %f"), MaxWalkSpeed, PlayerDirection)

	UE_LOG(LogTemp, Warning, TEXT("Dash cooldown is: %s"), bDashCooldown ? TEXT("TRUE") : TEXT("FALSE"))

	if (!bDashCooldown)
	{
		SetDashCooldown(true);
		GetCharacterMovement()->AddImpulse(DashImpulse, true);
		StartDashCooldown();
	}


}

void AFlintCharacter::TouchStarted(const ETouchIndex::Type FingerIndex, const FVector Location)
{
	// jump on any touch
	Jump();
}

void AFlintCharacter::TouchStopped(const ETouchIndex::Type FingerIndex, const FVector Location)
{
	StopJumping();
}

void AFlintCharacter::UpdateCharacter()
{
	// Update animation to match the motion
	UpdateAnimation();

	// Now setup the rotation of the controller based on the direction we are travelling
	const FVector PlayerVelocity = GetVelocity();	
	float TravelDirection = PlayerVelocity.X;
	// Set the rotation so that the character faces his direction of travel.
	if (Controller != nullptr)
	{
		if (TravelDirection < 0.0f)
		{
			Controller->SetControlRotation(FRotator(0.0, 180.0f, 0.0f));
		}
		else if (TravelDirection > 0.0f)
		{
			Controller->SetControlRotation(FRotator(0.0f, 0.0f, 0.0f));
		}
	}
}


void AFlintCharacter::StartDashCooldown()
{
	bool bDashIsOnCooldown = false;
	FTimerDelegate DashTimerDelegate = FTimerDelegate::CreateUObject(this, &AFlintCharacter::SetDashCooldown, bDashIsOnCooldown);
	GetWorldTimerManager().SetTimer(DashTimerHandle, DashTimerDelegate, DashCooldownTime, false, -1.0f);
}


void AFlintCharacter::Jump()
{
	const int JumpCountNow = GetCurrentJumpCount();
	UE_LOG(LogTemp, Warning, TEXT("You tried to jump!"))
	bPressedJump = true;
	JumpKeyHoldTime = MaxJumpTime;
	SetCurrentJumpCount(JumpCountNow + 1);
}

void AFlintCharacter::StopJumping()
{
	bPressedJump = false;
	ResetJumpState();
}

bool AFlintCharacter::CanJumpInternal_Implementation() const
{
	UE_LOG(LogTemp, Warning, TEXT("Can we jump? %d %d"), CurrentJumpCount, MaxJumpCount)
	return Super::CanJumpInternal_Implementation() || (CurrentJumpCount < MaxJumpCount && CurrentJumpCount > 0);
}

void AFlintCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);
	CurrentJumpCount = 0;
	SetDashCooldown(false);
	GetWorldTimerManager().ClearTimer(DashTimerHandle);
}

// Accessor functions
void AFlintCharacter::SetCurrentJumpCount(int NewJumpCount)
{
	CurrentJumpCount = NewJumpCount;
}

int AFlintCharacter::GetCurrentJumpCount()
{
	return CurrentJumpCount;
}

void AFlintCharacter::SetMaxJumpCount(int NewMaxJumpCount)
{
	MaxJumpCount = NewMaxJumpCount;
}

int AFlintCharacter::GetMaxJumpCount()
{
	return MaxJumpCount;
}

void AFlintCharacter::SetDashCooldown(bool CooldownState)
{
	bDashCooldown = CooldownState;
}

bool AFlintCharacter::GetDashCooldown()
{
	return bDashCooldown;
}
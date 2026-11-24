#include "PogoCharacter.h"

#include "Animation/AnimSequence.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "InputActionValue.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/GameplayStaticsTypes.h"
#include "NoSleepPogo.h"
#include "PogoArt.h"
#include "PogoGameInstance.h"
#include "PogoGameMode.h"
#include "PogoPlayerController.h"

static const FName TagFail(TEXT("PogoFail"));

APogoCharacter::APogoCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(15.f, 90.f);
	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->GravityScale = PogoGravityScale;
	Move->AirControl = 0.f;
	Move->MaxWalkSpeed = 600.f;
	Move->GroundFriction = 12.f;
	Move->BrakingDecelerationWalking = 6000.f;
	Move->BrakingDecelerationFalling = 0.f;
	Move->FallingLateralFriction = 0.f;
	Move->MaxAcceleration = 2000.f;
	Move->PerchRadiusThreshold = 10.f;
	Move->PerchAdditionalHeight = 5.f;
	Move->MaxStepHeight = 20.f;
	Move->SetWalkableFloorAngle(40.f);
	Move->bOrientRotationToMovement = false;
	Move->bCanWalkOffLedges = true;
	Move->JumpZVelocity = 0.f;
	Move->bImpartBaseVelocityZ = false;

	USkeletalMeshComponent* Rider = GetMesh();
	Rider->SetRelativeLocation(FVector(0.f, 0.f, -90.f));
	Rider->SetRelativeRotation(FRotator::ZeroRotator);
	Rider->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Rider->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	Rider->bCastDynamicShadow = true;
	Rider->CastShadow = true;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(GetCapsuleComponent());
	Camera->SetRelativeLocation(FVector(-40.f, 0.f, 62.f));
	Camera->bUsePawnControlRotation = true;
	Camera->SetFieldOfView(95.f);

	auto MakeRing = [this](const TCHAR* Name)
	{
		UStaticMeshComponent* R = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		R->SetupAttachment(GetCapsuleComponent());
		R->SetUsingAbsoluteLocation(true);
		R->SetUsingAbsoluteRotation(true);
		R->SetUsingAbsoluteScale(true);
		R->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		R->SetCastShadow(false);
		R->SetHiddenInGame(true);
		return R;
	};
	RingFull = MakeRing(TEXT("RingFull"));
	RingBroken = MakeRing(TEXT("RingBroken"));

	// Curvas por defecto (ajustables en el editor). Alturas en cm, velocidades en cm/s.
	if (FRichCurve* H = JumpHeightCurve.GetRichCurve())
	{
		H->AddKey(0.00f, 40.f);
		H->AddKey(0.25f, 82.f);
		H->AddKey(0.50f, 140.f);
		H->AddKey(0.75f, 215.f);
		H->AddKey(1.00f, 290.f);
	}
	if (FRichCurve* S = ForwardSpeedCurve.GetRichCurve())
	{
		S->AddKey(0.00f, 230.f);
		S->AddKey(0.50f, 380.f);
		S->AddKey(1.00f, 440.f);
	}
}

void APogoCharacter::BeginPlay()
{
	Super::BeginPlay();
	GetCharacterMovement()->GravityScale = PogoGravityScale;

	if (USkeletalMesh* SK = PogoArt::RiderMesh())
	{
		GetMesh()->SetSkeletalMesh(SK);
	}
	GetMesh()->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	AnimCompress = PogoArt::RiderAnim(TEXT("Compress"));
	AnimLetGo = PogoArt::RiderAnim(TEXT("LetGo"));
	AnimBalance = PogoArt::RiderAnim(TEXT("Balance"));
	RingFull->SetStaticMesh(PogoArt::Mesh(TEXT("SM_Ring")));
	RingBroken->SetStaticMesh(PogoArt::Mesh(TEXT("SM_RingBroken")));

	if (GetMesh()->GetSkeletalMeshAsset())
	{
		RestHead = GetMesh()->GetBoneLocation(TEXT("head"), EBoneSpaces::ComponentSpace);
		if (RestHead.IsNearlyZero())
		{
			RestHead = FVector(-40.f, 0.f, 152.f);
		}
	}
	EnterState(EPogoState::Ready);
}

// --------------------------------------------------------------------------------- entrada
void APogoCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	APogoPlayerController* PC = Cast<APogoPlayerController>(GetController());
	if (!EIC || !PC)
	{
		UE_LOG(LogPogo, Warning, TEXT("SetupPlayerInputComponent sin EnhancedInput o sin APogoPlayerController"));
		return;
	}
	PC->EnsureInput();
	EIC->BindAction(PC->IA_Move, ETriggerEvent::Triggered, this, &APogoCharacter::OnMove);
	EIC->BindAction(PC->IA_Move, ETriggerEvent::Completed, this, &APogoCharacter::OnMoveStop);
	EIC->BindAction(PC->IA_Look, ETriggerEvent::Triggered, this, &APogoCharacter::OnLook);
	EIC->BindAction(PC->IA_Charge, ETriggerEvent::Started, this, &APogoCharacter::OnChargeStart);
	EIC->BindAction(PC->IA_Charge, ETriggerEvent::Completed, this, &APogoCharacter::OnChargeStop);
}

void APogoCharacter::OnMove(const FInputActionValue& Value)
{
	if (bBotControlled)
	{
		return;
	}
	const FVector2D V = Value.Get<FVector2D>();
	MoveInput = FVector(V.X, V.Y, 0.f);
}

void APogoCharacter::OnMoveStop(const FInputActionValue& Value)
{
	if (!bBotControlled)
	{
		MoveInput = FVector::ZeroVector;
	}
}

void APogoCharacter::OnLook(const FInputActionValue& Value)
{
	if (bBotControlled)
	{
		return;
	}
	const FVector2D V = Value.Get<FVector2D>();
	float Sens = 1.f;
	bool bInvert = false;
	if (const UPogoGameInstance* GI = UPogoGameInstance::Get(this))
	{
		Sens = GI->Settings.MouseSensitivity;
		bInvert = GI->Settings.bInvertY;
	}
	AddControllerYawInput(V.X * Sens);
	AddControllerPitchInput((bInvert ? V.Y : -V.Y) * Sens);
}

void APogoCharacter::OnChargeStart(const FInputActionValue& Value)
{
	if (!bBotControlled)
	{
		SetChargeHeld(true);
	}
}

void APogoCharacter::OnChargeStop(const FInputActionValue& Value)
{
	if (!bBotControlled)
	{
		SetChargeHeld(false);
	}
}

void APogoCharacter::SetChargeHeld(bool bHeld)
{
	bChargeHeld = bHeld;
}

void APogoCharacter::BotLaunch(float Alpha)
{
	if (State != EPogoState::Air && State != EPogoState::Finished)
	{
		Launch(Alpha);
	}
}

FVector APogoCharacter::GetWorldMoveDir() const
{
	if (MoveInput.IsNearlyZero(0.05f))
	{
		return FVector::ZeroVector;
	}
	const FRotator Yaw(0.f, GetControlRotation().Yaw, 0.f);
	const FVector Fwd = FRotationMatrix(Yaw).GetUnitAxis(EAxis::X);
	const FVector Right = FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y);
	FVector Dir = Fwd * MoveInput.Y + Right * MoveInput.X;
	if (Dir.SizeSquared() > 1.f)
	{
		Dir.Normalize();
	}
	return Dir;
}

// --------------------------------------------------------------------------------- fisica
float APogoCharacter::GetPogoGravity() const
{
	return FMath::Abs(GetCharacterMovement()->GetGravityZ());
}

float APogoCharacter::GetJumpHeight(float Alpha) const
{
	const FRichCurve* C = JumpHeightCurve.GetRichCurveConst();
	return C ? C->Eval(FMath::Clamp(Alpha, 0.f, 1.f)) : 40.f + 250.f * Alpha;
}

float APogoCharacter::GetForwardSpeed(float Alpha) const
{
	const FRichCurve* C = ForwardSpeedCurve.GetRichCurveConst();
	return C ? C->Eval(FMath::Clamp(Alpha, 0.f, 1.f)) : 230.f + 210.f * Alpha;
}

FVector APogoCharacter::GetTipLocation() const
{
	return GetActorLocation() - FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
}

void APogoCharacter::Launch(float Alpha)
{
	Alpha = FMath::Clamp(Alpha, 0.f, 1.f);
	const float G = GetPogoGravity();
	const float Vz = FMath::Sqrt(2.f * G * GetJumpHeight(Alpha));
	const FVector Dir = GetWorldMoveDir();
	const float HSpeed = GetForwardSpeed(Alpha) * FMath::Min(1.f, Dir.Size());
	const FVector Horizontal = Dir.GetSafeNormal() * HSpeed;
	LastLaunchSpeed = HSpeed;

	LaunchCharacter(FVector(Horizontal.X, Horizontal.Y, Vz), true, true);
	CompressionVel -= 2.f + 6.f * Alpha;

	if (Alpha < 0.05f)
	{
		PogoArt::PlaySfx(this, TEXT("SFX_BounceSmall"), 0.35f, FMath::FRandRange(0.95f, 1.08f));
	}
	else
	{
		PogoArt::PlaySfx(this, TEXT("SFX_Boing"), 0.55f + 0.45f * Alpha, 1.25f - 0.45f * Alpha);
	}
	if (APogoGameMode* GM = GetWorld()->GetAuthGameMode<APogoGameMode>())
	{
		GM->NotifyTakeoff(this, Alpha);
	}
	EnterState(EPogoState::Air);
}

void APogoCharacter::EnterState(EPogoState NewState)
{
	State = NewState;
	StateTime = 0.f;
	if (NewState == EPogoState::Charging)
	{
		ChargeTime = 0.f;
		ChargeAlpha = 0.f;
		bChargeFullClicked = false;
		PogoArt::PlaySfx(this, TEXT("SFX_Compress"), 0.45f);
	}
	if (NewState == EPogoState::Contact)
	{
		ContactTimer = ContactGrace;
	}
	if (NewState == EPogoState::Air || NewState == EPogoState::Ready)
	{
		ChargeAlpha = 0.f;
	}
}

void APogoCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);
	const float Impact = FMath::Abs(GetCharacterMovement()->Velocity.Z);
	GetCharacterMovement()->StopMovementImmediately();

	if (State == EPogoState::Finished)
	{
		return;
	}

	APogoGameMode* GM = GetWorld()->GetAuthGameMode<APogoGameMode>();
	if (State == EPogoState::Ready)
	{
		return; // reaparicion: se apoya sin rebotar
	}

	const float Strength = FMath::Clamp(Impact / 1100.f, 0.f, 1.f);
	CompressionVel += 3.f + 9.f * Strength;
	CameraKickVel -= 40.f * Strength;
	PogoArt::PlaySfx(this, Strength > 0.6f ? TEXT("SFX_LandHeavy") : TEXT("SFX_Land"), 0.35f + 0.6f * Strength, FMath::FRandRange(0.92f, 1.06f));

	if (IsFailComponent(Hit.GetComponent()))
	{
		if (GM)
		{
			GM->PlayerFailed(this, EPogoFailReason::OffRoute);
		}
		return;
	}
	if (GM)
	{
		GM->NotifyLanded(this, Hit);
		if (State == EPogoState::Finished || State == EPogoState::Ready)
		{
			return; // la meta o una reaparicion cambiaron el estado
		}
	}
	EnterState(bChargeHeld ? EPogoState::Charging : EPogoState::Contact);
}

bool APogoCharacter::IsFailComponent(const UPrimitiveComponent* C) const
{
	if (!C)
	{
		return false;
	}
	if (C->ComponentHasTag(TagFail))
	{
		return true;
	}
	const AActor* A = C->GetOwner();
	return A && A->ActorHasTag(TagFail);
}

// --------------------------------------------------------------------------------- tick
void APogoCharacter::Tick(float Dt)
{
	Super::Tick(Dt);
	StateTime += Dt;
	UCharacterMovementComponent* Move = GetCharacterMovement();

	switch (State)
	{
	case EPogoState::Ready:
		if (Move->IsMovingOnGround())
		{
			Move->StopMovementImmediately();
			if (bChargeHeld)
			{
				EnterState(EPogoState::Charging);
			}
			else if (!MoveInput.IsNearlyZero(0.2f) && StateTime > 0.15f)
			{
				Launch(0.f);
			}
		}
		break;

	case EPogoState::Contact:
		Move->StopMovementImmediately();
		ContactTimer -= Dt;
		if (bChargeHeld)
		{
			EnterState(EPogoState::Charging);
		}
		else if (ContactTimer <= 0.f)
		{
			Launch(0.f);
		}
		break;

	case EPogoState::Charging:
		Move->StopMovementImmediately();
		ChargeTime += Dt;
		ChargeAlpha = FMath::Clamp(ChargeTime / ChargeTimeMax, 0.f, 1.f);
		if (ChargeAlpha >= 1.f && !bChargeFullClicked)
		{
			bChargeFullClicked = true;
			PogoArt::PlaySfx(this, TEXT("SFX_ChargeFull"), 0.8f);
		}
		if (!bChargeHeld)
		{
			Launch(ChargeAlpha);
		}
		else if (!Move->IsMovingOnGround() && StateTime > 0.05f)
		{
			// el apoyo desaparecio bajo los pies (borde): pasa a vuelo sin impulso
			EnterState(EPogoState::Air);
		}
		break;

	case EPogoState::Air:
	{
		// correccion corta en el aire
		const FVector Dir = GetWorldMoveDir();
		FVector V = Move->Velocity;
		FVector H(V.X, V.Y, 0.f);
		if (!Dir.IsNearlyZero())
		{
			H += Dir * AirAcceleration * Dt;
			const float Cap = FMath::Max(LastLaunchSpeed, AirMinSpeedCap);
			if (H.Size() > Cap)
			{
				H = H.GetSafeNormal() * Cap;
			}
			Move->Velocity = FVector(H.X, H.Y, V.Z);
		}
		if (Move->IsMovingOnGround() && StateTime > 0.1f)
		{
			// aterrizaje no reportado (p.ej. se deslizo de un borde): forzar contacto
			EnterState(bChargeHeld ? EPogoState::Charging : EPogoState::Contact);
		}
		break;
	}

	case EPogoState::Finished:
		Move->StopMovementImmediately();
		break;
	}

	UpdateCompression(Dt);
	UpdateAnimation();
	UpdateCamera(Dt);
	UpdatePrediction();
}

void APogoCharacter::UpdateCompression(float Dt)
{
	float Target = 0.f;
	switch (State)
	{
	case EPogoState::Charging: Target = 0.18f + 0.82f * ChargeAlpha; break;
	case EPogoState::Contact: Target = 0.3f; break;
	case EPogoState::Ready: Target = 0.04f; break;
	default: Target = 0.f; break;
	}
	const float K = State == EPogoState::Charging ? 420.f : 260.f;
	const float D = State == EPogoState::Charging ? 40.f : 14.f;
	CompressionVel += ((Target - Compression) * K - CompressionVel * D) * Dt;
	Compression = FMath::Clamp(Compression + CompressionVel * Dt, 0.f, 1.f);
	if ((Compression <= 0.f && CompressionVel < 0.f) || (Compression >= 1.f && CompressionVel > 0.f))
	{
		CompressionVel *= -0.25f;
	}

	CameraKickVel += (-CameraKick * 180.f - CameraKickVel * 16.f) * Dt;
	CameraKick += CameraKickVel * Dt;
}

void APogoCharacter::UpdateAnimation()
{
	USkeletalMeshComponent* M = GetMesh();
	if (!M->GetSkeletalMeshAsset())
	{
		return;
	}
	UAnimSequence* Want = AnimCompress;
	if (State == EPogoState::Finished)
	{
		Want = AnimLetGo;
	}
	else if (State == EPogoState::Ready && Compression < 0.08f)
	{
		Want = AnimBalance;
	}
	if (!Want)
	{
		return;
	}
	if (Want != CurrentAnim)
	{
		CurrentAnim = Want;
		const bool bLoop = Want == AnimBalance;
		M->PlayAnimation(Want, bLoop);
		M->SetPlayRate(Want == AnimCompress ? 0.f : 1.f);
	}
	if (Want == AnimCompress)
	{
		M->SetPosition(Compression * AnimCompress->GetPlayLength(), false);
	}
}

void APogoCharacter::UpdateCamera(float Dt)
{
	float Sway = 1.f;
	float Fov = 95.f;
	if (const UPogoGameInstance* GI = UPogoGameInstance::Get(this))
	{
		Sway = GI->Settings.Sway;
		Fov = GI->Settings.FOV;
	}
	Camera->SetFieldOfView(Fov);

	FVector Head = RestHead;
	if (GetMesh()->GetSkeletalMeshAsset())
	{
		Head = GetMesh()->GetBoneLocation(TEXT("head"), EBoneSpaces::ComponentSpace);
	}
	// el balanceo escala cuanto sigue la camara al cuerpo; en la meta siempre sigue
	const float Follow = State == EPogoState::Finished ? 1.f : Sway;
	FVector Local = FMath::Lerp(RestHead, Head, Follow);
	Local.Z += CameraKick * Sway * 0.1f;
	FVector Desired = GetMesh()->GetRelativeLocation() + Local;

	// evitar que la camara atraviese paredes (esta 40 cm detras de la punta)
	const FVector Center = GetActorLocation() + FVector(0.f, 0.f, Desired.Z);
	const FVector Target = GetActorTransform().TransformPosition(Desired);
	FHitResult Hit;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(PogoCam), false, this);
	if (GetWorld()->SweepSingleByChannel(Hit, Center, Target, FQuat::Identity, ECC_Camera, FCollisionShape::MakeSphere(8.f), Q)
		&& !Hit.bStartPenetrating && Hit.Time > 0.f)
	{
		Desired = GetActorTransform().InverseTransformPosition(Hit.Location);
	}
	Camera->SetRelativeLocation(Desired);
}

// --------------------------------------------------------------------------------- anillo de caida
FPogoLandingPrediction APogoCharacter::PredictLaunch(float Alpha, const FVector& WorldDir, float InputScale) const
{
	const float G = GetPogoGravity();
	const float Vz = FMath::Sqrt(2.f * G * GetJumpHeight(Alpha));
	const FVector H = WorldDir.GetSafeNormal() * GetForwardSpeed(Alpha) * FMath::Clamp(InputScale, 0.f, 1.f);

	FPredictProjectilePathParams Params;
	Params.StartLocation = GetTipLocation() + FVector(0.f, 0.f, 3.f);
	Params.LaunchVelocity = FVector(H.X, H.Y, Vz);
	Params.bTraceWithCollision = true;
	Params.bTraceWithChannel = true;
	Params.TraceChannel = ECC_Visibility;
	Params.ProjectileRadius = 0.f;
	Params.MaxSimTime = 3.f;
	Params.SimFrequency = 30.f;
	Params.OverrideGravityZ = -G;
	Params.ActorsToIgnore.Add(const_cast<APogoCharacter*>(this));

	FPredictProjectilePathResult Res;
	FPogoLandingPrediction Out;
	if (UGameplayStatics::PredictProjectilePath(this, Params, Res) && Res.HitResult.bBlockingHit)
	{
		Out.bHit = true;
		Out.Location = Res.HitResult.ImpactPoint;
		Out.Normal = Res.HitResult.ImpactNormal;
		Out.Time = Res.HitResult.Time * Params.MaxSimTime;
		if (Res.PathData.Num() > 0)
		{
			Out.Time = Res.PathData.Last().Time;
		}
		Out.Component = Res.HitResult.GetComponent();
		Out.bValid = Out.Normal.Z > 0.75f && !IsFailComponent(Res.HitResult.GetComponent());
	}
	return Out;
}

void APogoCharacter::UpdatePrediction()
{
	bool bShow = true;
	if (const UPogoGameInstance* GI = UPogoGameInstance::Get(this))
	{
		bShow = GI->Settings.bShowRing;
	}
	Prediction = FPogoLandingPrediction();

	if (State == EPogoState::Charging || State == EPogoState::Contact || State == EPogoState::Ready)
	{
		const FVector Dir = GetWorldMoveDir();
		Prediction = PredictLaunch(State == EPogoState::Charging ? ChargeAlpha : 0.f, Dir, Dir.Size());
	}
	else if (State == EPogoState::Air)
	{
		FPredictProjectilePathParams Params;
		Params.StartLocation = GetTipLocation() + FVector(0.f, 0.f, 3.f);
		Params.LaunchVelocity = GetCharacterMovement()->Velocity;
		Params.bTraceWithCollision = true;
		Params.bTraceWithChannel = true;
		Params.TraceChannel = ECC_Visibility;
		Params.MaxSimTime = 3.f;
		Params.SimFrequency = 30.f;
		Params.OverrideGravityZ = -GetPogoGravity();
		Params.ActorsToIgnore.Add(this);
		FPredictProjectilePathResult Res;
		if (UGameplayStatics::PredictProjectilePath(this, Params, Res) && Res.HitResult.bBlockingHit)
		{
			Prediction.bHit = true;
			Prediction.Location = Res.HitResult.ImpactPoint;
			Prediction.Normal = Res.HitResult.ImpactNormal;
			Prediction.Component = Res.HitResult.GetComponent();
			Prediction.bValid = Prediction.Normal.Z > 0.75f && !IsFailComponent(Res.HitResult.GetComponent());
		}
	}

	const bool bVisible = bShow && Prediction.bHit && State != EPogoState::Finished;
	RingFull->SetHiddenInGame(!(bVisible && Prediction.bValid));
	RingBroken->SetHiddenInGame(!(bVisible && !Prediction.bValid));
	if (bVisible)
	{
		UStaticMeshComponent* R = Prediction.bValid ? RingFull.Get() : RingBroken.Get();
		const FRotator Rot = FRotationMatrix::MakeFromZX(Prediction.Normal, FVector::ForwardVector).Rotator();
		R->SetWorldLocationAndRotation(Prediction.Location + Prediction.Normal * 1.5f, Rot);
		const float Scale = State == EPogoState::Air ? 0.55f : 0.7f;
		R->SetWorldScale3D(FVector(Scale, Scale, 1.f));
	}
}

// --------------------------------------------------------------------------------- reaparicion / meta
void APogoCharacter::ResetToSpawn(const FTransform& Spawn)
{
	const FVector Loc = Spawn.GetLocation() + FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2.f);
	SetActorLocationAndRotation(Loc, FRotator(0.f, Spawn.Rotator().Yaw, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
	if (AController* C = GetController())
	{
		C->SetControlRotation(FRotator(-8.f, Spawn.Rotator().Yaw, 0.f));
	}
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->SetMovementMode(MOVE_Falling);
	Compression = 0.f;
	CompressionVel = 0.f;
	LastLaunchSpeed = 0.f;
	EnterState(EPogoState::Ready);
}

void APogoCharacter::StartFinish()
{
	EnterState(EPogoState::Finished);
	GetCharacterMovement()->StopMovementImmediately();
	RingFull->SetHiddenInGame(true);
	RingBroken->SetHiddenInGame(true);
	FTimerHandle H;
	GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		PogoArt::PlaySfx(this, TEXT("SFX_PogoDrop"), 0.8f);
	}), 0.45f, false);
}

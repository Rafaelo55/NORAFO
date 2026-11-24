#include "PogoCar.h"

#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "PogoArt.h"
#include "PogoCharacter.h"
#include "PogoGameInstance.h"
#include "PogoGameMode.h"
#include "Sound/SoundAttenuation.h"

APogoCar::APogoCar()
{
	PrimaryActorTick.bCanEverTick = true;

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	SetRootComponent(Body);
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetMobility(EComponentMobility::Movable);

	Hazard = CreateDefaultSubobject<UBoxComponent>(TEXT("Hazard"));
	Hazard->SetupAttachment(Body);
	Hazard->SetBoxExtent(FVector(225.f, 100.f, 80.f));
	Hazard->SetRelativeLocation(FVector(0.f, 0.f, 78.f));
	Hazard->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Hazard->SetCollisionObjectType(ECC_WorldDynamic);
	Hazard->SetCollisionResponseToAllChannels(ECR_Ignore);
	Hazard->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Hazard->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Hazard->SetGenerateOverlapEvents(true);
	Hazard->ComponentTags.Add(TEXT("PogoFail"));

	Engine = CreateDefaultSubobject<UAudioComponent>(TEXT("Engine"));
	Engine->SetupAttachment(Body);
	Engine->bAutoActivate = false;
	Engine->bOverrideAttenuation = true;
	Engine->AttenuationOverrides.bAttenuate = true;
	Engine->AttenuationOverrides.bSpatialize = true;
	Engine->AttenuationOverrides.AttenuationShape = EAttenuationShape::Sphere;
	Engine->AttenuationOverrides.AttenuationShapeExtents = FVector(300.f, 0.f, 0.f);
	Engine->AttenuationOverrides.FalloffDistance = 3500.f;
}

void APogoCar::BeginPlay()
{
	Super::BeginPlay();
	Body->SetStaticMesh(PogoArt::Mesh(TEXT("SM_Car")));
	Hazard->OnComponentBeginOverlap.AddDynamic(this, &APogoCar::OnHazardOverlap);
	if (USoundBase* S = PogoArt::Sound(TEXT("SFX_CarLoop")))
	{
		Engine->SetSound(S);
		const UPogoGameInstance* GI = UPogoGameInstance::Get(this);
		Engine->SetVolumeMultiplier(0.8f * (GI ? GI->Settings.SfxVolume : 1.f));
		Engine->Play();
	}
}

void APogoCar::InitPath(const TArray<FVector>& Points, float InSpeed, float StartOffset)
{
	Starts.Reset();
	Ends.Reset();
	Lengths.Reset();
	Total = 0.f;
	for (int32 i = 0; i + 1 < Points.Num(); i += 2)
	{
		Starts.Add(Points[i]);
		Ends.Add(Points[i + 1]);
		const float L = FVector::Dist(Points[i], Points[i + 1]);
		Lengths.Add(L);
		Total += L;
	}
	Speed = InSpeed;
	Distance = FMath::Fmod(StartOffset * Speed, FMath::Max(Total, 1.f));
	FVector Dir;
	SetActorLocation(Eval(Distance, Dir));
	SmoothRot = Dir.Rotation();
	SetActorRotation(SmoothRot);
}

FVector APogoCar::Eval(float Dist, FVector& OutDir) const
{
	OutDir = FVector::ForwardVector;
	if (Total <= 0.f)
	{
		return GetActorLocation();
	}
	Dist = FMath::Fmod(Dist, Total);
	if (Dist < 0.f)
	{
		Dist += Total;
	}
	for (int32 i = 0; i < Lengths.Num(); ++i)
	{
		if (Dist <= Lengths[i] || i == Lengths.Num() - 1)
		{
			OutDir = (Ends[i] - Starts[i]).GetSafeNormal();
			return Starts[i] + OutDir * FMath::Min(Dist, Lengths[i]);
		}
		Dist -= Lengths[i];
	}
	return Starts[0];
}

void APogoCar::Tick(float Dt)
{
	Super::Tick(Dt);
	if (Total <= 0.f)
	{
		return;
	}
	const float Prev = Distance;
	Distance = FMath::Fmod(Distance + Speed * Dt, Total);
	FVector Dir;
	const FVector Loc = Eval(Distance, Dir);
	const FRotator Want = Dir.Rotation();
	const bool bJump = FVector::Dist(Loc, GetActorLocation()) > Speed * Dt * 3.f + 50.f || Distance < Prev;
	SmoothRot = bJump ? Want : FMath::RInterpTo(SmoothRot, Want, Dt, 9.f);
	SetActorLocationAndRotation(Loc, SmoothRot, false, nullptr, bJump ? ETeleportType::TeleportPhysics : ETeleportType::None);
}

bool APogoCar::WillOccupy(const FBox& Region, float From, float To) const
{
	const FBox R = Region.ExpandBy(FVector(260.f, 260.f, 0.f));
	for (float t = From; t <= To; t += 0.05f)
	{
		FVector Dir;
		const FVector P = Eval(Distance + Speed * t, Dir);
		if (P.X >= R.Min.X && P.X <= R.Max.X && P.Y >= R.Min.Y && P.Y <= R.Max.Y)
		{
			return true;
		}
	}
	return false;
}

void APogoCar::OnHazardOverlap(UPrimitiveComponent* OverlappedComp, AActor* Other, UPrimitiveComponent* OtherComp, int32 BodyIndex, bool bFromSweep, const FHitResult& Sweep)
{
	if (APogoCharacter* Pogo = Cast<APogoCharacter>(Other))
	{
		if (APogoGameMode* GM = GetWorld()->GetAuthGameMode<APogoGameMode>())
		{
			GM->PlayerFailed(Pogo, EPogoFailReason::Car);
		}
	}
}

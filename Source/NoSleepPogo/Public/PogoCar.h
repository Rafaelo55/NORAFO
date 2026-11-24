#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PogoCar.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class UAudioComponent;

/** Auto blanco que repite su recorrido (tramos rectos; entre tramos separados se teletransporta). */
UCLASS()
class NOSLEEPPOGO_API APogoCar : public AActor
{
	GENERATED_BODY()

public:
	APogoCar();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Points: pares consecutivos forman tramos (A0,B0,A1,B1,...). */
	void InitPath(const TArray<FVector>& Points, float InSpeed, float StartOffset);

	/** Para el bot: el auto ocupara la region en algun momento dentro de [Now+From, Now+To]? */
	bool WillOccupy(const FBox& Region, float From, float To) const;

	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Body;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> Hazard;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UAudioComponent> Engine;

private:
	FVector Eval(float Dist, FVector& OutDir) const;

	UFUNCTION()
	void OnHazardOverlap(UPrimitiveComponent* OverlappedComp, AActor* Other, UPrimitiveComponent* OtherComp, int32 BodyIndex, bool bFromSweep, const FHitResult& Sweep);

	TArray<FVector> Starts;
	TArray<FVector> Ends;
	TArray<float> Lengths;
	float Total = 0.f;
	float Speed = 900.f;
	float Distance = 0.f;
	FRotator SmoothRot = FRotator::ZeroRotator;
};

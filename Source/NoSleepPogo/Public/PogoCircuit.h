#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PogoCircuit.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class APlayerStart;
class APogoCar;

USTRUCT()
struct FPogoCheckpoint
{
	GENERATED_BODY()

	FBox Zone = FBox(ForceInit);
	FTransform Spawn;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> MarkerMat;
};

/** Objetivo para el bot de validacion: una caja de apoyo a la que hay que llegar. */
struct FPogoBotTarget
{
	FBox Box = FBox(ForceInit);
	bool bCarCheck = false;
	FString Label;
};

/**
 * Contenedor de toda la geometria de un circuito. Construye en C++ (a partir de los
 * modelos importados desde Blender) el recorrido descrito en el GDD.
 */
UCLASS()
class NOSLEEPPOGO_API APogoCircuit : public AActor
{
	GENERATED_BODY()

public:
	APogoCircuit();

	virtual void Tick(float DeltaSeconds) override;

	void Build(int32 Index);

	static FName HighLineTag(int32 N);
	void SetCheckpointActive(int32 Index);
	void SetGoalReached();

	// ---- datos del recorrido
	FTransform StartTransform;
	UPROPERTY() TArray<FPogoCheckpoint> Checkpoints;
	FBox GoalBox = FBox(ForceInit);
	float KillZ = -600.f;
	int32 HighLineCount = 0;
	TArray<FPogoBotTarget> BotRoute;
	TArray<FPogoBotTarget> BotRouteHigh;
	UPROPERTY() TArray<TObjectPtr<APogoCar>> Cars;
	UPROPERTY() TObjectPtr<APlayerStart> PlayerStart;
	FTransform MenuCameraFrom;
	FTransform MenuCameraTo;
	TArray<FTransform> ShotViews;

private:
	void BuildCuadra();
	void BuildGaleria();
	void BuildZigzag();
	void BuildYard();
	void FinishBuild();

	// ---- piezas
	UMaterialInterface* WorldMat(const FString& Tex, float Tile, const FLinearColor& Tint = FLinearColor::White);
	UStaticMeshComponent* AddMesh(const FString& Mesh, const FTransform& T, bool bCollide);
	UStaticMeshComponent* AddBox(const FVector& Min, const FVector& Max, const FString& Tex, float Tile = 200.f,
		const FLinearColor& Tint = FLinearColor::White, bool bCollide = true);
	UStaticMeshComponent* AddFail(const FVector& Min, const FVector& Max, const FString& Tex, float Tile = 300.f,
		const FLinearColor& Tint = FLinearColor::White);
	UStaticMeshComponent* AddSupport(const FVector& Min, const FVector& Max, const FString& Tex, float Tile = 200.f,
		const FLinearColor& Tint = FLinearColor::White, int32 HighLine = 0);
	void AddStripes(const FVector& Min, const FVector& Max, const FString& StripeMat);
	void AddAwning(float X0, float X1, float WallY, bool bFacesPlusY, float TopZ, float Depth, const FString& Fabric, int32 HighLine = 0);
	void AddFoldedAwning(float X0, float X1, float WallY, bool bFacesPlusY, float Z);
	void AddBuilding(const FVector& Min, const FVector& Max, const FString& Facade, int32 ShopFace, const FString& ShopTex,
		const FString& Roof = TEXT("Concrete"), bool bParapet = true);
	void QueueStreetFrontage(const FVector& Min, const FVector& Max, int32 ShopFace);
	void AddFrontageInstances(const TCHAR* MeshName, const TArray<FTransform>& Transforms);
	UStaticMeshComponent* AddProp(const FString& Mesh, const FVector& Loc, float Yaw, const FVector& Scale = FVector::OneVector,
		bool bCollide = true, const FString& SlotName = TEXT(""), const FString& MatOverride = TEXT(""));
	void AddBeam(const FVector& A, const FVector& B, float Thick, const FString& Tex);
	void AddPole(const FVector& Base, float Yaw);
	void AddWire(const FVector& A, const FVector& B, float Sag);
	void AddRailingLine(const FVector& A, const FVector& B);
	void AddCheckpoint(const FVector& Ground, float Yaw, const FBox& Zone);
	void AddGoal(const FVector& Center, float SizeAcross, float SizeAlong, float FacingYaw);
	void AddStart(const FVector& Ground, float Yaw);
	APogoCar* AddCar(const TArray<FVector>& Path, float Speed, float Offset);
	void AddSky(const FVector& Center);
	void AddSkyline(const FVector& Center, float Radius, int32 Count, float MinH, float MaxH);
	void AddBot(TArray<FPogoBotTarget>& Route, const FVector& Min, const FVector& Max, const FString& Label, bool bCar = false);

	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TMap<FString, TObjectPtr<UMaterialInstanceDynamic>> MatCache;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> GoalMat;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> BeaconMat;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Beacon;

	int32 CircuitIndex = 0;
	TArray<FTransform> ShopFronts;
	TArray<FTransform> WindowFronts;
	int32 ComponentCounter = 0;
	float Time = 0.f;
	bool bGoalReached = false;
	FVector BeaconBase = FVector::ZeroVector;
};

#include "PogoCircuit.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NoSleepPogo.h"
#include "PogoArt.h"
#include "PogoCar.h"
#include "PogoTypes.h"

namespace
{
	const FLinearColor White = FLinearColor::White;
	const FName TagFail(TEXT("PogoFail"));
	const FName TagSupport(TEXT("PogoSupport"));
	constexpr float SW = 15.f; // altura de la vereda

	const TCHAR* Facades[] = {TEXT("FacadeBeige"), TEXT("FacadeGray"), TEXT("FacadeBrown"), TEXT("FacadeTile")};
	const TCHAR* Shops[] = {TEXT("ShopA"), TEXT("ShopB"), TEXT("ShopC")};
	const TCHAR* Signs[] = {TEXT("SignRed"), TEXT("SignBlue"), TEXT("SignYellow")};
}

APogoCircuit::APogoCircuit()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Static);
	SetRootComponent(Root);
}

FName APogoCircuit::HighLineTag(int32 N)
{
	return FName(*FString::Printf(TEXT("PogoHL%d"), N));
}

// ======================================================================================== piezas
UMaterialInterface* APogoCircuit::WorldMat(const FString& Tex, float Tile, const FLinearColor& Tint)
{
	const FString Key = FString::Printf(TEXT("%s|%.0f|%.2f,%.2f,%.2f"), *Tex, Tile, Tint.R, Tint.G, Tint.B);
	if (TObjectPtr<UMaterialInstanceDynamic>* Found = MatCache.Find(Key))
	{
		return Found->Get();
	}
	UMaterialInterface* Parent = PogoArt::Master(TEXT("PS1_World"));
	if (!Parent)
	{
		return nullptr;
	}
	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Parent, this);
	MID->SetTextureParameterValue(TEXT("Tex"), PogoArt::Tex(Tex));
	MID->SetScalarParameterValue(TEXT("TileSize"), Tile);
	MID->SetVectorParameterValue(TEXT("Tint"), Tint);
	MatCache.Add(Key, MID);
	return MID;
}

UStaticMeshComponent* APogoCircuit::AddMesh(const FString& Mesh, const FTransform& T, bool bCollide)
{
	UStaticMesh* SM = PogoArt::Mesh(Mesh);
	if (!SM)
	{
		return nullptr;
	}
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this, *FString::Printf(TEXT("%s_%d"), *Mesh, ComponentCounter++));
	C->SetMobility(EComponentMobility::Static);
	C->SetupAttachment(Root);
	C->SetStaticMesh(SM);
	C->SetRelativeTransform(T);
	C->SetCollisionProfileName(bCollide ? UCollisionProfile::BlockAll_ProfileName : UCollisionProfile::NoCollision_ProfileName);
	C->SetGenerateOverlapEvents(false);
	C->bUseDefaultCollision = false;
	C->RegisterComponent();
	return C;
}

UStaticMeshComponent* APogoCircuit::AddBox(const FVector& Min, const FVector& Max, const FString& Tex, float Tile, const FLinearColor& Tint, bool bCollide)
{
	const FVector Size = (Max - Min).GetAbs();
	const FTransform T(FRotator::ZeroRotator, (Min + Max) * 0.5f, Size / 100.f);
	UStaticMeshComponent* C = AddMesh(TEXT("SM_Cube"), T, bCollide);
	if (C)
	{
		C->SetMaterial(0, WorldMat(Tex, Tile, Tint));
		if (!bCollide)
		{
			C->SetCastShadow(false);
		}
	}
	return C;
}

UStaticMeshComponent* APogoCircuit::AddFail(const FVector& Min, const FVector& Max, const FString& Tex, float Tile, const FLinearColor& Tint)
{
	UStaticMeshComponent* C = AddBox(Min, Max, Tex, Tile, Tint, true);
	if (C)
	{
		C->ComponentTags.Add(TagFail);
	}
	return C;
}

UStaticMeshComponent* APogoCircuit::AddSupport(const FVector& Min, const FVector& Max, const FString& Tex, float Tile, const FLinearColor& Tint, int32 HighLine)
{
	UStaticMeshComponent* C = AddBox(Min, Max, Tex, Tile, Tint, true);
	if (C)
	{
		C->ComponentTags.Add(TagSupport);
		if (HighLine > 0)
		{
			C->ComponentTags.Add(HighLineTag(HighLine));
		}
	}
	AddStripes(Min, Max, HighLine > 0 ? TEXT("HighLine") : TEXT("EdgeStripe"));
	return C;
}

void APogoCircuit::AddStripes(const FVector& Min, const FVector& Max, const FString& StripeMat)
{
	// GDD: "Dale canto y una franja clara a cada apoyo jugable"
	const float W = 9.f;
	const float P = 0.7f;
	const float Z0 = Max.Z - W;
	const float Z1 = Max.Z + P;
	const FVector Boxes[4][2] = {
		{FVector(Max.X - W, Min.Y - P, Z0), FVector(Max.X + P, Max.Y + P, Z1)},
		{FVector(Min.X - P, Min.Y - P, Z0), FVector(Min.X + W, Max.Y + P, Z1)},
		{FVector(Min.X - P, Max.Y - W, Z0), FVector(Max.X + P, Max.Y + P, Z1)},
		{FVector(Min.X - P, Min.Y - P, Z0), FVector(Max.X + P, Min.Y + W, Z1)},
	};
	UMaterialInterface* M = PogoArt::Mat(StripeMat);
	for (const auto& B : Boxes)
	{
		const FVector Size = B[1] - B[0];
		if (UStaticMeshComponent* C = AddMesh(TEXT("SM_Cube"), FTransform(FRotator::ZeroRotator, (B[0] + B[1]) * 0.5f, Size / 100.f), false))
		{
			C->SetMaterial(0, M);
			C->SetCastShadow(false);
		}
	}
}

void APogoCircuit::AddAwning(float X0, float X1, float WallY, bool bFacesPlusY, float TopZ, float Depth, const FString& Fabric, int32 HighLine)
{
	const float Width = X1 - X0;
	const float Cx = (X0 + X1) * 0.5f;
	const float Yaw = bFacesPlusY ? 180.f : 0.f;
	const float Out = bFacesPlusY ? 1.f : -1.f;
	if (UStaticMeshComponent* C = AddMesh(TEXT("SM_Awning"), FTransform(FRotator(0.f, Yaw, 0.f), FVector(Cx, WallY, TopZ), FVector(Width / 100.f, Depth / 100.f, 1.f)), false))
	{
		C->SetMaterialByName(TEXT("AwningRed"), PogoArt::Mat(Fabric));
		if (HighLine > 0)
		{
			C->SetMaterialByName(TEXT("EdgeStripe"), PogoArt::Mat(TEXT("HighLine")));
		}
	}
	// colision exacta con la tapa visible (GDD: el aterrizaje coincide con el borde visible)
	UBoxComponent* B = NewObject<UBoxComponent>(this, *FString::Printf(TEXT("AwningCol_%d"), ComponentCounter++));
	B->SetMobility(EComponentMobility::Static);
	B->SetupAttachment(Root);
	B->SetBoxExtent(FVector(Width * 0.5f, Depth * 0.5f, 6.f));
	B->SetRelativeLocation(FVector(Cx, WallY + Out * Depth * 0.5f, TopZ - 6.f));
	B->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	B->ComponentTags.Add(TagSupport);
	if (HighLine > 0)
	{
		B->ComponentTags.Add(HighLineTag(HighLine));
	}
	B->RegisterComponent();

	// farolitos colgando bajo los toldos comunes
	if (HighLine == 0 && (ComponentCounter % 3) == 0 && Width > 200.f)
	{
		for (float s : {-0.3f, 0.3f})
		{
			AddProp(TEXT("SM_Lantern"), FVector(Cx + s * Width, WallY + Out * Depth * 0.82f, TopZ - 100.f), 0.f, FVector::OneVector, false);
		}
	}
}

void APogoCircuit::AddFoldedAwning(float X0, float X1, float WallY, bool bFacesPlusY, float Z)
{
	const float Width = X1 - X0;
	AddMesh(TEXT("SM_AwningFolded"), FTransform(FRotator(0.f, bFacesPlusY ? 180.f : 0.f, 0.f), FVector((X0 + X1) * 0.5f, WallY, Z), FVector(Width / 100.f, 1.f, 1.f)), false);
}

void APogoCircuit::AddBuilding(const FVector& Min, const FVector& Max, const FString& Facade, int32 ShopFace, const FString& ShopTex, const FString& Roof, bool bParapet)
{
	AddBox(Min, FVector(Max.X, Max.Y, Max.Z - 2.f), Facade, 300.f);
	AddBox(FVector(Min.X, Min.Y, Max.Z - 2.f), Max, Roof, 200.f);
	if (bParapet)
	{
		const float T = 20.f;
		const float H = 35.f;
		const FString PM = TEXT("PlasterGray");
		AddBox(FVector(Min.X, Min.Y, Max.Z), FVector(Max.X, Min.Y + T, Max.Z + H), PM, 200.f);
		AddBox(FVector(Min.X, Max.Y - T, Max.Z), FVector(Max.X, Max.Y, Max.Z + H), PM, 200.f);
		AddBox(FVector(Min.X, Min.Y, Max.Z), FVector(Min.X + T, Max.Y, Max.Z + H), PM, 200.f);
		AddBox(FVector(Max.X - T, Min.Y, Max.Z), FVector(Max.X, Max.Y, Max.Z + H), PM, 200.f);
	}
	if (ShopFace > 0 && !ShopTex.IsEmpty())
	{
		const float P = 2.f;
		const float Top = FMath::Min(300.f, Max.Z - 2.f);
		FVector A, B;
		switch (ShopFace)
		{
		case 1: A = FVector(Min.X, Max.Y - 1.f, Min.Z); B = FVector(Max.X, Max.Y + P, Top); break;
		case 2: A = FVector(Min.X, Min.Y - P, Min.Z); B = FVector(Max.X, Min.Y + 1.f, Top); break;
		case 3: A = FVector(Max.X - 1.f, Min.Y, Min.Z); B = FVector(Max.X + P, Max.Y, Top); break;
		default: A = FVector(Min.X - P, Min.Y, Min.Z); B = FVector(Min.X + 1.f, Max.Y, Top); break;
		}
		AddBox(A, B, ShopTex, 300.f, White, false);
	}
}

UStaticMeshComponent* APogoCircuit::AddProp(const FString& Mesh, const FVector& Loc, float Yaw, const FVector& Scale, bool bCollide, const FString& SlotName, const FString& MatOverride)
{
	UStaticMeshComponent* C = AddMesh(Mesh, FTransform(FRotator(0.f, Yaw, 0.f), Loc, Scale), bCollide);
	if (C && !MatOverride.IsEmpty())
	{
		if (SlotName.IsEmpty())
		{
			C->SetMaterial(0, PogoArt::Mat(MatOverride));
		}
		else
		{
			C->SetMaterialByName(FName(*SlotName), PogoArt::Mat(MatOverride));
		}
	}
	return C;
}

void APogoCircuit::AddBeam(const FVector& A, const FVector& B, float Thick, const FString& Tex)
{
	const FVector D = B - A;
	const float Len = D.Size();
	if (Len < 1.f)
	{
		return;
	}
	if (UStaticMeshComponent* C = AddMesh(TEXT("SM_Cube"), FTransform(D.Rotation(), (A + B) * 0.5f, FVector(Len / 100.f, Thick / 100.f, Thick / 100.f)), false))
	{
		C->SetMaterial(0, PogoArt::Mat(Tex));
		C->SetCastShadow(true);
	}
}

void APogoCircuit::AddPole(const FVector& Base, float Yaw)
{
	AddProp(TEXT("SM_UtilityPole"), Base, Yaw, FVector::OneVector, true);
}

void APogoCircuit::AddWire(const FVector& A, const FVector& B, float Sag)
{
	const int32 Seg = 5;
	FVector Prev = A;
	for (int32 i = 1; i <= Seg; ++i)
	{
		const float t = float(i) / Seg;
		FVector P = FMath::Lerp(A, B, t);
		P.Z -= Sag * 4.f * t * (1.f - t);
		AddBeam(Prev, P, 2.5f, TEXT("Wire"));
		Prev = P;
	}
}

void APogoCircuit::AddRailingLine(const FVector& A, const FVector& B)
{
	const FVector D = B - A;
	const float Len = D.Size2D();
	const int32 N = FMath::Max(1, FMath::CeilToInt(Len / 200.f));
	const float Seg = Len / N;
	const FVector Dir = D.GetSafeNormal2D();
	for (int32 i = 0; i < N; ++i)
	{
		AddProp(TEXT("SM_Railing"), A + Dir * Seg * i, Dir.Rotation().Yaw, FVector(Seg / 203.f, 1.f, 1.f), true);
	}
}

void APogoCircuit::AddCheckpoint(const FVector& Ground, float Yaw, const FBox& Zone)
{
	FPogoCheckpoint CP;
	CP.Zone = Zone;
	CP.Spawn = FTransform(FRotator(0.f, Yaw, 0.f), Ground);
	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(PogoArt::Mat(TEXT("White")), this);
	MID->SetVectorParameterValue(TEXT("Tint"), FLinearColor(0.25f, 0.32f, 0.55f));
	CP.MarkerMat = MID;
	// cuadrado hueco como en el plano del GDD
	const float S = 70.f;
	const float T = 12.f;
	const FVector C = Ground + FVector(0.f, 0.f, 0.6f);
	const FVector Bars[4][2] = {
		{C + FVector(-S, -S, 0), C + FVector(S, -S + T, 1.2f)},
		{C + FVector(-S, S - T, 0), C + FVector(S, S, 1.2f)},
		{C + FVector(-S, -S, 0), C + FVector(-S + T, S, 1.2f)},
		{C + FVector(S - T, -S, 0), C + FVector(S, S, 1.2f)},
	};
	for (const auto& B : Bars)
	{
		if (UStaticMeshComponent* M = AddMesh(TEXT("SM_Cube"), FTransform(FRotator::ZeroRotator, (B[0] + B[1]) * 0.5f, (B[1] - B[0]) / 100.f), false))
		{
			M->SetMaterial(0, MID);
			M->SetCastShadow(false);
		}
	}
	Checkpoints.Add(CP);
}

void APogoCircuit::AddGoal(const FVector& Center, float SizeAcross, float SizeAlong, float FacingYaw)
{
	// mosaico: arriba de la imagen (el ojo) hacia adelante, texto legible al llegar
	const FTransform T(FRotator(0.f, FacingYaw + 90.f, 0.f), Center + FVector(0.f, 0.f, 0.8f), FVector(SizeAcross / 100.f, SizeAlong / 100.f, 1.f));
	if (UStaticMeshComponent* M = AddMesh(TEXT("SM_Plane"), T, false))
	{
		GoalMat = UMaterialInstanceDynamic::Create(PogoArt::Mat(TEXT("EyeMosaic")), this);
		GoalMat->SetScalarParameterValue(TEXT("Jitter"), 0.f);
		M->SetMaterial(0, GoalMat);
		M->SetCastShadow(false);
	}
	const FVector Ext = FRotator(0.f, FacingYaw, 0.f).RotateVector(FVector(SizeAlong * 0.5f, SizeAcross * 0.5f, 0.f)).GetAbs();
	GoalBox = FBox(Center - FVector(Ext.X, Ext.Y, 15.f), Center + FVector(Ext.X, Ext.Y, 260.f));

	// baliza azul flotante: la meta se ve desde lejos
	BeaconBase = Center + FVector(0.f, 0.f, 420.f);
	Beacon = AddMesh(TEXT("SM_Cube"), FTransform(FRotator(45.f, 0.f, 45.f), BeaconBase, FVector(0.55f)), false);
	if (Beacon)
	{
		Beacon->SetMobility(EComponentMobility::Movable);
		BeaconMat = UMaterialInstanceDynamic::Create(PogoArt::Mat(TEXT("White")), this);
		BeaconMat->SetVectorParameterValue(TEXT("Tint"), FLinearColor(0.08f, 0.22f, 1.f));
		BeaconMat->SetScalarParameterValue(TEXT("Emissive"), 3.f);
		Beacon->SetMaterial(0, BeaconMat);
		Beacon->SetCastShadow(false);
	}
}

void APogoCircuit::AddStart(const FVector& Ground, float Yaw)
{
	StartTransform = FTransform(FRotator(0.f, Yaw, 0.f), Ground);
	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	PlayerStart = GetWorld()->SpawnActor<APlayerStart>(APlayerStart::StaticClass(), FTransform(FRotator(0.f, Yaw, 0.f), Ground + FVector(0.f, 0.f, 93.f)), SP);
}

APogoCar* APogoCircuit::AddCar(const TArray<FVector>& Path, float Speed, float Offset)
{
	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	APogoCar* Car = GetWorld()->SpawnActor<APogoCar>(APogoCar::StaticClass(), FTransform::Identity, SP);
	Car->InitPath(Path, Speed, Offset);
	Cars.Add(Car);
	return Car;
}

void APogoCircuit::AddSky(const FVector& Center)
{
	if (UStaticMeshComponent* C = AddMesh(TEXT("SM_SkyDome"), FTransform(FRotator::ZeroRotator, Center - FVector(0.f, 0.f, 3000.f), FVector(700.f)), false))
	{
		C->SetCastShadow(false);
		C->bAffectDistanceFieldLighting = false;
	}
}

void APogoCircuit::AddSkyline(const FVector& Center, float Radius, int32 Count, float MinH, float MaxH)
{
	FRandomStream R(7 + CircuitIndex);
	for (int32 i = 0; i < Count; ++i)
	{
		const float A = 2.f * PI * (i + R.FRandRange(-0.3f, 0.3f)) / Count;
		const FVector P = Center + FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * Radius * R.FRandRange(0.85f, 1.2f);
		const FVector Half(R.FRandRange(500.f, 1100.f), R.FRandRange(500.f, 1100.f), 0.f);
		const float H = R.FRandRange(MinH, MaxH);
		AddBox(P - Half - FVector(0, 0, 50.f), P + Half + FVector(0, 0, H), Facades[R.RandRange(0, 3)], 300.f, White, false);
	}
}

void APogoCircuit::AddBot(TArray<FPogoBotTarget>& Route, const FVector& Min, const FVector& Max, const FString& Label, bool bCar)
{
	FPogoBotTarget T;
	T.Box = FBox(Min, Max);
	T.bCarCheck = bCar;
	T.Label = Label;
	Route.Add(T);
}

// ======================================================================================== API
void APogoCircuit::SetCheckpointActive(int32 Index)
{
	for (int32 i = 0; i < Checkpoints.Num(); ++i)
	{
		if (UMaterialInstanceDynamic* M = Checkpoints[i].MarkerMat)
		{
			const bool bOn = i == Index;
			M->SetVectorParameterValue(TEXT("Tint"), bOn ? FLinearColor(0.1f, 0.3f, 1.f) : FLinearColor(0.25f, 0.32f, 0.55f));
			M->SetScalarParameterValue(TEXT("Emissive"), bOn ? 2.5f : 0.f);
		}
	}
}

void APogoCircuit::SetGoalReached()
{
	bGoalReached = true;
}

void APogoCircuit::Tick(float Dt)
{
	Super::Tick(Dt);
	Time += Dt;
	if (GoalMat)
	{
		GoalMat->SetScalarParameterValue(TEXT("Emissive"), (bGoalReached ? 0.6f : 0.12f) + 0.1f * FMath::Sin(Time * 3.f));
	}
	if (Beacon)
	{
		const float Spin = bGoalReached ? 360.f : 70.f;
		Beacon->SetWorldLocationAndRotation(BeaconBase + FVector(0.f, 0.f, 25.f * FMath::Sin(Time * 1.6f)), FRotator(45.f, Time * Spin, 45.f));
	}
}

// ======================================================================================== construccion
void APogoCircuit::Build(int32 Index)
{
	CircuitIndex = Index;
	switch (Index)
	{
	case 1: BuildGaleria(); break;
	case 2: BuildZigzag(); break;
	case PogoCircuits::TestYard: BuildYard(); break;
	default: BuildCuadra(); break;
	}
	FinishBuild();
}

void APogoCircuit::FinishBuild()
{
	UE_LOG(LogPogo, Log, TEXT("Circuito %d construido: %d componentes, %d puntos, %d autos, bot %d/%d objetivos"),
		CircuitIndex, ComponentCounter, Checkpoints.Num(), Cars.Num(), BotRoute.Num(), BotRouteHigh.Num());
}

// ---------------------------------------------------------------------------------------- 01 La cuadra
void APogoCircuit::BuildCuadra()
{
	KillZ = -400.f;
	HighLineCount = 2;
	FRandomStream R(101);
	AddSky(FVector(4000.f, 0.f, 0.f));
	AddSkyline(FVector(4000.f, 0.f, 0.f), 11000.f, 26, 1200.f, 4200.f);

	// ---- calzada principal (fuera del recorrido) y vereda de enfrente
	AddFail(FVector(-5000, 250, -40), FVector(17000, 1650, 0), TEXT("Asphalt"), 400.f);
	for (float x = -4800.f; x < 16800.f; x += 600.f)
	{
		AddBox(FVector(x, 945, 0), FVector(x + 300, 965, 0.6f), TEXT("White"), 100.f, White, false);
	}
	AddBox(FVector(-5000, 1650, -40), FVector(17000, 2150, SW), TEXT("Paving"), 200.f);
	AddBox(FVector(-5000, 1645, -40), FVector(17000, 1675, SW + 1), TEXT("Curb"), 100.f);
	for (float x = -5000.f; x < 17000.f;)
	{
		const float W = R.FRandRange(700.f, 1300.f);
		AddBuilding(FVector(x, 2150, -40), FVector(x + W, 3600, R.FRandRange(800.f, 1900.f)), Facades[R.RandRange(0, 3)], 2, Shops[R.RandRange(0, 2)]);
		x += W;
	}

	// ---- zona 1: vereda de salida
	AddBox(FVector(-900, -250, -40), FVector(1200, 250, SW), TEXT("Tiles"), 200.f);
	AddBox(FVector(1200, -250, -40), FVector(2600, 250, SW), TEXT("Paving"), 200.f);
	AddBox(FVector(-900, 225, -40), FVector(2600, 255, SW + 1), TEXT("Curb"), 100.f);
	AddBox(FVector(-900, -75, SW), FVector(2600, -45, SW + 0.4f), TEXT("GuideTile"), 60.f, White, false);
	AddBuilding(FVector(-2000, -1600, -40), FVector(-900, 250, 1300), TEXT("FacadeGray"), 3, TEXT("ShopB"));
	AddBuilding(FVector(-900, -1600, -40), FVector(300, -250, 900), TEXT("FacadeBeige"), 1, TEXT("ShopA"));
	AddBuilding(FVector(300, -1600, -40), FVector(1350, -250, 1500), TEXT("FacadeTile"), 1, TEXT("ShopC"));
	AddBuilding(FVector(1350, -1600, -40), FVector(2350, -250, 1200), TEXT("FacadeBrown"), 1, TEXT("ShopB"));

	AddStart(FVector(0, 0, SW), 0.f);
	// apoyos de practica: probar altura y direccion antes del cruce
	AddSupport(FVector(450, -210, SW), FVector(700, 30, 45), TEXT("Concrete"), 150.f);
	AddSupport(FVector(950, -210, SW), FVector(1200, 30, 100), TEXT("PlasterGray"), 150.f);
	AddProp(TEXT("SM_Vending"), FVector(1520, -208, SW), 90.f);
	AddProp(TEXT("SM_Vending"), FVector(1620, -208, SW), 90.f);
	AddProp(TEXT("SM_CrateStack"), FVector(1760, -200, SW), 10.f);
	AddProp(TEXT("SM_Planter"), FVector(250, -195, SW), 0.f);
	for (float x = -700.f; x < 2500.f; x += 300.f)
	{
		AddProp(TEXT("SM_Bollard"), FVector(x, 212, SW), 0.f);
	}
	AddCheckpoint(FVector(1950, 0, SW), 0.f, FBox(FVector(1750, -250, -60), FVector(2600, 250, 500)));
	AddFoldedAwning(-700, -100, -250, true, 290);
	AddFoldedAwning(450, 1150, -250, true, 300);
	AddProp(TEXT("SM_SignVertical"), FVector(100, -195, 360), 180.f, FVector::OneVector, false, TEXT("SignRed"), TEXT("SignBlue"));
	AddProp(TEXT("SM_SignVertical"), FVector(1500, -195, 400), 180.f, FVector::OneVector, false, TEXT("SignRed"), TEXT("SignRed"));
	AddProp(TEXT("SM_SignVertical"), FVector(2200, -195, 380), 180.f, FVector::OneVector, false, TEXT("SignRed"), TEXT("SignYellow"));
	for (float x : {-500.f, 700.f, 1800.f})
	{
		AddProp(TEXT("SM_ACUnit"), FVector(x, -236, 650 + R.FRandRange(-40.f, 60.f)), 90.f, FVector::OneVector, false);
	}

	// ---- zona 2: cruce con trafico
	AddFail(FVector(2600, -4700, -40), FVector(4000, -250, 0), TEXT("Asphalt"), 400.f);
	AddBox(FVector(2600, -250, -40), FVector(4000, 250, 1), TEXT("Asphalt"), 400.f); // senda peatonal: segura
	for (float x = 2640.f; x < 3960.f; x += 100.f)
	{
		if (x + 50.f < 3150.f || x > 3450.f)
		{
			AddBox(FVector(x, -235, 1), FVector(x + 50, 235, 1.6f), TEXT("White"), 100.f, White, false);
		}
	}
	AddSupport(FVector(3150, -300, -40), FVector(3450, 300, SW), TEXT("Concrete"), 150.f); // refugio
	for (const FVector& P : {FVector(3175, -275, SW), FVector(3425, -275, SW), FVector(3175, 275, SW), FVector(3425, 275, SW)})
	{
		AddProp(TEXT("SM_Bollard"), P, 0.f);
	}
	AddBox(FVector(2620, -440, 0), FVector(3140, -400, 0.6f), TEXT("White"), 100.f, White, false);
	AddBox(FVector(3460, 400, 0), FVector(3980, 440, 0.6f), TEXT("White"), 100.f, White, false);
	for (float y = -4500.f; y < -600.f; y += 500.f)
	{
		AddBox(FVector(3290, y, 0), FVector(3310, y + 250, 0.6f), TEXT("White"), 100.f, White, false);
	}
	AddBox(FVector(2350, -4700, -40), FVector(2600, -250, SW), TEXT("Paving"), 200.f);
	AddBox(FVector(4000, -4700, -40), FVector(4250, -250, SW), TEXT("Paving"), 200.f);
	AddBuilding(FVector(1350, -4700, -40), FVector(2350, -1600, 1100), TEXT("FacadeGray"), 3, TEXT("ShopA"));
	AddBuilding(FVector(4250, -4700, -40), FVector(5400, -1600, 1400), TEXT("FacadeBeige"), 4, TEXT("ShopC"));
	AddBuilding(FVector(1600, -6000, -40), FVector(5000, -4700, 1500), TEXT("FacadeTile"), 1, TEXT("ShopB"));
	AddProp(TEXT("SM_RoadSign"), FVector(2480, 215, SW), 180.f);
	AddProp(TEXT("SM_RoadSign"), FVector(4120, -215, SW), 0.f);
	// el auto blanco: baja por el carril oeste, gira en la avenida y sube por el este
	AddCar({FVector(2875, -4400, 0), FVector(2875, 800, 0),
	        FVector(2875, 800, 0), FVector(3300, 1250, 0),
	        FVector(3300, 1250, 0), FVector(3725, 800, 0),
	        FVector(3725, 800, 0), FVector(3725, -4400, 0)}, 900.f, 0.f);

	// ---- zona 3: frente comercial
	AddBox(FVector(4000, -250, -40), FVector(6700, 250, SW), TEXT("Paving"), 200.f);
	AddBox(FVector(4000, 225, -40), FVector(6700, 255, SW + 1), TEXT("Curb"), 100.f);
	AddBox(FVector(4000, -75, SW), FVector(6700, -45, SW + 0.4f), TEXT("GuideTile"), 60.f, White, false);
	AddCheckpoint(FVector(4450, 0, SW), 0.f, FBox(FVector(4000, -250, -60), FVector(4900, 250, 500)));
	AddBuilding(FVector(4250, -1600, -40), FVector(5150, -250, 900), TEXT("FacadeBeige"), 1, TEXT("ShopA"));
	AddBuilding(FVector(5150, -1600, -40), FVector(6050, -250, 1200), TEXT("FacadeBrown"), 1, TEXT("ShopB"));
	AddBuilding(FVector(6050, -1600, -40), FVector(6700, -250, 750), TEXT("FacadeGray"), 1, TEXT("ShopC"));
	AddAwning(4350, 4800, -250, true, 260, 100, TEXT("AwningBlue"));
	AddFoldedAwning(4900, 5300, -250, true, 300);
	// ruta principal: apoyos anchos a media altura (4 saltos)
	AddSupport(FVector(5300, -40, SW), FVector(5600, 220, 100), TEXT("PlasterBeige"), 150.f);
	AddSupport(FVector(5800, -40, SW), FVector(6050, 200, 190), TEXT("Concrete"), 150.f);
	AddSupport(FVector(6250, -40, SW), FVector(6500, 200, 275), TEXT("PlasterGray"), 150.f);
	AddProp(TEXT("SM_CrateStack"), FVector(5250, 180, SW), 0.f);
	// linea alta: dos toldos estrechos (3 saltos)
	AddAwning(5450, 5700, -250, true, 215, 120, TEXT("AwningRed"), 1);
	AddAwning(6150, 6400, -250, true, 320, 110, TEXT("AwningRed"), 2);
	AddProp(TEXT("SM_SignVertical"), FVector(4600, -195, 420), 180.f, FVector::OneVector, false, TEXT("SignRed"), TEXT("SignYellow"));
	AddProp(TEXT("SM_SignVertical"), FVector(5950, -195, 520), 180.f, FVector::OneVector, false, TEXT("SignRed"), TEXT("SignRed"));
	AddProp(TEXT("SM_ACUnit"), FVector(4500, -236, 640), 90.f, FVector::OneVector, false);
	AddProp(TEXT("SM_ACUnit"), FVector(5400, -236, 700), 90.f, FVector::OneVector, false);
	AddProp(TEXT("SM_ACUnit"), FVector(6300, -236, 560), 90.f, FVector::OneVector, false);
	for (float x = 4300.f; x < 5200.f; x += 300.f)
	{
		AddProp(TEXT("SM_Bollard"), FVector(x, 212, SW), 0.f);
	}

	// ---- zona 4: azotea del ojo (las dos rutas llegan al mismo techo)
	const float RoofZ = 365.f;
	AddBuilding(FVector(6700, -1600, -40), FVector(8700, 250, RoofZ), TEXT("FacadeTile"), 4, TEXT("ShopA"), TEXT("RoofMosaic"), false);
	AddBuilding(FVector(8700, -1600, -40), FVector(10500, 250, 1300), TEXT("FacadeBrown"), 2, TEXT("ShopC"));
	AddRailingLine(FVector(6800, 240, RoofZ), FVector(8690, 240, RoofZ));
	AddRailingLine(FVector(8690, -1590, RoofZ), FVector(6720, -1590, RoofZ));
	AddSupport(FVector(6750, -1590, RoofZ), FVector(7150, -1150, RoofZ + 240), TEXT("PlasterGray"), 150.f);
	AddProp(TEXT("SM_WaterTank"), FVector(8350, -1250, RoofZ), 0.f);
	AddProp(TEXT("SM_RoofVent"), FVector(7300, 80, RoofZ), 0.f);
	AddProp(TEXT("SM_ACUnit"), FVector(8500, 100, RoofZ), 0.f);
	AddGoal(FVector(7800, -600, RoofZ), 640.f, 400.f, 0.f);

	// ---- postes y cables
	TArray<FVector> Poles = {FVector(-500, 205, SW), FVector(1300, 205, SW), FVector(2450, 205, SW), FVector(4600, 205, SW), FVector(6300, 205, SW)};
	for (const FVector& P : Poles)
	{
		AddPole(P, 0.f);
	}
	for (int32 i = 0; i + 1 < Poles.Num(); ++i)
	{
		for (float o : {-70.f, 70.f})
		{
			AddWire(Poles[i] + FVector(0, o, 845), Poles[i + 1] + FVector(0, o, 845), 60.f);
		}
	}
	for (float x : {0.f, 2000.f, 4200.f, 6400.f})
	{
		AddPole(FVector(x, 1700, SW), 0.f);
	}
	AddWire(Poles[1] + FVector(0, 70, 845), FVector(2000, 1630, 845 + SW), 90.f);
	AddWire(Poles[3] + FVector(0, 70, 845), FVector(4200, 1630, 845 + SW), 90.f);

	// ---- bot de validacion
	const FVector P0(1750, -200, SW), P1(2550, 200, SW);
	for (TArray<FPogoBotTarget>* Route : {&BotRoute, &BotRouteHigh})
	{
		AddBot(*Route, FVector(1000, -150, SW), FVector(1600, 150, SW), TEXT("vereda"));
		AddBot(*Route, P0, P1, TEXT("punto A"));
		AddBot(*Route, FVector(2700, -150, 1), FVector(3080, 150, 1), TEXT("carril oeste"), true);
		AddBot(*Route, FVector(3180, -200, SW), FVector(3420, 200, SW), TEXT("refugio"), true);
		AddBot(*Route, FVector(3520, -150, 1), FVector(3900, 150, 1), TEXT("carril este"), true);
		AddBot(*Route, FVector(4100, -150, SW), FVector(4800, 150, SW), TEXT("punto B"), true);
	}
	AddBot(BotRoute, FVector(4950, 0, SW), FVector(5200, 180, SW), TEXT("bifurcacion"));
	AddBot(BotRoute, FVector(5300, -40, 100), FVector(5600, 220, 100), TEXT("apoyo 1"));
	AddBot(BotRoute, FVector(5800, -40, 190), FVector(6050, 200, 190), TEXT("apoyo 2"));
	AddBot(BotRoute, FVector(6250, -40, 275), FVector(6500, 200, 275), TEXT("apoyo 3"));
	AddBot(BotRoute, FVector(6760, -200, RoofZ), FVector(7300, 200, RoofZ), TEXT("azotea"));
	AddBot(BotRoute, GoalBox.Min * FVector(1, 1, 0) + FVector(0, 0, RoofZ), GoalBox.Max * FVector(1, 1, 0) + FVector(0, 0, RoofZ), TEXT("ojo azul"));
	AddBot(BotRouteHigh, FVector(4950, -230, SW), FVector(5150, -100, SW), TEXT("bifurcacion alta"));
	AddBot(BotRouteHigh, FVector(5450, -250, 215), FVector(5700, -130, 215), TEXT("toldo 1"));
	AddBot(BotRouteHigh, FVector(6150, -250, 320), FVector(6400, -140, 320), TEXT("toldo 2"));
	AddBot(BotRouteHigh, FVector(6760, -250, RoofZ), FVector(7200, 0, RoofZ), TEXT("azotea"));
	AddBot(BotRouteHigh, GoalBox.Min * FVector(1, 1, 0) + FVector(0, 0, RoofZ), GoalBox.Max * FVector(1, 1, 0) + FVector(0, 0, RoofZ), TEXT("ojo azul"));

	MenuCameraFrom = FTransform(FRotator(-9.f, -28.f, 0.f), FVector(2200, 1500, 480));
	MenuCameraTo = FTransform(FRotator(-12.f, -12.f, 0.f), FVector(4300, 1300, 620));
	ShotViews = {
		FTransform(FRotator(-8.f, 0.f, 0.f), FVector(-300, 0, 170)),
		FTransform(FRotator(-15.f, -20.f, 0.f), FVector(2100, 600, 400)),
		FTransform(FRotator(-12.f, -10.f, 0.f), FVector(4500, 300, 300)),
		FTransform(FRotator(-25.f, -45.f, 0.f), FVector(6000, 1200, 900)),
		FTransform(FRotator(-55.f, 0.f, 0.f), FVector(7300, -600, 900)),
	};
}

// ---------------------------------------------------------------------------------------- 02 Galeria de toldos
void APogoCircuit::BuildGaleria()
{
	KillZ = -300.f;
	HighLineCount = 2;
	FRandomStream R(202);
	AddSky(FVector(2500.f, 0.f, 0.f));
	AddSkyline(FVector(2500.f, 0.f, 0.f), 9000.f, 24, 1500.f, 4000.f);

	// canal (fuera del recorrido) y vereda de servicio a la derecha
	AddFail(FVector(-1500, -250, -400), FVector(5000, 250, -110), TEXT("Water"), 300.f);
	AddBox(FVector(-1500, -260, -400), FVector(5000, -250, 0), TEXT("Concrete"), 200.f);
	AddBox(FVector(-600, 120, -400), FVector(5000, 250, SW), TEXT("Paving"), 200.f); // vereda de servicio
	AddBox(FVector(-600, 110, SW - 10), FVector(5000, 125, SW + 1), TEXT("Curb"), 100.f);

	// muelle de salida
	AddSupport(FVector(-700, -250, -400), FVector(400, 250, 100), TEXT("Wood"), 150.f);
	AddStart(FVector(-350, 0, 100), 0.f);

	// fachadas a ambos lados
	for (int32 Side = 0; Side < 2; ++Side)
	{
		float x = -1500.f;
		while (x < 5000.f)
		{
			const float W = R.FRandRange(600.f, 1000.f);
			const float H = R.FRandRange(900.f, 1600.f);
			if (Side == 0)
			{
				AddBuilding(FVector(x, -1300, -400), FVector(FMath::Min(x + W, 5000.f), -250, H), Facades[R.RandRange(0, 3)], 1, Shops[R.RandRange(0, 2)]);
			}
			else
			{
				AddBuilding(FVector(x, 250, -400), FVector(FMath::Min(x + W, 5000.f), 1300, H), Facades[R.RandRange(0, 3)], 2, Shops[R.RandRange(0, 2)]);
			}
			x += W;
		}
	}

	// toldos alternados: L = pared izquierda (y=-250, mira a +Y), R = pared derecha (y=250, mira a -Y)
	struct FA { float X0, X1; bool bLeft; float Z; float D; const TCHAR* Fab; int32 HL; };
	const FA Aw[] = {
		{500, 800, true, 190, 150, TEXT("AwningGreen"), 0},
		{850, 1100, false, 220, 150, TEXT("AwningBlue"), 0},
		{1150, 1300, true, 330, 100, TEXT("AwningRed"), 1},
		{1350, 1600, true, 240, 150, TEXT("AwningGreen"), 0},
		{1700, 1950, false, 260, 150, TEXT("AwningBlue"), 0},
		{2050, 2300, true, 230, 150, TEXT("AwningGreen"), 0},
		{2800, 3050, false, 270, 150, TEXT("AwningBlue"), 0},
		{3100, 3250, true, 380, 100, TEXT("AwningRed"), 2},
		{3300, 3550, true, 300, 150, TEXT("AwningGreen"), 0},
		{3650, 3900, false, 320, 150, TEXT("AwningBlue"), 0},
		{4400, 4650, true, 340, 150, TEXT("AwningGreen"), 0},
		{4700, 4900, false, 360, 150, TEXT("AwningBlue"), 0},
	};
	for (const FA& A : Aw)
	{
		AddAwning(A.X0, A.X1, A.bLeft ? -250.f : 250.f, A.bLeft, A.Z, A.D, A.Fab, A.HL);
	}
	// pasarelas sobre el canal (puntos de reaparicion)
	AddSupport(FVector(2400, -250, 170), FVector(2700, 250, 210), TEXT("Wood"), 150.f);
	AddSupport(FVector(4000, -250, 260), FVector(4300, 250, 300), TEXT("Wood"), 150.f);
	AddCheckpoint(FVector(2550, 0, 210), 0.f, FBox(FVector(2400, -250, 150), FVector(2700, 250, 600)));
	AddCheckpoint(FVector(4150, 0, 300), 0.f, FBox(FVector(4000, -250, 250), FVector(4300, 250, 700)));
	// apoyos de recuperacion sobre la vereda de servicio
	AddSupport(FVector(1500, 150, SW), FVector(1600, 240, 110), TEXT("Crate"), 100.f);
	AddSupport(FVector(2200, 150, SW), FVector(2300, 240, 110), TEXT("Crate"), 100.f);
	AddProp(TEXT("SM_Vending"), FVector(3490, 205, SW), -90.f);
	AddProp(TEXT("SM_Vending"), FVector(4540, 205, SW), -90.f);
	AddSupport(FVector(-100, 150, SW), FVector(0, 240, 60), TEXT("Crate"), 100.f);

	// terraza final con el ojo
	const float TZ = 400.f;
	AddBuilding(FVector(5000, -1300, -400), FVector(6700, 1300, TZ), TEXT("FacadeTile"), 4, TEXT("ShopB"), TEXT("RoofMosaic"), false);
	AddRailingLine(FVector(6690, -1290, TZ), FVector(6690, 1290, TZ));
	AddRailingLine(FVector(5000, -1290, TZ), FVector(6690, -1290, TZ));
	AddRailingLine(FVector(6690, 1290, TZ), FVector(5000, 1290, TZ));
	AddProp(TEXT("SM_WaterTank"), FVector(6400, 900, TZ), 0.f);
	AddGoal(FVector(5800, 0, TZ), 640.f, 400.f, 0.f);

	// guirnaldas de farolitos cruzando la galeria
	for (float x = 600.f; x < 4900.f; x += 700.f)
	{
		AddWire(FVector(x, -250, 560), FVector(x + 150, 250, 560), 70.f);
		for (float t : {0.3f, 0.5f, 0.7f})
		{
			AddProp(TEXT("SM_Lantern"), FVector(x + 150 * t, -250 + 500 * t, 560 - 70 * 4 * t * (1 - t) - 62), 0.f, FVector(0.8f), false);
		}
	}
	for (float x = 200.f; x < 4800.f; x += 900.f)
	{
		AddProp(TEXT("SM_SignVertical"), FVector(x, -195, 520), 180.f, FVector::OneVector, false, TEXT("SignRed"), Signs[R.RandRange(0, 2)]);
		AddProp(TEXT("SM_SignVertical"), FVector(x + 450, 195, 540), 0.f, FVector::OneVector, false, TEXT("SignRed"), Signs[R.RandRange(0, 2)]);
	}

	// bot
	struct FB { FVector Min, Max; const TCHAR* L; };
	const FB Main[] = {
		{FVector(-300, -150, 100), FVector(300, 150, 100), TEXT("muelle")},
		{FVector(500, -250, 190), FVector(800, -100, 190), TEXT("toldo L1")},
		{FVector(850, 100, 220), FVector(1100, 250, 220), TEXT("toldo R1")},
		{FVector(1350, -250, 240), FVector(1600, -100, 240), TEXT("toldo L2")},
		{FVector(1700, 100, 260), FVector(1950, 250, 260), TEXT("toldo R2")},
		{FVector(2050, -250, 230), FVector(2300, -100, 230), TEXT("toldo L3")},
		{FVector(2400, -250, 210), FVector(2700, 250, 210), TEXT("pasarela A")},
		{FVector(2800, 100, 270), FVector(3050, 250, 270), TEXT("toldo R3")},
		{FVector(3300, -250, 300), FVector(3550, -100, 300), TEXT("toldo L4")},
		{FVector(3650, 100, 320), FVector(3900, 250, 320), TEXT("toldo R4")},
		{FVector(4000, -250, 300), FVector(4300, 250, 300), TEXT("pasarela B")},
		{FVector(4400, -250, 340), FVector(4650, -100, 340), TEXT("toldo L5")},
		{FVector(4700, 100, 360), FVector(4900, 250, 360), TEXT("toldo R5")},
		{FVector(5050, -300, TZ), FVector(5400, 300, TZ), TEXT("terraza")},
	};
	for (const FB& B : Main)
	{
		AddBot(BotRoute, B.Min, B.Max, B.L);
	}
	for (int32 i = 0; i < UE_ARRAY_COUNT(Main); ++i)
	{
		AddBot(BotRouteHigh, Main[i].Min, Main[i].Max, Main[i].L);
		if (i == 2)
		{
			AddBot(BotRouteHigh, FVector(1150, -250, 330), FVector(1300, -150, 330), TEXT("toldo alto 1"));
		}
		if (i == 7)
		{
			AddBot(BotRouteHigh, FVector(3100, -250, 380), FVector(3250, -150, 380), TEXT("toldo alto 2"));
		}
	}
	for (TArray<FPogoBotTarget>* Route : {&BotRoute, &BotRouteHigh})
	{
		AddBot(*Route, GoalBox.Min * FVector(1, 1, 0) + FVector(0, 0, TZ), GoalBox.Max * FVector(1, 1, 0) + FVector(0, 0, TZ), TEXT("ojo azul"));
	}

	MenuCameraFrom = FTransform(FRotator(-10.f, 10.f, 0.f), FVector(-600, -100, 500));
	MenuCameraTo = FTransform(FRotator(-6.f, -5.f, 0.f), FVector(1500, 50, 450));
	ShotViews = {
		FTransform(FRotator(-8.f, 0.f, 0.f), FVector(-600, 0, 250)),
		FTransform(FRotator(-12.f, 0.f, 0.f), FVector(1200, 0, 520)),
		FTransform(FRotator(-15.f, 0.f, 0.f), FVector(3000, 0, 600)),
		FTransform(FRotator(-45.f, 0.f, 0.f), FVector(5000, 0, 1000)),
	};
}

// ---------------------------------------------------------------------------------------- 03 Azoteas en zigzag
void APogoCircuit::BuildZigzag()
{
	KillZ = 650.f;
	HighLineCount = 2;
	FRandomStream R(303);
	AddSky(FVector(2500.f, 1800.f, 0.f));
	AddSkyline(FVector(2500.f, 1800.f, 0.f), 10000.f, 26, 1200.f, 3600.f);

	// suelo de calles (muy abajo)
	AddFail(FVector(-3000, -3000, -40), FVector(9000, 7000, 0), TEXT("Asphalt"), 400.f);

	// techos jugables (sin parapeto): R0, R1, W1, R2, R3
	struct FRf { FVector Min, Max; const TCHAR* Facade; const TCHAR* Roof; };
	const FRf Roofs[] = {
		{FVector(-700, -500, -40), FVector(500, 500, 900), TEXT("FacadeGray"), TEXT("Concrete")},
		{FVector(1850, -700, -40), FVector(2900, 100, 1000), TEXT("FacadeBeige"), TEXT("RoofMosaic")},
		{FVector(2300, 350, -40), FVector(3400, 1700, 1050), TEXT("FacadeBrown"), TEXT("Concrete")},
		{FVector(2800, 2850, -40), FVector(3600, 3700, 1100), TEXT("FacadeTile"), TEXT("RoofMosaic")},
		{FVector(4750, 3000, -40), FVector(6200, 4400, 1150), TEXT("FacadeBeige"), TEXT("RoofMosaic")},
	};
	for (const FRf& B : Roofs)
	{
		AddBuilding(B.Min, B.Max, B.Facade, 0, TEXT(""), B.Roof, false);
	}
	AddStart(FVector(-350, 0, 900), 0.f);
	AddRailingLine(FVector(-690, -490, 900), FVector(-690, 490, 900));
	AddRailingLine(FVector(-690, 490, 900), FVector(400, 490, 900));
	AddRailingLine(FVector(400, -490, 900), FVector(-690, -490, 900));

	// torres altas con cornisas
	AddBuilding(FVector(750, -320, -40), FVector(1600, 600, 1500), TEXT("FacadeTile"), 0, TEXT(""));
	AddSupport(FVector(750, -420, 900), FVector(1600, -320, 950), TEXT("Concrete"), 150.f); // cornisa E1
	AddBuilding(FVector(3600, 1800, -40), FVector(4400, 2800, 1600), TEXT("FacadeGray"), 0, TEXT(""));
	AddSupport(FVector(3500, 1900, 1000), FVector(3600, 2600, 1050), TEXT("Concrete"), 150.f); // cornisa E2
	AddBuilding(FVector(3800, 3850, -40), FVector(4500, 4600, 1700), TEXT("FacadeBrown"), 0, TEXT(""));
	AddSupport(FVector(3800, 3750, 1050), FVector(4500, 3850, 1100), TEXT("Concrete"), 150.f); // cornisa E3
	// el apoyo cercano que deja mal orientado el salto siguiente
	AddSupport(FVector(3050, 150, 940), FVector(3250, 330, 1000), TEXT("MetalGray"), 100.f);
	AddBox(FVector(3080, 180, -40), FVector(3220, 300, 940), TEXT("Concrete"), 200.f);
	// chimeneas de la linea alta
	AddSupport(FVector(1700, -300, -40), FVector(1800, -200, 1150), TEXT("PlasterGray"), 150.f, White, 1);
	AddSupport(FVector(3650, 3300, -40), FVector(3750, 3400, 1250), TEXT("PlasterGray"), 150.f, White, 2);

	// relleno urbano bajo (lejos del recorrido)
	for (int32 i = 0; i < 26; ++i)
	{
		const FVector C(R.FRandRange(-2500.f, 8000.f), R.FRandRange(-2500.f, 6500.f), 0.f);
		bool bNear = false;
		for (const FRf& B : Roofs)
		{
			if (FBox(B.Min - FVector(500), B.Max + FVector(500)).IsInsideXY(C))
			{
				bNear = true;
			}
		}
		if (bNear || FBox(FVector(700, -500, 0), FVector(4600, 4700, 0)).IsInsideXY(C))
		{
			continue;
		}
		const FVector Half(R.FRandRange(400.f, 800.f), R.FRandRange(400.f, 800.f), 0.f);
		AddBuilding(C - Half - FVector(0, 0, 40), C + Half + FVector(0, 0, R.FRandRange(400.f, 800.f)), Facades[R.RandRange(0, 3)], 0, TEXT(""));
	}
	// objetos de techo
	AddProp(TEXT("SM_WaterTank"), FVector(2700, -550, 1000), 0.f);
	AddProp(TEXT("SM_RoofVent"), FVector(2000, -600, 1000), 0.f);
	AddProp(TEXT("SM_ACUnit"), FVector(3300, 1600, 1050), 0.f);
	AddProp(TEXT("SM_RoofVent"), FVector(2900, 3600, 1100), 0.f);
	AddProp(TEXT("SM_WaterTank"), FVector(6000, 4200, 1150), 0.f);
	AddProp(TEXT("SM_UtilityPole"), FVector(1750, 300, 0), 0.f, FVector(1.4f), false);
	AddWire(FVector(1750, 300, 1176), FVector(2350, 1200, 1050 + 700), 120.f);

	AddCheckpoint(FVector(2150, -300, 1000), 0.f, FBox(FVector(1850, -700, 950), FVector(2900, 100, 1400)));
	AddCheckpoint(FVector(3200, 3200, 1100), 0.f, FBox(FVector(2800, 2850, 1050), FVector(3600, 3700, 1500)));
	AddRailingLine(FVector(6190, 3010, 1150), FVector(6190, 4390, 1150));
	AddRailingLine(FVector(6190, 4390, 1150), FVector(4760, 4390, 1150));
	AddGoal(FVector(5600, 3700, 1150), 640.f, 400.f, 0.f);

	struct FB { FVector Min, Max; const TCHAR* L; };
	const FB Main[] = {
		{FVector(750, -420, 950), FVector(1600, -320, 950), TEXT("cornisa 1")},
		{FVector(1950, -650, 1000), FVector(2850, 50, 1000), TEXT("techo 1")},
		{FVector(2350, 400, 1050), FVector(3350, 1650, 1050), TEXT("techo ancho")},
		{FVector(3500, 1900, 1050), FVector(3600, 2600, 1050), TEXT("cornisa 2")},
		{FVector(2850, 2900, 1100), FVector(3550, 3650, 1100), TEXT("techo 2")},
		{FVector(3800, 3750, 1100), FVector(4500, 3850, 1100), TEXT("cornisa 3")},
		{FVector(4800, 3400, 1150), FVector(5300, 4000, 1150), TEXT("techo final")},
	};
	for (int32 i = 0; i < UE_ARRAY_COUNT(Main); ++i)
	{
		AddBot(BotRoute, Main[i].Min, Main[i].Max, Main[i].L);
		if (i == 1)
		{
			// la linea alta pasa por la chimenea entre la cornisa 1 y el techo 1
			BotRouteHigh.Insert(FPogoBotTarget{FBox(FVector(1700, -300, 1150), FVector(1800, -200, 1150)), false, TEXT("chimenea 1")}, BotRouteHigh.Num());
		}
		if (i == 5)
		{
			BotRouteHigh.Insert(FPogoBotTarget{FBox(FVector(3650, 3300, 1250), FVector(3750, 3400, 1250)), false, TEXT("chimenea 2")}, BotRouteHigh.Num());
		}
		AddBot(BotRouteHigh, Main[i].Min, Main[i].Max, Main[i].L);
	}
	for (TArray<FPogoBotTarget>* Route : {&BotRoute, &BotRouteHigh})
	{
		AddBot(*Route, GoalBox.Min * FVector(1, 1, 0) + FVector(0, 0, 1150), GoalBox.Max * FVector(1, 1, 0) + FVector(0, 0, 1150), TEXT("ojo azul"));
	}

	MenuCameraFrom = FTransform(FRotator(-18.f, 30.f, 0.f), FVector(-600, -900, 1400));
	MenuCameraTo = FTransform(FRotator(-20.f, 60.f, 0.f), FVector(1500, -800, 1500));
	ShotViews = {
		FTransform(FRotator(-10.f, 0.f, 0.f), FVector(-600, 0, 1100)),
		FTransform(FRotator(-30.f, 45.f, 0.f), FVector(1000, -800, 1700)),
		FTransform(FRotator(-35.f, 60.f, 0.f), FVector(2500, 1500, 2000)),
		FTransform(FRotator(-35.f, 20.f, 0.f), FVector(3500, 2800, 1800)),
	};
}

// ---------------------------------------------------------------------------------------- patio gris (prototipo 01)
void APogoCircuit::BuildYard()
{
	KillZ = -400.f;
	HighLineCount = 0;
	AddSky(FVector::ZeroVector);
	const FLinearColor Gray(0.55f, 0.55f, 0.57f);
	AddBox(FVector(-2000, -2000, -40), FVector(3000, 2000, 0), TEXT("Concrete"), 200.f, Gray);
	AddStart(FVector(0, 0, 0), 0.f);
	AddSupport(FVector(300, -150, 0), FVector(550, 150, 85), TEXT("Concrete"), 150.f, Gray);
	AddSupport(FVector(800, -150, 0), FVector(1050, 150, 175), TEXT("Concrete"), 150.f, Gray);
	AddSupport(FVector(1300, -150, 0), FVector(1600, 150, 260), TEXT("Concrete"), 150.f, Gray);
	AddGoal(FVector(1450, 0, 260), 280.f, 280.f, 0.f);
	AddBot(BotRoute, FVector(300, -150, 85), FVector(550, 150, 85), TEXT("plataforma 1"));
	AddBot(BotRoute, FVector(800, -150, 175), FVector(1050, 150, 175), TEXT("plataforma 2"));
	AddBot(BotRoute, FVector(1300, -150, 260), FVector(1600, 150, 260), TEXT("plataforma 3"));
	BotRouteHigh = BotRoute;
	MenuCameraFrom = FTransform(FRotator(-20.f, 20.f, 0.f), FVector(-800, -600, 500));
	MenuCameraTo = MenuCameraFrom;
	ShotViews = {FTransform(FRotator(-20.f, 10.f, 0.f), FVector(-700, -400, 450))};
}

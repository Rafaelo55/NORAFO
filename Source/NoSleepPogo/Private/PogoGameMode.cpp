#include "PogoGameMode.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/SkyLight.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NoSleepPogo.h"
#include "PogoArt.h"
#include "PogoAutoTest.h"
#include "PogoCharacter.h"
#include "PogoCircuit.h"
#include "PogoGameInstance.h"
#include "PogoPlayerController.h"
#include "TimerManager.h"

APogoGameMode::APogoGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	DefaultPawnClass = APogoCharacter::StaticClass();
	PlayerControllerClass = APogoPlayerController::StaticClass();
}

void APogoGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	const FString CircuitOpt = UGameplayStatics::ParseOption(Options, TEXT("Circuit"));
	FParse::Value(FCommandLine::Get(), TEXT("PogoTest="), TestMode);
	if (!CircuitOpt.IsEmpty())
	{
		CircuitIndex = FCString::Atoi(*CircuitOpt);
		bMenuMode = false;
	}
	else
	{
		CircuitIndex = -1;
		bMenuMode = true;
		FString ForceCircuit;
		if (FParse::Value(FCommandLine::Get(), TEXT("PogoCircuit="), ForceCircuit))
		{
			CircuitIndex = FCString::Atoi(*ForceCircuit);
			bMenuMode = false;
		}
	}

	SpawnEnvironment();

	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Circuit = GetWorld()->SpawnActor<APogoCircuit>(APogoCircuit::StaticClass(), FTransform::Identity, SP);
	Circuit->Build(bMenuMode ? 0 : CircuitIndex);

	if (bMenuMode)
	{
		MenuCamera = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), Circuit->MenuCameraFrom, SP);
		MenuCamera->GetCameraComponent()->SetFieldOfView(80.f);
		MenuCamera->GetCameraComponent()->bConstrainAspectRatio = false;
	}
	UE_LOG(LogPogo, Log, TEXT("InitGame circuito=%d menu=%d test=%s"), CircuitIndex, bMenuMode ? 1 : 0, *TestMode);
}

void APogoGameMode::SpawnEnvironment()
{
	UWorld* W = GetWorld();
	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// Sol: sombras marcadas
	ADirectionalLight* Sun = W->SpawnActor<ADirectionalLight>(ADirectionalLight::StaticClass(), FTransform(FRotator(-52.f, 35.f, 0.f)), SP);
	if (UDirectionalLightComponent* L = Cast<UDirectionalLightComponent>(Sun->GetLightComponent()))
	{
		L->SetMobility(EComponentMobility::Movable);
		L->SetIntensity(4.2f);
		L->SetLightColor(FLinearColor(1.f, 0.93f, 0.82f));
		L->SetDynamicShadowDistanceMovableLight(9000.f);
		L->SetDynamicShadowCascades(2);
		L->SetCastShadows(true);
	}

	SkyLight = W->SpawnActor<ASkyLight>(ASkyLight::StaticClass(), FTransform::Identity, SP);
	if (USkyLightComponent* S = SkyLight->GetLightComponent())
	{
		S->SetMobility(EComponentMobility::Movable);
		S->SourceType = ESkyLightSourceType::SLS_CapturedScene;
		S->SetIntensity(1.1f);
		S->SetLightColor(FLinearColor(0.85f, 0.9f, 1.f));
		S->SetLowerHemisphereColor(FLinearColor(0.25f, 0.24f, 0.22f));
		S->bLowerHemisphereIsBlack = false;
		S->bRealTimeCapture = false;
		S->SkyDistanceThreshold = 8000.f; // el domo del cielo (700 m) cuenta como cielo
	}

	// luz de relleno sin sombras: sombras legibles pero no negras (iluminacion plana estilo PS1)
	ADirectionalLight* Fill = W->SpawnActor<ADirectionalLight>(ADirectionalLight::StaticClass(), FTransform(FRotator(-25.f, -150.f, 0.f)), SP);
	if (UDirectionalLightComponent* L = Cast<UDirectionalLightComponent>(Fill->GetLightComponent()))
	{
		L->SetMobility(EComponentMobility::Movable);
		L->SetIntensity(0.9f);
		L->SetLightColor(FLinearColor(0.62f, 0.7f, 0.9f));
		L->SetCastShadows(false);
		L->SetAtmosphereSunLightIndex(1);
	}

	AExponentialHeightFog* Fog = W->SpawnActor<AExponentialHeightFog>(AExponentialHeightFog::StaticClass(), FTransform(FVector(0.f, 0.f, -200.f)), SP);
	if (UExponentialHeightFogComponent* F = Fog->GetComponent())
	{
		F->SetFogDensity(0.035f);
		F->SetFogHeightFalloff(0.08f);
		F->SetFogInscatteringColor(FLinearColor(0.55f, 0.62f, 0.74f));
		F->SetStartDistance(1800.f);
		F->SetFogMaxOpacity(0.92f);
		F->SetFogCutoffDistance(45000.f);
	}

	APostProcessVolume* PPV = W->SpawnActor<APostProcessVolume>(APostProcessVolume::StaticClass(), FTransform::Identity, SP);
	PPV->bUnbound = true;
	PPV->Priority = 10.f;
	FPostProcessSettings& S = PPV->Settings;
	S.bOverride_AutoExposureMethod = true;
	S.AutoExposureMethod = AEM_Manual;
	S.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
	S.AutoExposureApplyPhysicalCameraExposure = false;
	S.bOverride_AutoExposureBias = true;
	S.AutoExposureBias = 0.f;
	S.bOverride_BloomIntensity = true;
	S.BloomIntensity = 0.f;
	S.bOverride_VignetteIntensity = true;
	S.VignetteIntensity = 0.35f;
	S.bOverride_AmbientOcclusionIntensity = true;
	S.AmbientOcclusionIntensity = 0.f;
	S.bOverride_MotionBlurAmount = true;
	S.MotionBlurAmount = 0.f;
	S.bOverride_ColorSaturation = true;
	S.ColorSaturation = FVector4(1.12f, 1.12f, 1.12f, 1.f);
	S.bOverride_SceneFringeIntensity = true;
	S.SceneFringeIntensity = 0.f;
	if (UMaterialInterface* PP = PogoArt::Master(TEXT("PP_PS1")))
	{
		S.WeightedBlendables.Array.Add(FWeightedBlendable(1.f, PP));
	}
}

AActor* APogoGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	if (Circuit && Circuit->PlayerStart)
	{
		return Circuit->PlayerStart;
	}
	return Super::ChoosePlayerStart_Implementation(Player);
}

void APogoGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	if (bMenuMode)
	{
		if (MenuCamera)
		{
			NewPlayer->SetViewTarget(MenuCamera);
		}
		return;
	}
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);
}

void APogoGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (UPogoGameInstance* GI = UPogoGameInstance::Get(this))
	{
		GI->ApplySettings();
	}

	if (USoundBase* M = PogoArt::Sound(TEXT("MUS_Loop")))
	{
		Music = UGameplayStatics::CreateSound2D(this, M, 1.f, 1.f, 0.f, nullptr, false, false);
		if (Music)
		{
			Music->bIsUISound = true;
			Music->Play();
		}
	}
	if (USoundBase* A = PogoArt::Sound(TEXT("AMB_City")))
	{
		Ambience = UGameplayStatics::CreateSound2D(this, A, 1.f, 1.f, 0.f, nullptr, false, false);
		if (Ambience)
		{
			Ambience->bIsUISound = true;
			Ambience->Play();
		}
	}
	ApplyAudioVolumes();

	if (SkyLight)
	{
		FTimerHandle H;
		GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			if (SkyLight && SkyLight->GetLightComponent())
			{
				SkyLight->GetLightComponent()->RecaptureSky();
			}
		}), 0.2f, false);
	}

	if (!TestMode.IsEmpty())
	{
		FActorSpawnParameters SP;
		APogoAutoTest* T = GetWorld()->SpawnActor<APogoAutoTest>(APogoAutoTest::StaticClass(), FTransform::Identity, SP);
		T->Mode = TestMode;
	}
}

void APogoGameMode::ApplyAudioVolumes()
{
	const UPogoGameInstance* GI = UPogoGameInstance::Get(this);
	const float Mus = GI ? GI->Settings.MusicVolume : 0.5f;
	const float Sfx = GI ? GI->Settings.SfxVolume : 1.f;
	if (Music)
	{
		Music->SetVolumeMultiplier(FMath::Max(0.001f, Mus * 0.6f));
	}
	if (Ambience)
	{
		Ambience->SetVolumeMultiplier(FMath::Max(0.001f, Sfx * 0.5f));
	}
}

void APogoGameMode::Tick(float Dt)
{
	Super::Tick(Dt);
	FailCooldown = FMath::Max(0.f, FailCooldown - Dt);

	if (bMenuMode)
	{
		UpdateMenuCamera(Dt);
		return;
	}
	if (bTimerStarted && !bFinished)
	{
		RunTime += Dt;
	}

	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	APogoCharacter* Pogo = PC ? Cast<APogoCharacter>(PC->GetPawn()) : nullptr;
	if (Pogo && Circuit && !bFinished && Pogo->GetActorLocation().Z < Circuit->KillZ)
	{
		PlayerFailed(Pogo, EPogoFailReason::Fell);
	}
}

void APogoGameMode::UpdateMenuCamera(float Dt)
{
	if (!MenuCamera || !Circuit)
	{
		return;
	}
	MenuTime += Dt;
	const float A = 0.5f + 0.5f * FMath::Sin(MenuTime * 0.08f);
	const FVector Loc = FMath::Lerp(Circuit->MenuCameraFrom.GetLocation(), Circuit->MenuCameraTo.GetLocation(), A);
	const FRotator Rot = FMath::Lerp(Circuit->MenuCameraFrom.Rotator(), Circuit->MenuCameraTo.Rotator(), A);
	MenuCamera->SetActorLocationAndRotation(Loc, Rot);
}

int32 APogoGameMode::GetHighLineCount() const
{
	return Circuit ? Circuit->HighLineCount : 0;
}

FText APogoGameMode::GetObjectiveText() const
{
	return FText::Format(FText::FromString(TEXT("META  /  {0}")), PogoCircuits::Get(CircuitIndex).GoalName);
}

// ------------------------------------------------------------------------------------- eventos
void APogoGameMode::NotifyTakeoff(APogoCharacter* Pogo, float Alpha)
{
	if (!bTimerStarted && !bFinished)
	{
		bTimerStarted = true; // GDD: el reloj arranca al despegar por primera vez
		RunTime = 0.f;
	}
}

void APogoGameMode::NotifyLanded(APogoCharacter* Pogo, const FHitResult& Hit)
{
	if (!Circuit || bFinished)
	{
		return;
	}
	const FVector Tip = Pogo->GetTipLocation();
	APogoPlayerController* PC = Cast<APogoPlayerController>(Pogo->GetController());

	// meta: caer dentro del ojo azul
	if (Circuit->GoalBox.IsInsideOrOn(Tip))
	{
		Finish(Pogo);
		return;
	}

	// puntos de reaparicion
	for (int32 i = 0; i < Circuit->Checkpoints.Num(); ++i)
	{
		if (i > CheckpointIndex && Circuit->Checkpoints[i].Zone.IsInsideOrOn(Tip))
		{
			CheckpointIndex = i;
			Circuit->SetCheckpointActive(i);
			PogoArt::PlaySfx(this, TEXT("SFX_Checkpoint"), 0.7f);
			if (PC)
			{
				PC->ShowToast(FText::FromString(TEXT("PUNTO DE REAPARICIÓN")), PogoColors::BlueLight);
			}
		}
	}

	// linea alta: tocar los toldos marcados en orden durante la misma pasada
	const UPrimitiveComponent* C = Hit.GetComponent();
	if (C && Circuit->HighLineCount > 0 && !bHighLineDone)
	{
		for (int32 n = 1; n <= Circuit->HighLineCount; ++n)
		{
			if (C->ComponentHasTag(APogoCircuit::HighLineTag(n)))
			{
				if (n == HighLineNext)
				{
					HighLineNext++;
					PogoArt::PlaySfx(this, TEXT("SFX_Awning"), 0.8f, 0.9f + 0.1f * n);
					if (HighLineNext > Circuit->HighLineCount)
					{
						bHighLineDone = true;
						if (PC)
						{
							PC->ShowToast(FText::FromString(TEXT("LÍNEA ALTA COMPLETA")), PogoColors::Gold, 2.6f);
						}
					}
					else if (PC)
					{
						PC->ShowToast(FText::Format(FText::FromString(TEXT("LÍNEA ALTA  {0}/{1}")), FText::AsNumber(n), FText::AsNumber(Circuit->HighLineCount)), PogoColors::Gold);
					}
				}
				break;
			}
		}
	}
}

void APogoGameMode::PlayerFailed(APogoCharacter* Pogo, EPogoFailReason Reason)
{
	if (bFinished || FailCooldown > 0.f || !Circuit || !Pogo)
	{
		return;
	}
	FailCooldown = 0.6f;
	Falls++;
	// GDD: al reaparecer se reinicia la secuencia de toldos del tramo (el reloj sigue)
	if (!bHighLineDone)
	{
		HighLineNext = 1;
	}
	if (Reason == EPogoFailReason::Car)
	{
		PogoArt::PlaySfx(this, TEXT("SFX_Crash"), 0.9f);
		PogoArt::PlaySfx(this, TEXT("SFX_CarHorn"), 0.6f);
	}
	PogoArt::PlaySfx(this, TEXT("SFX_Respawn"), 0.6f);

	const FTransform Spawn = CheckpointIndex >= 0 ? Circuit->Checkpoints[CheckpointIndex].Spawn : Circuit->StartTransform;
	Pogo->ResetToSpawn(Spawn);

	if (APogoPlayerController* PC = Cast<APogoPlayerController>(Pogo->GetController()))
	{
		if (PC->PlayerCameraManager)
		{
			PC->PlayerCameraManager->StartCameraFade(1.f, 0.f, 0.45f, FLinearColor(0.02f, 0.03f, 0.08f), false, false);
		}
		const TCHAR* Msg = Reason == EPogoFailReason::Car ? TEXT("¡EL AUTO!  VOLVÉS AL ÚLTIMO PUNTO")
			: (Reason == EPogoFailReason::Manual ? TEXT("VOLVÉS AL ÚLTIMO PUNTO") : TEXT("FUERA DEL RECORRIDO"));
		PC->ShowToast(FText::FromString(Msg), FLinearColor(0.9f, 0.35f, 0.3f));
	}
	UE_LOG(LogPogo, Log, TEXT("Caida %d (razon %d) -> reaparece en punto %d"), Falls, (int32)Reason, CheckpointIndex);
}

void APogoGameMode::Finish(APogoCharacter* Pogo)
{
	if (bFinished)
	{
		return;
	}
	bFinished = true; // el reloj se detiene una sola vez
	if (!bTimerStarted)
	{
		RunTime = 0.f;
	}
	Pogo->StartFinish();
	PogoArt::PlaySfx(this, TEXT("SFX_Goal"), 0.9f);
	Circuit->SetGoalReached();

	FPogoRunResult Result;
	UPogoGameInstance* GI = UPogoGameInstance::Get(this);
	if (GI && TestMode.IsEmpty())
	{
		Result = GI->RecordRun(CircuitIndex, RunTime, Falls, bHighLineDone);
	}
	else
	{
		// las pruebas automaticas no tocan el progreso guardado del jugador
		Result.Time = RunTime;
		Result.Best = RunTime;
		Result.Falls = Falls;
		Result.bHighLine = bHighLineDone;
		Result.bCleanRun = Falls == 0;
	}
	Result.Circuit = CircuitIndex;
	UE_LOG(LogPogo, Log, TEXT("META circuito=%d tiempo=%.2f caidas=%d lineaAlta=%d record=%d"), CircuitIndex, RunTime, Falls, bHighLineDone ? 1 : 0, Result.bNewRecord ? 1 : 0);

	APogoPlayerController* PC = Cast<APogoPlayerController>(Pogo->GetController());
	if (PC)
	{
		PC->ShowToast(FText::Format(FText::FromString(TEXT("{0} ALCANZADA")), PogoCircuits::Get(CircuitIndex).GoalName), PogoColors::BlueLight, 2.4f);
		FTimerHandle H;
		TWeakObjectPtr<APogoPlayerController> WeakPC(PC);
		GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [WeakPC, Result]()
		{
			if (WeakPC.IsValid())
			{
				WeakPC->ShowResults(Result);
			}
		}), 2.4f, false);
	}
}

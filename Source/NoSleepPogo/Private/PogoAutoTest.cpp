#include "PogoAutoTest.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/FileManager.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/Paths.h"
#include "NoSleepPogo.h"
#include "PogoCar.h"
#include "PogoCharacter.h"
#include "PogoCircuit.h"
#include "PogoGameInstance.h"
#include "PogoGameMode.h"
#include "PogoPlayerController.h"
#include "UnrealClient.h"

APogoAutoTest::APogoAutoTest()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
}

APogoCharacter* APogoAutoTest::Pogo() const
{
	APlayerController* P = GetWorld()->GetFirstPlayerController();
	return P ? Cast<APogoCharacter>(P->GetPawn()) : nullptr;
}

APogoGameMode* APogoAutoTest::GM() const
{
	return GetWorld()->GetAuthGameMode<APogoGameMode>();
}

APogoPlayerController* APogoAutoTest::PC() const
{
	return Cast<APogoPlayerController>(GetWorld()->GetFirstPlayerController());
}

void APogoAutoTest::Shot(const FString& Name, bool bUI)
{
	const FString Dir = FPaths::ProjectSavedDir() / TEXT("PogoTest");
	IFileManager::Get().MakeDirectory(*Dir, true);
	const int32 C = GM() ? GM()->GetCircuitIndex() : -1;
	const FString File = Dir / FString::Printf(TEXT("c%d_%s.png"), C, *Name);
	FScreenshotRequest::RequestScreenshot(File, bUI, false);
	UE_LOG(LogPogo, Log, TEXT("POGO_TEST shot %s"), *File);
}

void APogoAutoTest::Quit(float Delay)
{
	if (bQuitting)
	{
		return;
	}
	bQuitting = true;
	FTimerHandle H;
	GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		UE_LOG(LogPogo, Log, TEXT("POGO_TEST quit"));
		FPlatformMisc::RequestExit(false);
	}), Delay, false);
}

void APogoAutoTest::Tick(float Dt)
{
	Super::Tick(Dt);
	Clock += FApp::GetDeltaTime();
	StepTime += FApp::GetDeltaTime();
	if (bQuitting)
	{
		return;
	}
	if (Clock > 420.f)
	{
		UE_LOG(LogPogo, Error, TEXT("POGO_TEST timeout en modo %s (objetivo %d)"), *Mode, Target);
		Shot(TEXT("timeout"));
		Quit(1.f);
		return;
	}
	if (Mode == TEXT("bot") || Mode == TEXT("bothigh"))
	{
		bHigh = Mode == TEXT("bothigh");
		TickBot(Dt);
	}
	else if (Mode == TEXT("shots"))
	{
		TickShots(Dt);
	}
	else if (Mode == TEXT("menu"))
	{
		if (Clock > 4.f && Step == 0)
		{
			Shot(TEXT("menu"));
			Step = 1;
			Quit(1.5f);
		}
	}
	else if (Mode == TEXT("mech"))
	{
		TickMech(Dt);
	}
}

// =========================================================================================== bot
bool APogoAutoTest::IsInside(const FPogoBotTarget& T, const FVector& Tip, float Tol) const
{
	return Tip.X >= T.Box.Min.X - Tol && Tip.X <= T.Box.Max.X + Tol && Tip.Y >= T.Box.Min.Y - Tol && Tip.Y <= T.Box.Max.Y + Tol
		&& FMath::Abs(Tip.Z - T.Box.Max.Z) < 30.f;
}

static bool SolveAlpha(const APogoCharacter* P, float Dx, float Dz, float& OutAlpha, float& OutScale, float& OutTime, float& OutMaxReach)
{
	const float G = P->GetPogoGravity();
	OutMaxReach = 0.f;
	for (float A = 0.f; A <= 1.0001f; A += 0.01f)
	{
		const float H = P->GetJumpHeight(A);
		if (H < Dz + 35.f)
		{
			continue; // no supera el borde con margen
		}
		const float Vz = FMath::Sqrt(2.f * G * H);
		const float Disc = Vz * Vz - 2.f * G * Dz;
		if (Disc < 0.f)
		{
			continue;
		}
		const float Tland = (Vz + FMath::Sqrt(Disc)) / G;
		const float Full = P->GetForwardSpeed(A) * Tland;
		OutMaxReach = FMath::Max(OutMaxReach, Full);
		const float S = Full > 1.f ? Dx / Full : 0.f;
		if (S <= 0.92f)
		{
			OutAlpha = A;
			OutScale = FMath::Clamp(S, 0.f, 1.f);
			OutTime = Tland;
			return true;
		}
	}
	return false;
}

bool APogoAutoTest::PlanJump(const FPogoBotTarget& T, float& OutAlpha, float& OutScale, FVector& OutAim, float& OutTime) const
{
	APogoCharacter* P = Pogo();
	const FVector Tip = P->GetTipLocation();
	const FVector Ext = T.Box.GetExtent();
	const FVector Margin(FMath::Min(40.f, Ext.X * 0.5f), FMath::Min(40.f, Ext.Y * 0.5f), 0.f);
	FVector Aim(FMath::Clamp(Tip.X, T.Box.Min.X + Margin.X, T.Box.Max.X - Margin.X),
	            FMath::Clamp(Tip.Y, T.Box.Min.Y + Margin.Y, T.Box.Max.Y - Margin.Y), T.Box.Max.Z);
	// apuntar un poco hacia el centro para no caer justo en el borde (como maximo 30 cm)
	const FVector ToCenter = FVector(T.Box.GetCenter().X, T.Box.GetCenter().Y, T.Box.Max.Z) - Aim;
	Aim += ToCenter.GetClampedToMaxSize(30.f);
	OutAim = Aim;
	float Reach = 0.f;
	if (SolveAlpha(P, FVector::Dist2D(Tip, Aim), Aim.Z - Tip.Z, OutAlpha, OutScale, OutTime, Reach))
	{
		return true;
	}
	// fuera de alcance: primero ir al borde del apoyo actual mas cercano al objetivo (como haria un jugador)
	float Reach0 = 0.f;
	float A0, S0, T0;
	SolveAlpha(P, 1e6f, 0.f, A0, S0, T0, Reach0);
	FVector Edge = Tip;
	bool bHasSupport = false;
	if (const UPrimitiveComponent* FC = P->GetCharacterMovement()->CurrentFloor.HitResult.GetComponent())
	{
		const FBox B = FC->Bounds.GetBox();
		const FVector Shrink(FMath::Min(35.f, B.GetExtent().X * 0.5f), FMath::Min(35.f, B.GetExtent().Y * 0.5f), 0.f);
		Edge = FVector(FMath::Clamp(Aim.X, B.Min.X + Shrink.X, B.Max.X - Shrink.X), FMath::Clamp(Aim.Y, B.Min.Y + Shrink.Y, B.Max.Y - Shrink.Y), Tip.Z);
		bHasSupport = true;
	}
	float Hop = FVector::Dist2D(Tip, Edge);
	FVector Dir = (Edge - Tip).GetSafeNormal2D();
	if (!bHasSupport || Hop < 25.f)
	{
		// ya estamos en el mejor borde: salto maximo hacia el objetivo
		Dir = (Aim - Tip).GetSafeNormal2D();
		Hop = FMath::Min(FVector::Dist2D(Tip, Aim), Reach0 * 0.92f);
		OutAim = Tip + Dir * Hop;
		OutAim.Z = Aim.Z;
		if (!SolveAlpha(P, Hop, Aim.Z - Tip.Z, OutAlpha, OutScale, OutTime, Reach0))
		{
			OutAlpha = 1.f;
			OutScale = 1.f;
			OutTime = 1.f;
		}
		return false;
	}
	Hop = FMath::Min(Hop, Reach0 * 0.7f);
	OutAim = Tip + Dir * Hop;
	OutAim.Z = Tip.Z;
	if (!SolveAlpha(P, Hop, 0.f, OutAlpha, OutScale, OutTime, Reach0))
	{
		OutAlpha = 0.5f;
		OutScale = 0.5f;
		OutTime = 0.6f;
	}
	return false;
}

void APogoAutoTest::TickBot(float Dt)
{
	APogoCharacter* P = Pogo();
	APogoGameMode* G = GM();
	if (!P || !G || !G->GetCircuit())
	{
		return;
	}
	if (!P->bBotControlled)
	{
		P->bBotControlled = true;
		const UPogoGameInstance* GI0 = UPogoGameInstance::Get(this);
		UE_LOG(LogPogo, Log, TEXT("POGO_TEST bot inicia circuito=%d ruta=%s intento=%d"), G->GetCircuitIndex(), bHigh ? TEXT("alta") : TEXT("principal"), GI0 ? GI0->BotAttempt + 1 : 1);
	}
	const TArray<FPogoBotTarget>& Route = bHigh ? G->GetCircuit()->BotRouteHigh : G->GetCircuit()->BotRoute;
	const EPogoState S = P->GetPogoState();
	UPogoGameInstance* GI = UPogoGameInstance::Get(this);
	const int32 Attempt = GI ? GI->BotAttempt + 1 : 1;
	constexpr int32 MaxAttempts = 4;
	constexpr float AttemptLimit = 30.f;

	// limite por intento: 30 s sin llegar a la meta -> se reinicia el nivel (hasta 4 intentos)
	if (!G->IsFinished() && G->GetRunTime() > AttemptLimit && Step < 100)
	{
		Step = 100;
		UE_LOG(LogPogo, Warning, TEXT("POGO_TEST intento %d/%d sin meta en %.0f s: atascado en objetivo %d (%s), caidas=%d"),
			Attempt, MaxAttempts, AttemptLimit, Target, Route.IsValidIndex(Target) ? *Route[Target].Label : TEXT("-"), G->GetFalls());
		Shot(FString::Printf(TEXT("%s_atascado_intento%d"), bHigh ? TEXT("bothigh") : TEXT("bot"), Attempt));
		P->SetChargeHeld(false);
		P->SetMoveInput(FVector2D::ZeroVector);
		if (Attempt < MaxAttempts && GI)
		{
			GI->BotAttempt = Attempt;
			FTimerHandle H;
			GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				if (APogoPlayerController* C = PC())
				{
					C->RestartCircuit();
				}
			}), 0.5f, false);
		}
		else
		{
			UE_LOG(LogPogo, Log, TEXT("POGO_TEST RESULT circuito=%d ruta=%s completo=0 intentos=%d objetivo=%d/%d"),
				G->GetCircuitIndex(), bHigh ? TEXT("alta") : TEXT("principal"), Attempt, Target, Route.Num());
			Quit(1.5f);
		}
		return;
	}
	if (Step >= 100 && Step < 200 && !G->IsFinished())
	{
		return;
	}

	if (S == EPogoState::Finished || G->IsFinished())
	{
		if (FinishedTime < 0.f)
		{
			FinishedTime = Clock;
			UE_LOG(LogPogo, Log, TEXT("POGO_TEST RESULT circuito=%d ruta=%s completo=1 intento=%d tiempo=%.2f caidas=%d saltos=%d fallidos=%d lineaAlta=%d"),
				G->GetCircuitIndex(), bHigh ? TEXT("alta") : TEXT("principal"), Attempt, G->GetRunTime(), G->GetFalls(), Jumps, Misses, G->IsHighLineDone() ? 1 : 0);
		}
		if (Clock - FinishedTime > 3.5f && Step < 200)
		{
			Step = 200;
			Shot(bHigh ? TEXT("bothigh_results") : TEXT("bot_results"));
			Quit(1.5f);
		}
		return;
	}

	// detectar caidas
	if (G->GetFalls() != LastFalls)
	{
		LastFalls = G->GetFalls();
		bInFlight = false;
		bJumpPending = false;
		P->SetChargeHeld(false);
		P->SetMoveInput(FVector2D::ZeroVector);
		// retomar desde el objetivo mas cercano al punto de reaparicion
		const FVector Tip = P->GetTipLocation();
		int32 Best = 0;
		float BestD = TNumericLimits<float>::Max();
		for (int32 i = 0; i < Route.Num(); ++i)
		{
			const float D = FVector::Dist(Route[i].Box.GetCenter(), Tip);
			if (D < BestD)
			{
				BestD = D;
				Best = i;
			}
		}
		Target = Best;
		UE_LOG(LogPogo, Warning, TEXT("POGO_TEST caida %d -> retoma en objetivo %d (%s)"), LastFalls, Target, *Route[Target].Label);
		return;
	}

	if (bInFlight)
	{
		if (S == EPogoState::Air)
		{
			return;
		}
		bInFlight = false;
		const FVector Tip = P->GetTipLocation();
		const bool bOk = Route.IsValidIndex(Target) && IsInside(Route[Target], Tip, 5.f);
		UE_LOG(LogPogo, Log, TEXT("POGO_TEST salto %d -> %s: cae en (%.0f,%.0f,%.0f) %s"), Jumps, Route.IsValidIndex(Target) ? *Route[Target].Label : TEXT("?"),
			Tip.X, Tip.Y, Tip.Z, bOk ? TEXT("OK") : TEXT("fuera del objetivo"));
		if (!bOk)
		{
			Misses++;
		}
	}

	if (!Route.IsValidIndex(Target))
	{
		return;
	}

	// avanzar objetivos ya alcanzados
	const FVector Tip = P->GetTipLocation();
	while (Route.IsValidIndex(Target) && IsInside(Route[Target], Tip, 5.f) && (S != EPogoState::Air))
	{
		UE_LOG(LogPogo, Log, TEXT("POGO_TEST objetivo %d alcanzado: %s"), Target, *Route[Target].Label);
		if (Target % 3 == 2)
		{
			Shot(FString::Printf(TEXT("%s_%02d"), bHigh ? TEXT("bothigh") : TEXT("bot"), Target));
		}
		Target++;
	}
	if (!Route.IsValidIndex(Target))
	{
		return;
	}
	const FPogoBotTarget& T = Route[Target];

	if (S == EPogoState::Charging && bJumpPending)
	{
		// soltar al llegar a la carga planificada (corrigiendo la escala para la carga real)
		const float NextAlpha = FMath::Clamp(P->GetChargeAlpha() + Dt / P->ChargeTimeMax, 0.f, 1.f);
		if (NextAlpha >= PendingAlpha - 0.005f || P->GetChargeAlpha() >= 1.f)
		{
			float A, Sc, Tm;
			FVector Aim;
			PlanJump(T, A, Sc, Aim, Tm);
			const float G2 = P->GetPogoGravity();
			const float H = P->GetJumpHeight(NextAlpha);
			const float Vz = FMath::Sqrt(2.f * G2 * H);
			const float Dz = Aim.Z - Tip.Z;
			const float Disc = FMath::Max(0.f, Vz * Vz - 2.f * G2 * Dz);
			const float Tl = (Vz + FMath::Sqrt(Disc)) / G2;
			const float Full = P->GetForwardSpeed(NextAlpha) * Tl;
			const float Scale = Full > 1.f ? FMath::Clamp(FVector::Dist2D(Tip, Aim) / Full, 0.f, 1.f) : 0.f;
			P->SetMoveInput(FVector2D(0.f, Scale));
			P->SetChargeHeld(false);
			bJumpPending = false;
			bInFlight = true;
			Jumps++;
		}
		return;
	}

	if (S == EPogoState::Ready || S == EPogoState::Contact)
	{
		if (!P->GetCharacterMovement()->IsMovingOnGround())
		{
			return;
		}
		float A, Sc, Tm;
		FVector Aim;
		const bool bReach = PlanJump(T, A, Sc, Aim, Tm);

		// esperar al auto desde un lugar seguro
		bool bUnsafeHere = false;
		for (const FPogoBotTarget& R : Route)
		{
			if (R.Label.Contains(TEXT("carril")) && IsInside(R, Tip, 60.f))
			{
				bUnsafeHere = true;
			}
		}
		if (T.bCarCheck && !bUnsafeHere)
		{
			const FVector Lo(FMath::Min(Tip.X, Aim.X) - 60.f, -300.f, -100.f);
			const FVector Hi(FMath::Max(Tip.X, Aim.X) + 60.f, 300.f, 500.f);
			const float Window = Tm + A * P->ChargeTimeMax + (T.Label.Contains(TEXT("carril")) ? 2.6f : 0.6f);
			for (APogoCar* Car : G->GetCircuit()->Cars)
			{
				if (Car && Car->WillOccupy(FBox(Lo, Hi), 0.f, Window))
				{
					WaitTime += Dt;
					P->SetMoveInput(FVector2D::ZeroVector);
					return; // rebota en el lugar hasta que pase el auto
				}
			}
		}
		if (WaitTime > 0.f)
		{
			UE_LOG(LogPogo, Log, TEXT("POGO_TEST espero al auto %.1f s"), WaitTime);
			WaitTime = 0.f;
		}

		const FVector Dir = (Aim - Tip).GetSafeNormal2D();
		if (AController* C = P->GetController())
		{
			C->SetControlRotation(FRotator(-12.f, Dir.Rotation().Yaw, 0.f));
		}
		UE_LOG(LogPogo, Log, TEXT("POGO_TEST plan %d -> %s carga=%.2f escala=%.2f dist=%.0f dz=%.0f %s"), Target, *T.Label, A, Sc,
			FVector::Dist2D(Tip, Aim), Aim.Z - Tip.Z, bReach ? TEXT("") : TEXT("(fuera de alcance: salto maximo)"));
		JumpFrom = Tip;
		if (A < 0.02f)
		{
			P->SetMoveInput(FVector2D(0.f, Sc));
			P->BotLaunch(0.f);
			bInFlight = true;
			Jumps++;
		}
		else
		{
			PendingAlpha = A;
			PendingScale = Sc;
			PendingAim = Aim;
			bJumpPending = true;
			P->SetMoveInput(FVector2D(0.f, Sc));
			P->SetChargeHeld(true);
		}
	}
}

// =========================================================================================== capturas
void APogoAutoTest::TickShots(float Dt)
{
	APogoGameMode* G = GM();
	APogoPlayerController* C = PC();
	APogoCharacter* P = Pogo();
	if (!G || !C || !G->GetCircuit())
	{
		return;
	}
	if (Actions.Num() == 0)
	{
		// cada accion se ejecuta en su propio paso; las capturas nunca comparten frame con un cambio de estado
		auto Add = [this](float Wait, TFunction<void()> Fn) { Actions.Add(TPair<float, TFunction<void()>>(Wait, MoveTemp(Fn))); };
		Add(3.0f, [this]() { Shot(TEXT("fp_start")); });
		Add(0.5f, [this]() { if (APogoCharacter* Q = Pogo()) { Q->bBotControlled = true; Q->SetMoveInput(FVector2D(0.f, 1.f)); Q->SetChargeHeld(true); } });
		Add(0.9f, [this]() { Shot(TEXT("fp_charging")); });
		Add(0.3f, [this]() { if (APogoCharacter* Q = Pogo()) { if (AController* Ctl = Q->GetController()) { Ctl->SetControlRotation(FRotator(-75.f, 0.f, 0.f)); } } });
		Add(0.4f, [this]() { Shot(TEXT("fp_lookdown")); });
		Add(0.3f, [this]() { if (APogoCharacter* Q = Pogo()) { Q->SetChargeHeld(false); if (AController* Ctl = Q->GetController()) { Ctl->SetControlRotation(FRotator(-10.f, 0.f, 0.f)); } } });
		Add(0.35f, [this]() { Shot(TEXT("fp_air")); });
		const TArray<FTransform> Views = G->GetCircuit()->ShotViews;
		for (int32 V = 0; V < Views.Num(); ++V)
		{
			const FTransform View = Views[V];
			Add(0.3f, [this, View]()
			{
				if (!ShotCam)
				{
					ShotCam = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), View);
					ShotCam->GetCameraComponent()->SetFieldOfView(85.f);
					ShotCam->GetCameraComponent()->bConstrainAspectRatio = false;
				}
				ShotCam->SetActorTransform(View);
				if (APogoPlayerController* Ctl = PC()) { Ctl->SetViewTarget(ShotCam); }
			});
			Add(1.0f, [this, V]() { Shot(FString::Printf(TEXT("view%d"), V), false); });
		}
		Add(0.3f, [this]() { if (APogoPlayerController* Ctl = PC()) { Ctl->SetViewTarget(Pogo()); Ctl->ShowPause(); } });
		Add(0.8f, [this]() { Shot(TEXT("ui_pause")); });
		Add(0.3f, [this]() { if (APogoPlayerController* Ctl = PC()) { Ctl->ShowOptions(true); } });
		Add(0.8f, [this]() { Shot(TEXT("ui_options")); });
		Add(0.3f, [this, G]()
		{
			APogoPlayerController* Ctl = PC();
			if (!Ctl) { return; }
			Ctl->Resume();
			FPogoRunResult R;
			R.Circuit = G->GetCircuitIndex();
			R.Time = 54.8f; R.Best = 54.8f; R.PreviousBest = 61.2f; R.bNewRecord = true;
			R.Falls = 0; R.bHighLine = true; R.bCleanRun = true; R.bUnlockedNext = true;
			Ctl->ShowResults(R);
		});
		Add(0.8f, [this]() { Shot(TEXT("ui_results")); });
		Add(0.3f, [this]() { Quit(1.0f); });
	}
	if (Actions.IsValidIndex(Step) && StepTime >= Actions[Step].Key)
	{
		StepTime = 0.f;
		const int32 Cur = Step++;
		Actions[Cur].Value();
	}
}

// =========================================================================================== mecanica
void APogoAutoTest::TickMech(float Dt)
{
	APogoCharacter* P = Pogo();
	if (!P)
	{
		return;
	}
	P->bBotControlled = true;
	const FVector Tip = P->GetTipLocation();
	// secuencia: 3 rebotes minimos, 3 cargas breves, 3 cargas completas (en el lugar) + 2 con avance
	static const float Alphas[] = {0.f, 0.f, 0.f, 0.25f, 0.25f, 0.25f, 1.f, 1.f, 1.f, 0.25f, 1.f};
	const int32 N = UE_ARRAY_COUNT(Alphas);
	const EPogoState S = P->GetPogoState();

	if (Step == 0)
	{
		if (Clock > 2.f && P->GetCharacterMovement()->IsMovingOnGround())
		{
			StartZ = Tip.Z;
			Step = 1;
		}
		return;
	}
	const int32 Idx = (Step - 1) / 2;
	if (Idx >= N)
	{
		for (const FString& L : MechLog)
		{
			UE_LOG(LogPogo, Log, TEXT("POGO_TEST mech %s"), *L);
		}
		Shot(TEXT("mech"));
		Quit(1.f);
		Step = 1000;
		return;
	}
	const bool bLaunchPhase = ((Step - 1) % 2) == 0;
	if (bLaunchPhase)
	{
		if ((S == EPogoState::Ready || S == EPogoState::Contact || S == EPogoState::Charging) && P->GetCharacterMovement()->IsMovingOnGround())
		{
			if (AController* C = P->GetController())
			{
				C->SetControlRotation(FRotator(-10.f, 0.f, 0.f));
			}
			const bool bForward = Idx >= 9;
			P->SetMoveInput(FVector2D(0.f, bForward ? 1.f : 0.f));
			// volver al origen antes de cada medicion
			if (!bForward)
			{
				P->SetActorLocation(FVector(-500.f, 0.f, P->GetActorLocation().Z));
			}
			else
			{
				P->SetActorLocation(FVector(-1200.f, -600.f, P->GetActorLocation().Z));
			}
			LaunchPos = P->GetTipLocation();
			ApexZ = LaunchPos.Z;
			P->BotLaunch(Alphas[Idx]);
			Step++;
			StepTime = 0.f;
		}
	}
	else
	{
		ApexZ = FMath::Max(ApexZ, Tip.Z);
		if (S != EPogoState::Air && StepTime > 0.1f)
		{
			const float Height = ApexZ - LaunchPos.Z;
			const float Dist = FVector::Dist2D(LaunchPos, Tip);
			MechLog.Add(FString::Printf(TEXT("carga=%.2f avance=%d altura=%.1f cm distancia=%.1f cm vuelo=%.2f s"), Alphas[Idx], Idx >= 9 ? 1 : 0, Height, Dist, StepTime));
			P->SetMoveInput(FVector2D::ZeroVector);
			Step++;
		}
	}
}

#include "PogoPlayerController.h"

#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Framework/Application/SlateApplication.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "NoSleepPogo.h"
#include "PogoArt.h"
#include "PogoCharacter.h"
#include "PogoGameInstance.h"
#include "PogoGameMode.h"
#include "PogoUI.h"
#include "Widgets/SWidget.h"

APogoPlayerController::APogoPlayerController()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
	bShowMouseCursor = false;
}

void APogoPlayerController::EnsureInput()
{
	if (IMC)
	{
		return;
	}
	IMC = NewObject<UInputMappingContext>(this, TEXT("IMC_Pogo"));

	IA_Move = NewObject<UInputAction>(this, TEXT("IA_Move"));
	IA_Move->ValueType = EInputActionValueType::Axis2D;
	IA_Look = NewObject<UInputAction>(this, TEXT("IA_Look"));
	IA_Look->ValueType = EInputActionValueType::Axis2D;
	IA_Charge = NewObject<UInputAction>(this, TEXT("IA_Charge"));
	IA_Charge->ValueType = EInputActionValueType::Boolean;
	IA_Pause = NewObject<UInputAction>(this, TEXT("IA_Pause"));
	IA_Pause->ValueType = EInputActionValueType::Boolean;
	IA_Pause->bTriggerWhenPaused = true;
	IA_Respawn = NewObject<UInputAction>(this, TEXT("IA_Respawn"));
	IA_Respawn->ValueType = EInputActionValueType::Boolean;

	auto Swizzle = [this]()
	{
		UInputModifierSwizzleAxis* S = NewObject<UInputModifierSwizzleAxis>(IMC);
		S->Order = EInputAxisSwizzle::YXZ;
		return S;
	};
	auto Negate = [this]()
	{
		return NewObject<UInputModifierNegate>(IMC);
	};

	// WASD / flechas: X = derecha, Y = adelante
	auto MapMove = [&](const FKey& Key, bool bForwardAxis, bool bNegative)
	{
		FEnhancedActionKeyMapping& M = IMC->MapKey(IA_Move, Key);
		if (bForwardAxis)
		{
			M.Modifiers.Add(Swizzle());
		}
		if (bNegative)
		{
			M.Modifiers.Add(Negate());
		}
	};
	MapMove(EKeys::W, true, false);
	MapMove(EKeys::S, true, true);
	MapMove(EKeys::D, false, false);
	MapMove(EKeys::A, false, true);
	MapMove(EKeys::Up, true, false);
	MapMove(EKeys::Down, true, true);
	MapMove(EKeys::Right, false, false);
	MapMove(EKeys::Left, false, true);
	{
		FEnhancedActionKeyMapping& M = IMC->MapKey(IA_Move, EKeys::Gamepad_Left2D);
		UInputModifierDeadZone* DZ = NewObject<UInputModifierDeadZone>(IMC);
		M.Modifiers.Add(DZ);
	}

	IMC->MapKey(IA_Look, EKeys::Mouse2D);
	{
		FEnhancedActionKeyMapping& M = IMC->MapKey(IA_Look, EKeys::Gamepad_Right2D);
		M.Modifiers.Add(NewObject<UInputModifierDeadZone>(IMC));
		UInputModifierScalar* Sc = NewObject<UInputModifierScalar>(IMC);
		Sc->Scalar = FVector(2.2, 1.6, 1.0);
		M.Modifiers.Add(Sc);
	}

	IMC->MapKey(IA_Charge, EKeys::SpaceBar);
	IMC->MapKey(IA_Charge, EKeys::LeftMouseButton);
	IMC->MapKey(IA_Charge, EKeys::Gamepad_FaceButton_Bottom);
	IMC->MapKey(IA_Charge, EKeys::Gamepad_RightTrigger);

	IMC->MapKey(IA_Pause, EKeys::Escape);
	IMC->MapKey(IA_Pause, EKeys::P);
	IMC->MapKey(IA_Pause, EKeys::Gamepad_Special_Right);
	IMC->MapKey(IA_Respawn, EKeys::R);
	IMC->MapKey(IA_Respawn, EKeys::Gamepad_FaceButton_Top);
}

void APogoPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	EnsureInput();
	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent))
	{
		EIC->BindAction(IA_Pause, ETriggerEvent::Started, this, &APogoPlayerController::OnPausePressed);
		EIC->BindAction(IA_Respawn, ETriggerEvent::Started, this, &APogoPlayerController::OnRespawnPressed);
	}
}

void APogoPlayerController::BeginPlay()
{
	Super::BeginPlay();
	EnsureInput();
	if (ULocalPlayer* LP = GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Sub = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			Sub->ClearAllMappings();
			Sub->AddMappingContext(IMC, 0);
		}
	}
	if (PlayerCameraManager)
	{
		PlayerCameraManager->ViewPitchMin = -85.f;
		PlayerCameraManager->ViewPitchMax = 80.f;
	}

	if (!IsLocalController())
	{
		return;
	}
	ConsoleCommand(TEXT("DisableAllScreenMessages"));
	APogoGameMode* GM = GetPogoGameMode();
	if (GM && GM->IsMenuMode())
	{
		ShowMainMenu();
	}
	else
	{
		ShowHUD();
		HintTimeLeft = 9.f;
	}
}

void APogoPlayerController::EndPlay(const EEndPlayReason::Type Reason)
{
	if (GEngine && GEngine->GameViewport)
	{
		if (HUDWidget.IsValid())
		{
			GEngine->GameViewport->RemoveViewportWidgetContent(HUDWidget.ToSharedRef());
		}
		if (ScreenWidget.IsValid())
		{
			GEngine->GameViewport->RemoveViewportWidgetContent(ScreenWidget.ToSharedRef());
		}
	}
	HUDWidget.Reset();
	ScreenWidget.Reset();
	Super::EndPlay(Reason);
}

void APogoPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	SetControlRotation(FRotator(-8.f, InPawn ? InPawn->GetActorRotation().Yaw : 0.f, 0.f));
}

void APogoPlayerController::Tick(float Dt)
{
	Super::Tick(Dt);
	const float RealDt = FApp::GetDeltaTime();
	ToastTimeLeft = FMath::Max(0.f, ToastTimeLeft - RealDt);
	if (!bPauseOpen)
	{
		HintTimeLeft = FMath::Max(0.f, HintTimeLeft - Dt);
	}
}

APogoCharacter* APogoPlayerController::GetPogo() const
{
	return Cast<APogoCharacter>(GetPawn());
}

APogoGameMode* APogoPlayerController::GetPogoGameMode() const
{
	return GetWorld() ? GetWorld()->GetAuthGameMode<APogoGameMode>() : nullptr;
}

// ------------------------------------------------------------------------------------- pantallas
void APogoPlayerController::SetScreen(TSharedPtr<SWidget> Widget, bool bUIOnly)
{
	if (!GEngine || !GEngine->GameViewport)
	{
		return;
	}
	if (ScreenWidget.IsValid())
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(ScreenWidget.ToSharedRef());
		ScreenWidget.Reset();
	}
	if (Widget.IsValid())
	{
		ScreenWidget = Widget;
		GEngine->GameViewport->AddViewportWidgetContent(ScreenWidget.ToSharedRef(), 20);
	}
	if (bUIOnly && ScreenWidget.IsValid())
	{
		FInputModeUIOnly Mode;
		Mode.SetWidgetToFocus(ScreenWidget);
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(Mode);
		bShowMouseCursor = true;
		FSlateApplication::Get().SetAllUserFocus(ScreenWidget, EFocusCause::SetDirectly);
	}
	else
	{
		SetInputMode(FInputModeGameOnly());
		bShowMouseCursor = false;
		FSlateApplication::Get().SetAllUserFocusToGameViewport();
	}
}

void APogoPlayerController::ShowHUD()
{
	if (!GEngine || !GEngine->GameViewport)
	{
		return;
	}
	if (!HUDWidget.IsValid())
	{
		HUDWidget = PogoUI::MakeHUD(this);
		GEngine->GameViewport->AddViewportWidgetContent(HUDWidget.ToSharedRef(), 10);
	}
	SetScreen(nullptr, false);
}

void APogoPlayerController::ClearScreen()
{
	SetScreen(nullptr, false);
}

void APogoPlayerController::ShowMainMenu()
{
	bPauseOpen = false;
	SetScreen(PogoUI::MakeMainMenu(this), true);
}

void APogoPlayerController::ShowPause()
{
	if (bResultsOpen)
	{
		return;
	}
	bPauseOpen = true;
	UGameplayStatics::SetGamePaused(this, true);
	PogoArt::PlaySfx(this, TEXT("SFX_UIClick"), 0.6f);
	SetScreen(PogoUI::MakePause(this), true);
}

void APogoPlayerController::Resume()
{
	bPauseOpen = false;
	UGameplayStatics::SetGamePaused(this, false);
	if (APogoCharacter* P = GetPogo())
	{
		P->SetChargeHeld(false);
		P->SetMoveInput(FVector2D::ZeroVector);
	}
	SetScreen(nullptr, false);
}

void APogoPlayerController::ShowOptions(bool bFromPause)
{
	bOptionsFromPause = bFromPause;
	SetScreen(PogoUI::MakeOptions(this, [this]()
	{
		if (UPogoGameInstance* GI = UPogoGameInstance::Get(this))
		{
			GI->SaveAll();
		}
		if (bOptionsFromPause)
		{
			SetScreen(PogoUI::MakePause(this), true);
		}
		else
		{
			ShowMainMenu();
		}
	}), true);
}

void APogoPlayerController::ShowResults(const FPogoRunResult& Result)
{
	LastResult = Result;
	bResultsOpen = true;
	SetScreen(PogoUI::MakeResults(this, Result), true);
}

void APogoPlayerController::ShowToast(const FText& Text, const FLinearColor& Color, float Duration)
{
	ToastText = Text;
	ToastColor = Color;
	ToastTimeLeft = Duration;
}

void APogoPlayerController::OnPausePressed()
{
	APogoGameMode* GM = GetPogoGameMode();
	if (!GM || GM->IsMenuMode() || bResultsOpen)
	{
		return;
	}
	if (bPauseOpen)
	{
		Resume();
	}
	else
	{
		ShowPause();
	}
}

void APogoPlayerController::OnRespawnPressed()
{
	APogoGameMode* GM = GetPogoGameMode();
	APogoCharacter* P = GetPogo();
	if (GM && P && !GM->IsMenuMode() && !GM->IsFinished() && !bPauseOpen && !bResultsOpen)
	{
		GM->PlayerFailed(P, EPogoFailReason::Manual);
	}
}

// ------------------------------------------------------------------------------------- acciones
void APogoPlayerController::PlayCircuit(int32 Index)
{
	if (UPogoGameInstance* GI = UPogoGameInstance::Get(this))
	{
		GI->LastCircuit = Index;
	}
	UGameplayStatics::SetGamePaused(this, false);
	UGameplayStatics::OpenLevel(this, FName(TEXT("L_City")), true, FString::Printf(TEXT("Circuit=%d"), Index));
}

void APogoPlayerController::RestartCircuit()
{
	if (APogoGameMode* GM = GetPogoGameMode())
	{
		PlayCircuit(GM->GetCircuitIndex());
	}
}

void APogoPlayerController::GoToMenu()
{
	UGameplayStatics::SetGamePaused(this, false);
	UGameplayStatics::OpenLevel(this, FName(TEXT("L_City")), true, TEXT(""));
}

void APogoPlayerController::QuitGame()
{
	if (UPogoGameInstance* GI = UPogoGameInstance::Get(this))
	{
		GI->SaveAll();
	}
	UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
}

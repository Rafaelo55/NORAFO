#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "PogoTypes.h"
#include "PogoPlayerController.generated.h"

class UInputAction;
class UInputMappingContext;
class SWidget;
class APogoCharacter;
class APogoGameMode;

/** Entrada (Enhanced Input creado en codigo) y todas las pantallas de interfaz (Slate). */
UCLASS()
class NOSLEEPPOGO_API APogoPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	APogoPlayerController();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void SetupInputComponent() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void Tick(float DeltaSeconds) override;

	void EnsureInput();

	UPROPERTY() TObjectPtr<UInputMappingContext> IMC;
	UPROPERTY() TObjectPtr<UInputAction> IA_Move;
	UPROPERTY() TObjectPtr<UInputAction> IA_Look;
	UPROPERTY() TObjectPtr<UInputAction> IA_Charge;
	UPROPERTY() TObjectPtr<UInputAction> IA_Pause;
	UPROPERTY() TObjectPtr<UInputAction> IA_Respawn;

	// ---- pantallas
	void ShowMainMenu();
	void ShowPause();
	void Resume();
	void ShowOptions(bool bFromPause);
	void ShowResults(const FPogoRunResult& Result);
	void ShowHUD();
	void ClearScreen();
	void ShowToast(const FText& Text, const FLinearColor& Color, float Duration = 2.2f);
	bool IsPausedMenuOpen() const { return bPauseOpen; }

	// ---- acciones de menu
	void PlayCircuit(int32 Index);
	void RestartCircuit();
	void GoToMenu();
	void QuitGame();

	APogoCharacter* GetPogo() const;
	APogoGameMode* GetPogoGameMode() const;

	// ---- datos para el HUD
	FText ToastText;
	FLinearColor ToastColor = FLinearColor::White;
	float ToastTimeLeft = 0.f;
	float HintTimeLeft = 0.f;
	FPogoRunResult LastResult;

private:
	void OnPausePressed();
	void OnRespawnPressed();
	void SetScreen(TSharedPtr<SWidget> Widget, bool bUIOnly);

	TSharedPtr<SWidget> HUDWidget;
	TSharedPtr<SWidget> ScreenWidget;
	bool bPauseOpen = false;
	bool bOptionsFromPause = false;
	bool bResultsOpen = false;
};

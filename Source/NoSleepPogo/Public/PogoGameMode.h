#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "PogoTypes.h"
#include "PogoGameMode.generated.h"

class APogoCircuit;
class APogoCharacter;
class ACameraActor;
class UAudioComponent;
class APostProcessVolume;
class ASkyLight;

/**
 * Partida: construye el circuito pedido por la URL (?Circuit=N), lleva el reloj,
 * las caidas, la reaparicion, la secuencia de toldos de la linea alta y la meta.
 * Sin ?Circuit el mapa funciona como menu principal (fondo de la cuadra).
 */
UCLASS()
class NOSLEEPPOGO_API APogoGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	APogoGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	bool IsMenuMode() const { return bMenuMode; }
	int32 GetCircuitIndex() const { return CircuitIndex; }
	APogoCircuit* GetCircuit() const { return Circuit; }

	// ---- estado de la pasada (lectura para el HUD)
	float GetRunTime() const { return RunTime; }
	bool IsTimerRunning() const { return bTimerStarted && !bFinished; }
	bool IsFinished() const { return bFinished; }
	int32 GetFalls() const { return Falls; }
	int32 GetHighLineProgress() const { return HighLineNext - 1; }
	int32 GetHighLineCount() const;
	bool IsHighLineDone() const { return bHighLineDone; }
	FText GetObjectiveText() const;

	// ---- eventos del jugador
	void NotifyTakeoff(APogoCharacter* Pogo, float Alpha);
	void NotifyLanded(APogoCharacter* Pogo, const FHitResult& Hit);
	void PlayerFailed(APogoCharacter* Pogo, EPogoFailReason Reason);

	void ApplyAudioVolumes();

	/** Bot de prueba / capturas (-PogoTest=bot|shots|mech). */
	FString TestMode;

private:
	void SpawnEnvironment();
	void Finish(APogoCharacter* Pogo);
	void UpdateMenuCamera(float Dt);

	UPROPERTY() TObjectPtr<APogoCircuit> Circuit;
	UPROPERTY() TObjectPtr<ACameraActor> MenuCamera;
	UPROPERTY() TObjectPtr<UAudioComponent> Music;
	UPROPERTY() TObjectPtr<UAudioComponent> Ambience;
	UPROPERTY() TObjectPtr<ASkyLight> SkyLight;

	int32 CircuitIndex = -1;
	bool bMenuMode = true;

	bool bTimerStarted = false;
	bool bFinished = false;
	float RunTime = 0.f;
	int32 Falls = 0;
	int32 CheckpointIndex = -1;
	int32 HighLineNext = 1;
	bool bHighLineDone = false;
	float FailCooldown = 0.f;
	float MenuTime = 0.f;
};

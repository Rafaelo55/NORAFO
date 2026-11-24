#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PogoAutoTest.generated.h"

class APogoCharacter;
class APogoGameMode;
class APogoPlayerController;
class ACameraActor;
struct FPogoBotTarget;

/**
 * Pruebas automaticas sin intervencion humana (linea de comandos -PogoTest=<modo>):
 *   bot      recorre la ruta principal con la fisica real y registra cada salto
 *   bothigh  igual, por la linea alta
 *   shots    capturas de pantalla del circuito, del HUD y de los menus
 *   menu     captura del menu principal
 *   mech     patio gris: mide la carga breve contra la completa (GDD prototipo 01)
 * Al terminar cierra el juego. Los resultados quedan en el log con el prefijo POGO_TEST.
 */
UCLASS()
class NOSLEEPPOGO_API APogoAutoTest : public AActor
{
	GENERATED_BODY()

public:
	APogoAutoTest();
	virtual void Tick(float DeltaSeconds) override;

	FString Mode;

private:
	void TickBot(float Dt);
	void TickShots(float Dt);
	void TickMech(float Dt);
	bool PlanJump(const FPogoBotTarget& T, float& OutAlpha, float& OutScale, FVector& OutAim, float& OutTime) const;
	bool IsInside(const FPogoBotTarget& T, const FVector& Tip, float Tol) const;
	void Shot(const FString& Name, bool bUI = true);
	void Quit(float Delay);

	APogoCharacter* Pogo() const;
	APogoGameMode* GM() const;
	APogoPlayerController* PC() const;

	float Clock = 0.f;
	float StepTime = 0.f;
	int32 Step = 0;
	bool bQuitting = false;

	// bot
	int32 Target = 0;
	int32 Jumps = 0;
	int32 Misses = 0;
	int32 LastFalls = 0;
	bool bJumpPending = false;
	bool bInFlight = false;
	float PendingAlpha = 0.f;
	float PendingScale = 0.f;
	FVector PendingAim = FVector::ZeroVector;
	float WaitTime = 0.f;
	float FinishedTime = -1.f;
	bool bHigh = false;
	FVector JumpFrom = FVector::ZeroVector;

	// mech
	float ApexZ = 0.f;
	float StartZ = 0.f;
	FVector LaunchPos = FVector::ZeroVector;
	TArray<FString> MechLog;

	UPROPERTY() TObjectPtr<ACameraActor> ShotCam;
	TArray<TPair<float, TFunction<void()>>> Actions;
};

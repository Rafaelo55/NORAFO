#pragma once

#include "CoreMinimal.h"
#include "Curves/CurveFloat.h"
#include "GameFramework/Character.h"
#include "PogoTypes.h"
#include "PogoCharacter.generated.h"

class UCameraComponent;
class UStaticMeshComponent;
class UAnimSequence;
struct FInputActionValue;

/** Prediccion de la caida (para el anillo y el bot). */
struct FPogoLandingPrediction
{
	bool bHit = false;
	bool bValid = false;
	FVector Location = FVector::ZeroVector;
	FVector Normal = FVector::UpVector;
	float Time = 0.f;
	TWeakObjectPtr<UPrimitiveComponent> Component;
};

/**
 * Controlador del pogo en primera persona.
 * Mantener Espacio al tocar suelo comprime el resorte (carga maxima en 0,45 s);
 * soltar salta. WASD define la direccion y permite una correccion corta en el aire.
 * Sin pulsar, cada contacto produce un rebote minimo.
 */
UCLASS()
class NOSLEEPPOGO_API APogoCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	APogoCharacter(const FObjectInitializer& ObjectInitializer);

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void Landed(const FHitResult& Hit) override;

	// ---- ajuste (GDD: "una curva regula el impulso")
	UPROPERTY(EditAnywhere, Category = "Pogo|Tuning") float ChargeTimeMax = 0.45f;
	/** Altura del salto (cm) segun la carga 0..1. */
	UPROPERTY(EditAnywhere, Category = "Pogo|Tuning") FRuntimeFloatCurve JumpHeightCurve;
	/** Velocidad horizontal al despegar (cm/s) segun la carga 0..1. */
	UPROPERTY(EditAnywhere, Category = "Pogo|Tuning") FRuntimeFloatCurve ForwardSpeedCurve;
	UPROPERTY(EditAnywhere, Category = "Pogo|Tuning") float AirAcceleration = 650.f;
	UPROPERTY(EditAnywhere, Category = "Pogo|Tuning") float AirMinSpeedCap = 170.f;
	UPROPERTY(EditAnywhere, Category = "Pogo|Tuning") float ContactGrace = 0.09f;
	UPROPERTY(EditAnywhere, Category = "Pogo|Tuning") float PogoGravityScale = 1.8f;

	UPROPERTY(VisibleAnywhere, Category = "Pogo") TObjectPtr<UCameraComponent> Camera;
	UPROPERTY(VisibleAnywhere, Category = "Pogo") TObjectPtr<UStaticMeshComponent> RingFull;
	UPROPERTY(VisibleAnywhere, Category = "Pogo") TObjectPtr<UStaticMeshComponent> RingBroken;

	// ---- estado (lectura para HUD / GameMode / bot)
	EPogoState GetPogoState() const { return State; }
	float GetChargeAlpha() const { return ChargeAlpha; }
	bool IsChargeHeld() const { return bChargeHeld; }
	const FPogoLandingPrediction& GetPrediction() const { return Prediction; }
	FVector GetMoveInput() const { return MoveInput; }

	float GetJumpHeight(float Alpha) const;
	float GetForwardSpeed(float Alpha) const;
	float GetPogoGravity() const;
	FVector GetTipLocation() const;

	/** Simula un despegue desde el suelo (para el anillo y el bot). */
	FPogoLandingPrediction PredictLaunch(float Alpha, const FVector& WorldDir, float InputScale) const;

	/** Reaparece en el punto dado en estado Listo. */
	void ResetToSpawn(const FTransform& Spawn);
	/** Llego al ojo azul: suelta el pogo. */
	void StartFinish();

	// ---- entrada (tambien usada por el bot)
	void SetMoveInput(const FVector2D& V) { MoveInput = FVector(V.X, V.Y, 0.f); }
	void SetChargeHeld(bool bHeld);
	/** Despegue directo con una carga dada (bot de validacion y prueba de mecanica). */
	void BotLaunch(float Alpha);
	bool bBotControlled = false;

protected:
	void OnMove(const FInputActionValue& Value);
	void OnMoveStop(const FInputActionValue& Value);
	void OnLook(const FInputActionValue& Value);
	void OnChargeStart(const FInputActionValue& Value);
	void OnChargeStop(const FInputActionValue& Value);

	void Launch(float Alpha);
	void EnterState(EPogoState NewState);
	FVector GetWorldMoveDir() const;
	void UpdateCompression(float Dt);
	void UpdateAnimation();
	void UpdateCamera(float Dt);
	void UpdatePrediction();
	bool IsFailComponent(const UPrimitiveComponent* C) const;

	EPogoState State = EPogoState::Ready;
	FVector MoveInput = FVector::ZeroVector;
	bool bChargeHeld = false;
	bool bChargeFullClicked = false;
	float ChargeTime = 0.f;
	float ChargeAlpha = 0.f;
	float ContactTimer = 0.f;
	float LastLaunchSpeed = 0.f;
	float StateTime = 0.f;
	bool bHasMovedOnce = false;

	// resorte visual (0 = extendido, 1 = comprimido)
	float Compression = 0.f;
	float CompressionVel = 0.f;
	float CameraKick = 0.f;
	float CameraKickVel = 0.f;

	FPogoLandingPrediction Prediction;

	UPROPERTY() TObjectPtr<UAnimSequence> AnimCompress;
	UPROPERTY() TObjectPtr<UAnimSequence> AnimLetGo;
	UPROPERTY() TObjectPtr<UAnimSequence> AnimBalance;
	UPROPERTY() TObjectPtr<UAnimSequence> CurrentAnim;

	FVector RestHead = FVector(-40.f, 0.f, 152.f);
};

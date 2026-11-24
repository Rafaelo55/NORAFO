#pragma once

#include "CoreMinimal.h"
#include "PogoTypes.generated.h"

/** Estados del controlador del pogo (GDD: contacto, carga y vuelo). */
UENUM(BlueprintType)
enum class EPogoState : uint8
{
	Ready,     // parado sobre el pogo esperando la primera entrada
	Contact,   // toco el suelo: ventana breve antes del rebote minimo
	Charging,  // manteniendo Espacio en el suelo: el resorte se comprime
	Air,       // en vuelo
	Finished   // llego al ojo azul y solto el pogo
};

UENUM(BlueprintType)
enum class EPogoFailReason : uint8
{
	Car,
	OffRoute,
	Fell,
	Manual
};

/** Opciones del jugador (se guardan en disco). */
USTRUCT(BlueprintType)
struct FPogoSettings
{
	GENERATED_BODY()

	UPROPERTY() float FOV = 95.f;
	UPROPERTY() float Sway = 1.f;
	UPROPERTY() float MouseSensitivity = 1.f;
	UPROPERTY() bool bInvertY = false;
	UPROPERTY() bool bShowTimer = true;
	UPROPERTY() bool bShowRing = true;
	UPROPERTY() float PixelScale = 50.f;
	UPROPERTY() float MusicVolume = 0.55f;
	UPROPERTY() float SfxVolume = 1.f;
};

/** Progreso local por circuito (GDD: identificador, desbloqueo, mejor tiempo y marcas). */
USTRUCT(BlueprintType)
struct FPogoCircuitProgress
{
	GENERATED_BODY()

	UPROPERTY() FName Id;
	UPROPERTY() bool bUnlocked = false;
	UPROPERTY() bool bCompleted = false;
	UPROPERTY() float BestTime = -1.f;
	UPROPERTY() bool bCleanRun = false;
	UPROPERTY() bool bHighLine = false;
	UPROPERTY() int32 Completions = 0;
};

/** Resultado de una pasada completa. */
USTRUCT(BlueprintType)
struct FPogoRunResult
{
	GENERATED_BODY()

	UPROPERTY() int32 Circuit = 0;
	UPROPERTY() float Time = 0.f;
	UPROPERTY() float PreviousBest = -1.f;
	UPROPERTY() float Best = -1.f;
	UPROPERTY() bool bNewRecord = false;
	UPROPERTY() int32 Falls = 0;
	UPROPERTY() bool bHighLine = false;
	UPROPERTY() bool bCleanRun = false;
	UPROPERTY() bool bUnlockedNext = false;
};

struct FPogoCircuitInfo
{
	FName Id;
	FText Number;
	FText Name;
	FText Tagline;
	FText Description;
	FText GoalName;
	FText Unlocks;
};

namespace PogoCircuits
{
	constexpr int32 Num = 3;
	constexpr int32 TestYard = 99;
	NOSLEEPPOGO_API const FPogoCircuitInfo& Get(int32 Index);
}

namespace PogoFormat
{
	/** 00:48,2 */
	NOSLEEPPOGO_API FText Time(float Seconds);
}

namespace PogoColors
{
	inline const FLinearColor Navy(0.006f, 0.010f, 0.025f, 1.f);
	inline const FLinearColor NavyPanel(0.010f, 0.016f, 0.040f, 0.86f);
	inline const FLinearColor Cream(0.90f, 0.86f, 0.79f, 1.f);
	inline const FLinearColor Blue(0.017f, 0.061f, 0.578f, 1.f);
	inline const FLinearColor BlueLight(0.39f, 0.60f, 0.77f, 1.f);
	inline const FLinearColor Red(0.39f, 0.042f, 0.036f, 1.f);
	inline const FLinearColor Gray(0.48f, 0.44f, 0.40f, 1.f);
	inline const FLinearColor Gold(0.95f, 0.70f, 0.15f, 1.f);
}

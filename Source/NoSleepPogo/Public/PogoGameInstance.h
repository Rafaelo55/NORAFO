#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "GameFramework/SaveGame.h"
#include "PogoTypes.h"
#include "PogoGameInstance.generated.h"

UCLASS()
class NOSLEEPPOGO_API UPogoSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY() int32 Version = 1;
	UPROPERTY() FPogoSettings Settings;
	UPROPERTY() TArray<FPogoCircuitProgress> Circuits;
};

/** Guarda opciones y progreso local; aplica las opciones de video. */
UCLASS()
class NOSLEEPPOGO_API UPogoGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	static UPogoGameInstance* Get(const UObject* WorldContext);

	virtual void Init() override;

	UPROPERTY() FPogoSettings Settings;
	UPROPERTY() TArray<FPogoCircuitProgress> Progress;

	/** Circuito elegido en el selector (para volver al menu con la tarjeta enfocada). */
	int32 LastCircuit = 0;

	/** Intento actual del bot de prueba (sobrevive al reinicio del nivel). */
	int32 BotAttempt = 0;

	FPogoCircuitProgress& GetProgress(int32 Circuit);
	bool IsUnlocked(int32 Circuit) const;

	/** Registra una llegada y devuelve el resultado listo para la pantalla final. */
	FPogoRunResult RecordRun(int32 Circuit, float Time, int32 Falls, bool bHighLine);

	void SaveAll();
	void LoadAll();
	void ResetProgress();
	void ApplySettings();

	static const TCHAR* SlotName;
};

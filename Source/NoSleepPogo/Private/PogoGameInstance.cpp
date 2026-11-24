#include "PogoGameInstance.h"

#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "NoSleepPogo.h"

const TCHAR* UPogoGameInstance::SlotName = TEXT("NoSleepPogo");

UPogoGameInstance* UPogoGameInstance::Get(const UObject* WorldContext)
{
	if (!WorldContext)
	{
		return nullptr;
	}
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? Cast<UPogoGameInstance>(World->GetGameInstance()) : nullptr;
}

void UPogoGameInstance::Init()
{
	Super::Init();
	LoadAll();
	ApplySettings();
}

FPogoCircuitProgress& UPogoGameInstance::GetProgress(int32 Circuit)
{
	while (Progress.Num() < PogoCircuits::Num)
	{
		FPogoCircuitProgress P;
		P.Id = PogoCircuits::Get(Progress.Num()).Id;
		P.bUnlocked = Progress.Num() == 0;
		Progress.Add(P);
	}
	static FPogoCircuitProgress Dummy;
	if (!Progress.IsValidIndex(Circuit))
	{
		Dummy = FPogoCircuitProgress();
		Dummy.bUnlocked = true;
		return Dummy;
	}
	return Progress[Circuit];
}

bool UPogoGameInstance::IsUnlocked(int32 Circuit) const
{
	if (Circuit == 0)
	{
		return true;
	}
	return Progress.IsValidIndex(Circuit) && Progress[Circuit].bUnlocked;
}

FPogoRunResult UPogoGameInstance::RecordRun(int32 Circuit, float Time, int32 Falls, bool bHighLine)
{
	FPogoRunResult R;
	R.Circuit = Circuit;
	R.Time = Time;
	R.Falls = Falls;
	R.bHighLine = bHighLine;
	R.bCleanRun = Falls == 0;

	if (Circuit < 0 || Circuit >= PogoCircuits::Num)
	{
		R.Best = Time;
		return R;
	}

	FPogoCircuitProgress& P = GetProgress(Circuit);
	R.PreviousBest = P.BestTime;
	if (P.BestTime < 0.f || Time < P.BestTime)
	{
		P.BestTime = Time;
		R.bNewRecord = true;
	}
	R.Best = P.BestTime;
	P.bCompleted = true;
	P.Completions++;
	P.bCleanRun |= R.bCleanRun;
	P.bHighLine |= bHighLine;

	if (Circuit + 1 < PogoCircuits::Num)
	{
		FPogoCircuitProgress& Next = GetProgress(Circuit + 1);
		if (!Next.bUnlocked)
		{
			Next.bUnlocked = true;
			R.bUnlockedNext = true;
		}
	}
	SaveAll();
	return R;
}

void UPogoGameInstance::SaveAll()
{
	UPogoSaveGame* Save = Cast<UPogoSaveGame>(UGameplayStatics::CreateSaveGameObject(UPogoSaveGame::StaticClass()));
	Save->Settings = Settings;
	GetProgress(0);
	Save->Circuits = Progress;
	if (!UGameplayStatics::SaveGameToSlot(Save, SlotName, 0))
	{
		UE_LOG(LogPogo, Warning, TEXT("No se pudo guardar el progreso"));
	}
}

void UPogoGameInstance::LoadAll()
{
	Progress.Reset();
	if (UGameplayStatics::DoesSaveGameExist(SlotName, 0))
	{
		if (UPogoSaveGame* Save = Cast<UPogoSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, 0)))
		{
			Settings = Save->Settings;
			// emparejar por identificador por si cambia el orden de los circuitos
			for (int32 i = 0; i < PogoCircuits::Num; ++i)
			{
				FPogoCircuitProgress& P = GetProgress(i);
				for (const FPogoCircuitProgress& S : Save->Circuits)
				{
					if (S.Id == P.Id)
					{
						P = S;
					}
				}
			}
		}
	}
	GetProgress(0).bUnlocked = true;
	if (FParse::Param(FCommandLine::Get(), TEXT("PogoUnlockAll")))
	{
		for (int32 i = 0; i < PogoCircuits::Num; ++i)
		{
			GetProgress(i).bUnlocked = true;
		}
	}
}

void UPogoGameInstance::ResetProgress()
{
	Progress.Reset();
	GetProgress(0);
	SaveAll();
}

void UPogoGameInstance::ApplySettings()
{
	Settings.PixelScale = FMath::Clamp(Settings.PixelScale, 25.f, 100.f);
	Settings.FOV = FMath::Clamp(Settings.FOV, 70.f, 115.f);
	if (IConsoleVariable* SP = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage")))
	{
		SP->Set(Settings.PixelScale, ECVF_SetByCode);
	}
	if (IConsoleVariable* UQ = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Upscale.Quality")))
	{
		UQ->Set(0, ECVF_SetByCode);
	}
}

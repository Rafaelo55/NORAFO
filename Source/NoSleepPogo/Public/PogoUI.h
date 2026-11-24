#pragma once

#include "CoreMinimal.h"
#include "PogoTypes.h"

class SWidget;
class APogoPlayerController;

/** Pantallas de interfaz construidas en Slate (sin assets de UMG). */
namespace PogoUI
{
	TSharedRef<SWidget> MakeHUD(APogoPlayerController* PC);
	TSharedRef<SWidget> MakeMainMenu(APogoPlayerController* PC);
	TSharedRef<SWidget> MakePause(APogoPlayerController* PC);
	TSharedRef<SWidget> MakeOptions(APogoPlayerController* PC, TFunction<void()> OnBack);
	TSharedRef<SWidget> MakeResults(APogoPlayerController* PC, const FPogoRunResult& Result);
}

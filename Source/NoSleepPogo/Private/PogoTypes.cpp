#include "PogoTypes.h"

namespace PogoCircuits
{
	static TArray<FPogoCircuitInfo> BuildInfos()
	{
		TArray<FPogoCircuitInfo> Out;
		{
			FPogoCircuitInfo I;
			I.Id = TEXT("cuadra");
			I.Number = FText::FromString(TEXT("CIRCUITO 01"));
			I.Name = FText::FromString(TEXT("La cuadra"));
			I.Tagline = FText::FromString(TEXT("Aprender el rebote"));
			I.Description = FText::FromString(TEXT("Vereda, cruce y dos toldos. La primera decisión es esperar al auto o aprovechar el hueco y seguir de largo."));
			I.GoalName = FText::FromString(TEXT("AZOTEA"));
			I.Unlocks = FText::FromString(TEXT("Completar abre Galería de toldos."));
			Out.Add(I);
		}
		{
			FPogoCircuitInfo I;
			I.Id = TEXT("galeria");
			I.Number = FText::FromString(TEXT("CIRCUITO 02"));
			I.Name = FText::FromString(TEXT("Galería de toldos"));
			I.Tagline = FText::FromString(TEXT("Enlazar saltos laterales"));
			I.Description = FText::FromString(TEXT("Los apoyos se alternan a izquierda y derecha. Una vereda de servicio permite recuperar el ritmo después de una caída corta."));
			I.GoalName = FText::FromString(TEXT("TERRAZA"));
			I.Unlocks = FText::FromString(TEXT("Completar abre Azoteas en zigzag."));
			Out.Add(I);
		}
		{
			FPogoCircuitInfo I;
			I.Id = TEXT("zigzag");
			I.Number = FText::FromString(TEXT("CIRCUITO 03"));
			I.Name = FText::FromString(TEXT("Azoteas en zigzag"));
			I.Tagline = FText::FromString(TEXT("Anticipar la salida"));
			I.Description = FText::FromString(TEXT("Las cornisas forman dos giros. El apoyo más cercano deja mal orientado el salto siguiente. Un techo ancho permite preparar otra línea."));
			I.GoalName = FText::FromString(TEXT("ÚLTIMA AZOTEA"));
			I.Unlocks = FText::FromString(TEXT("Completar cierra la serie de circuitos."));
			Out.Add(I);
		}
		return Out;
	}

	const FPogoCircuitInfo& Get(int32 Index)
	{
		static const TArray<FPogoCircuitInfo> Infos = BuildInfos();
		static FPogoCircuitInfo Yard = []()
		{
			FPogoCircuitInfo I;
			I.Id = TEXT("patio");
			I.Number = FText::FromString(TEXT("PRUEBA"));
			I.Name = FText::FromString(TEXT("Patio gris"));
			I.Tagline = FText::FromString(TEXT("Tres plataformas"));
			I.GoalName = FText::FromString(TEXT("PLATAFORMA ALTA"));
			return I;
		}();
		if (Infos.IsValidIndex(Index))
		{
			return Infos[Index];
		}
		return Yard;
	}
}

namespace PogoFormat
{
	FText Time(float Seconds)
	{
		if (Seconds < 0.f)
		{
			return FText::FromString(TEXT("--:--,-"));
		}
		const int32 Tenths = FMath::FloorToInt(Seconds * 10.f);
		const int32 Min = Tenths / 600;
		const int32 Sec = (Tenths / 10) % 60;
		const int32 Dec = Tenths % 10;
		return FText::FromString(FString::Printf(TEXT("%02d:%02d,%d"), Min, Sec, Dec));
	}
}

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PogoArt.generated.h"

class UStaticMesh;
class USkeletalMesh;
class UAnimSequence;
class UMaterialInterface;
class UTexture2D;
class USoundBase;

/** Cache raiz de assets cargados por ruta (evita que el GC los descargue entre usos). */
UCLASS()
class NOSLEEPPOGO_API UPogoAssetCache : public UObject
{
	GENERATED_BODY()

public:
	static UPogoAssetCache& Get();

	UObject* Load(UClass* Class, const FString& Path);

private:
	UPROPERTY()
	TMap<FString, TObjectPtr<UObject>> Assets;
};

/** Acceso tipado a los assets generados por el pipeline (Tools/unreal/import_assets.py). */
namespace PogoArt
{
	NOSLEEPPOGO_API UStaticMesh* Mesh(const FString& Name);
	NOSLEEPPOGO_API USkeletalMesh* RiderMesh();
	NOSLEEPPOGO_API UAnimSequence* RiderAnim(const FString& Name);
	NOSLEEPPOGO_API UMaterialInterface* Mat(const FString& Name);       // MI_<Name>
	NOSLEEPPOGO_API UMaterialInterface* Master(const FString& Name);    // M_<Name>
	NOSLEEPPOGO_API UTexture2D* Tex(const FString& Name);               // T_<Name>
	NOSLEEPPOGO_API USoundBase* Sound(const FString& Name);

	/** Reproduce un efecto 2D respetando el volumen de efectos de las opciones. */
	NOSLEEPPOGO_API void PlaySfx(const UObject* WorldContext, const FString& Name, float Volume = 1.f, float Pitch = 1.f);
}

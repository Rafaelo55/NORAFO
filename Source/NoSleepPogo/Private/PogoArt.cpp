#include "PogoArt.h"

#include "Animation/AnimSequence.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "NoSleepPogo.h"
#include "PogoGameInstance.h"
#include "Sound/SoundBase.h"

UPogoAssetCache& UPogoAssetCache::Get()
{
	static UPogoAssetCache* Instance = nullptr;
	if (!Instance)
	{
		Instance = NewObject<UPogoAssetCache>(GetTransientPackage(), TEXT("PogoAssetCache"));
		Instance->AddToRoot();
	}
	return *Instance;
}

UObject* UPogoAssetCache::Load(UClass* Class, const FString& Path)
{
	if (TObjectPtr<UObject>* Found = Assets.Find(Path))
	{
		return Found->Get();
	}
	UObject* Obj = StaticLoadObject(Class, nullptr, *Path);
	if (!Obj)
	{
		UE_LOG(LogPogo, Warning, TEXT("Asset no encontrado: %s"), *Path);
	}
	Assets.Add(Path, Obj);
	return Obj;
}

namespace PogoArt
{
	static FString P(const TCHAR* Folder, const FString& Name)
	{
		return FString::Printf(TEXT("/Game/Pogo/%s/%s.%s"), Folder, *Name, *Name);
	}

	UStaticMesh* Mesh(const FString& Name)
	{
		return Cast<UStaticMesh>(UPogoAssetCache::Get().Load(UStaticMesh::StaticClass(), P(TEXT("Meshes"), Name)));
	}

	USkeletalMesh* RiderMesh()
	{
		return Cast<USkeletalMesh>(UPogoAssetCache::Get().Load(USkeletalMesh::StaticClass(), P(TEXT("Characters"), TEXT("SK_PogoRider"))));
	}

	UAnimSequence* RiderAnim(const FString& Name)
	{
		return Cast<UAnimSequence>(UPogoAssetCache::Get().Load(UAnimSequence::StaticClass(), P(TEXT("Characters"), TEXT("A_Rider_") + Name)));
	}

	UMaterialInterface* Mat(const FString& Name)
	{
		return Cast<UMaterialInterface>(UPogoAssetCache::Get().Load(UMaterialInterface::StaticClass(), P(TEXT("Materials"), TEXT("MI_") + Name)));
	}

	UMaterialInterface* Master(const FString& Name)
	{
		return Cast<UMaterialInterface>(UPogoAssetCache::Get().Load(UMaterialInterface::StaticClass(), P(TEXT("Materials"), TEXT("M_") + Name)));
	}

	UTexture2D* Tex(const FString& Name)
	{
		return Cast<UTexture2D>(UPogoAssetCache::Get().Load(UTexture2D::StaticClass(), P(TEXT("Textures"), TEXT("T_") + Name)));
	}

	USoundBase* Sound(const FString& Name)
	{
		return Cast<USoundBase>(UPogoAssetCache::Get().Load(USoundBase::StaticClass(), P(TEXT("Audio"), Name)));
	}

	void PlaySfx(const UObject* WorldContext, const FString& Name, float Volume, float Pitch)
	{
		if (!WorldContext)
		{
			return;
		}
		float Master = 1.f;
		if (const UPogoGameInstance* GI = UPogoGameInstance::Get(WorldContext))
		{
			Master = GI->Settings.SfxVolume;
		}
		if (Master <= 0.001f)
		{
			return;
		}
		if (USoundBase* S = Sound(Name))
		{
			UGameplayStatics::PlaySound2D(WorldContext, S, Volume * Master, Pitch);
		}
	}
}

#include "PogoUI.h"

#include "Brushes/SlateColorBrush.h"
#include "Framework/Application/SlateApplication.h"
#include "PogoArt.h"
#include "PogoCharacter.h"
#include "PogoGameInstance.h"
#include "PogoGameMode.h"
#include "PogoPlayerController.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define T(x) FText::FromString(TEXT(x))

namespace
{
	using FPCWeak = TWeakObjectPtr<APogoPlayerController>;

	FSlateFontInfo Font(const ANSICHAR* Typeface, int32 Size, int32 Spacing = 0)
	{
		FSlateFontInfo F = FCoreStyle::GetDefaultFontStyle(Typeface, Size);
		F.LetterSpacing = Spacing;
		return F;
	}

	const FSlateBrush* Brush(const FLinearColor& C)
	{
		static TMap<uint32, TUniquePtr<FSlateColorBrush>> Cache;
		const uint32 Key = C.ToFColor(false).ToPackedARGB();
		if (TUniquePtr<FSlateColorBrush>* B = Cache.Find(Key))
		{
			return B->Get();
		}
		return Cache.Add(Key, MakeUnique<FSlateColorBrush>(C)).Get();
	}

	const FButtonStyle& ButtonStyle(int32 Kind)
	{
		auto Make = [](const FLinearColor& N, const FLinearColor& H, const FLinearColor& P)
		{
			FButtonStyle S = FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("Button");
			S.SetNormal(FSlateColorBrush(N));
			S.SetHovered(FSlateColorBrush(H));
			S.SetPressed(FSlateColorBrush(P));
			S.SetDisabled(FSlateColorBrush(FLinearColor(0.02f, 0.02f, 0.03f, 0.7f)));
			S.SetNormalPadding(FMargin(0));
			S.SetPressedPadding(FMargin(0));
			return S;
		};
		static const FButtonStyle Normal = Make(FLinearColor(0.02f, 0.03f, 0.07f, 0.92f), FLinearColor(0.03f, 0.10f, 0.62f, 1.f), FLinearColor(0.01f, 0.04f, 0.3f, 1.f));
		static const FButtonStyle Primary = Make(FLinearColor(0.02f, 0.07f, 0.52f, 1.f), FLinearColor(0.05f, 0.16f, 0.85f, 1.f), FLinearColor(0.01f, 0.04f, 0.3f, 1.f));
		static const FButtonStyle Card = Make(FLinearColor(0.012f, 0.02f, 0.05f, 0.88f), FLinearColor(0.025f, 0.07f, 0.42f, 0.95f), FLinearColor(0.01f, 0.04f, 0.3f, 1.f));
		return Kind == 1 ? Primary : (Kind == 2 ? Card : Normal);
	}

	void Click(const FPCWeak& PC)
	{
		if (PC.IsValid())
		{
			PogoArt::PlaySfx(PC.Get(), TEXT("SFX_UIClick"), 0.7f);
		}
	}

	TSharedRef<SWidget> Label(const FText& Text, int32 Size = 13, const FLinearColor& Color = PogoColors::Cream, const ANSICHAR* Face = "Mono", int32 Spacing = 120)
	{
		return SNew(STextBlock).Text(Text).Font(Font(Face, Size, Spacing)).ColorAndOpacity(Color);
	}

	TSharedRef<SWidget> Button(const FPCWeak& PC, const FText& Text, TFunction<void()> OnClick, int32 Kind = 0, float MinWidth = 340.f, TAttribute<bool> Enabled = true)
	{
		return SNew(SBox).MinDesiredWidth(MinWidth)
		[
			SNew(SButton)
			.ButtonStyle(&ButtonStyle(Kind))
			.IsEnabled(Enabled)
			.ContentPadding(FMargin(24.f, 13.f))
			.OnHovered_Lambda([PC]() { if (PC.IsValid()) { PogoArt::PlaySfx(PC.Get(), TEXT("SFX_UIMove"), 0.35f); } })
			.OnClicked_Lambda([PC, OnClick]()
			{
				Click(PC);
				OnClick();
				return FReply::Handled();
			})
			[
				SNew(STextBlock).Text(Text).Font(Font("Bold", 17, 90)).ColorAndOpacity(PogoColors::Cream)
			]
		];
	}

	TSharedRef<SWidget> Panel(TSharedRef<SWidget> Content, const FMargin& Pad = FMargin(18.f, 12.f), const FLinearColor& Color = PogoColors::NavyPanel)
	{
		return SNew(SBorder).BorderImage(Brush(Color)).Padding(Pad)[Content];
	}

	/** Contenedor de pantalla que recibe el foco y las teclas. */
	class SPogoScreen : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SPogoScreen) {}
			SLATE_DEFAULT_SLOT(FArguments, Content)
		SLATE_END_ARGS()

		TFunction<bool(const FKey&)> OnKey;

		void Construct(const FArguments& Args)
		{
			ChildSlot[Args._Content.Widget];
		}
		virtual bool SupportsKeyboardFocus() const override { return true; }
		virtual FReply OnKeyDown(const FGeometry& Geo, const FKeyEvent& Ev) override
		{
			if (OnKey && OnKey(Ev.GetKey()))
			{
				return FReply::Handled();
			}
			return SCompoundWidget::OnKeyDown(Geo, Ev);
		}
	};

	TSharedRef<SWidget> Screen(TSharedRef<SWidget> Content, TFunction<bool(const FKey&)> OnKey)
	{
		TSharedRef<SPogoScreen> S = SNew(SPogoScreen)[Content];
		S->OnKey = OnKey;
		return S;
	}

	FText StateText(const APogoCharacter* P)
	{
		if (!P)
		{
			return FText::GetEmpty();
		}
		switch (P->GetPogoState())
		{
		case EPogoState::Ready: return T("LISTO");
		case EPogoState::Contact: return T("EN CONTACTO");
		case EPogoState::Charging: return P->GetChargeAlpha() >= 1.f ? T("CARGA MÁXIMA") : T("CARGANDO");
		case EPogoState::Air: return T("EN EL AIRE");
		case EPogoState::Finished: return T("META");
		}
		return FText::GetEmpty();
	}
}

// =============================================================================================== HUD
TSharedRef<SWidget> PogoUI::MakeHUD(APogoPlayerController* InPC)
{
	FPCWeak PC(InPC);
	auto GM = [PC]() -> APogoGameMode* { return PC.IsValid() ? PC->GetPogoGameMode() : nullptr; };
	auto Pogo = [PC]() -> APogoCharacter* { return PC.IsValid() ? PC->GetPogo() : nullptr; };
	auto Settings = [PC]() -> const FPogoSettings* { const UPogoGameInstance* GI = PC.IsValid() ? UPogoGameInstance::Get(PC.Get()) : nullptr; return GI ? &GI->Settings : nullptr; };

	TSharedRef<SHorizontalBox> Marks = SNew(SHorizontalBox);
	for (int32 i = 0; i < 3; ++i)
	{
		Marks->AddSlot().AutoWidth().Padding(0.f, 0.f, 6.f, 0.f)
		[
			SNew(SBox).WidthOverride(30.f).HeightOverride(11.f)
			[
				SNew(SBorder)
				.BorderImage(Brush(FLinearColor::White))
				.BorderBackgroundColor_Lambda([Pogo, i]()
				{
					const APogoCharacter* P = Pogo();
					const float A = P && P->GetPogoState() == EPogoState::Charging ? P->GetChargeAlpha() : 0.f;
					const bool bOn = A >= (i + 1) / 3.f - 0.001f || (i == 0 && A > 0.05f);
					if (!bOn)
					{
						return FSlateColor(FLinearColor(0.25f, 0.27f, 0.33f, 0.55f));
					}
					return FSlateColor(A >= 1.f ? PogoColors::Gold : FLinearColor(0.15f, 0.38f, 1.f));
				})
			]
		];
	}

	return SNew(SOverlay)
	.Visibility(EVisibility::HitTestInvisible)
	// ---- 1. orientarse: objetivo
	+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(36.f, 32.f)
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			Panel(SNew(STextBlock)
				.Text_Lambda([GM]() { const APogoGameMode* G = GM(); return G ? G->GetObjectiveText() : FText::GetEmpty(); })
				.Font(Font("Mono", 15, 160)).ColorAndOpacity(PogoColors::Cream))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)
		[
			SNew(STextBlock)
			.Text_Lambda([GM]()
			{
				const APogoGameMode* G = GM();
				if (!G)
				{
					return FText::GetEmpty();
				}
				FString S = FString::Printf(TEXT("CAÍDAS %d"), G->GetFalls());
				if (G->GetHighLineCount() > 0)
				{
					S += G->IsHighLineDone() ? TEXT("   ·   LÍNEA ALTA COMPLETA") : FString::Printf(TEXT("   ·   LÍNEA ALTA %d/%d"), G->GetHighLineProgress(), G->GetHighLineCount());
				}
				return FText::FromString(S);
			})
			.Font(Font("Mono", 11, 140)).ColorAndOpacity(FLinearColor(0.85f, 0.85f, 0.9f, 0.85f))
			.ShadowOffset(FVector2D(1.f, 1.f)).ShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.8f))
		]
	]
	// ---- cronometro
	+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(36.f, 32.f)
	[
		SNew(SBox)
		.Visibility_Lambda([Settings]() { const FPogoSettings* S = Settings(); return (!S || S->bShowTimer) ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
		[
			Panel(SNew(STextBlock)
				.Text_Lambda([GM]() { const APogoGameMode* G = GM(); return PogoFormat::Time(G ? G->GetRunTime() : 0.f); })
				.Font(Font("Mono", 26, 60))
				.ColorAndOpacity_Lambda([GM]() { const APogoGameMode* G = GM(); return FSlateColor(G && G->IsFinished() ? PogoColors::Gold : PogoColors::Cream); }),
				FMargin(20.f, 10.f))
		]
	]
	// ---- 3. sentir la carga
	+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(36.f, 34.f)
	[
		Panel(
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock).Text_Lambda([Pogo]() { return StateText(Pogo()); }).Font(Font("Mono", 15, 160)).ColorAndOpacity(PogoColors::Cream)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 0.f)[Marks],
			FMargin(18.f, 12.f))
	]
	// ---- avisos
	+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(0.f, 120.f)
	[
		SNew(STextBlock)
		.Visibility_Lambda([PC]() { return PC.IsValid() && PC->ToastTimeLeft > 0.f ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
		.Text_Lambda([PC]() { return PC.IsValid() ? PC->ToastText : FText::GetEmpty(); })
		.Font(Font("Bold", 28, 140))
		.ColorAndOpacity_Lambda([PC]()
		{
			if (!PC.IsValid())
			{
				return FSlateColor(FLinearColor::White);
			}
			FLinearColor C = PC->ToastColor;
			C.A = FMath::Clamp(PC->ToastTimeLeft / 0.4f, 0.f, 1.f);
			return FSlateColor(C);
		})
		.ShadowOffset(FVector2D(2.f, 2.f)).ShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.85f))
	]
	// ---- ayuda inicial
	+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(0.f, 40.f)
	[
		SNew(SBox)
		.Visibility_Lambda([PC]() { return PC.IsValid() && PC->HintTimeLeft > 0.f ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
		[
			Panel(SNew(STextBlock)
				.Text(T("MANTENÉ ESPACIO PARA CARGAR  ·  SOLTÁ PARA SALTAR  ·  WASD DIRECCIÓN  ·  R VOLVER AL PUNTO"))
				.Font(Font("Mono", 12, 120)).ColorAndOpacity(PogoColors::Cream), FMargin(18.f, 10.f))
		]
	];
}

// =============================================================================================== menu principal
TSharedRef<SWidget> PogoUI::MakeMainMenu(APogoPlayerController* InPC)
{
	FPCWeak PC(InPC);
	UPogoGameInstance* GI = UPogoGameInstance::Get(InPC);

	TSharedRef<SVerticalBox> Cards = SNew(SVerticalBox);
	for (int32 i = 0; i < PogoCircuits::Num; ++i)
	{
		const FPogoCircuitInfo& Info = PogoCircuits::Get(i);
		const bool bUnlocked = GI ? GI->IsUnlocked(i) : i == 0;
		FPogoCircuitProgress Prog;
		if (GI)
		{
			Prog = GI->GetProgress(i);
		}
		FString Status;
		if (!bUnlocked)
		{
			Status = FString::Printf(TEXT("BLOQUEADO  ·  completá %s"), *PogoCircuits::Get(i - 1).Name.ToString());
		}
		else if (Prog.BestTime >= 0.f)
		{
			Status = FString::Printf(TEXT("RÉCORD %s"), *PogoFormat::Time(Prog.BestTime).ToString());
		}
		else
		{
			Status = TEXT("SIN COMPLETAR");
		}

		TSharedRef<SHorizontalBox> MarksRow = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()[Label(FText::FromString(Status), 11, bUnlocked ? PogoColors::BlueLight : PogoColors::Gray)];
		if (bUnlocked)
		{
			MarksRow->AddSlot().AutoWidth().Padding(18.f, 0.f, 0.f, 0.f)[Label(T("PASADA LIMPIA"), 11, Prog.bCleanRun ? PogoColors::Gold : FLinearColor(0.3f, 0.3f, 0.34f))];
			MarksRow->AddSlot().AutoWidth().Padding(14.f, 0.f, 0.f, 0.f)[Label(T("LÍNEA ALTA"), 11, Prog.bHighLine ? PogoColors::Gold : FLinearColor(0.3f, 0.3f, 0.34f))];
		}

		Cards->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 10.f)
		[
			SNew(SBox).WidthOverride(600.f)
			[
				SNew(SButton)
				.ButtonStyle(&ButtonStyle(2))
				.IsEnabled(bUnlocked)
				.ContentPadding(FMargin(22.f, 14.f))
				.OnHovered_Lambda([PC]() { if (PC.IsValid()) { PogoArt::PlaySfx(PC.Get(), TEXT("SFX_UIMove"), 0.35f); } })
				.OnClicked_Lambda([PC, i]()
				{
					Click(PC);
					if (PC.IsValid())
					{
						PC->PlayCircuit(i);
					}
					return FReply::Handled();
				})
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[Label(Info.Number, 11, PogoColors::BlueLight)]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)
						[
							SNew(STextBlock).Text(Info.Name).Font(Font("Bold", 24)).ColorAndOpacity(bUnlocked ? PogoColors::Cream : PogoColors::Gray)
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom).Padding(14.f, 0.f, 0.f, 3.f)
						[
							SNew(STextBlock).Text(Info.Tagline).Font(Font("Regular", 14)).ColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.75f))
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f, 0.f, 6.f)
					[
						SNew(STextBlock).Text(Info.Description).AutoWrapText(true).Font(Font("Regular", 12)).ColorAndOpacity(FLinearColor(0.62f, 0.63f, 0.68f))
					]
					+ SVerticalBox::Slot().AutoHeight()[MarksRow]
				]
			]
		];
	}

	TSharedRef<SWidget> Content = SNew(SOverlay)
	+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Fill)
	[
		SNew(SBox).WidthOverride(760.f)
		[
			SNew(SBorder).BorderImage(Brush(FLinearColor(0.004f, 0.007f, 0.02f, 0.78f))).Padding(FMargin(64.f, 52.f, 40.f, 40.f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[Label(T("PARKOUR EN PRIMERA PERSONA  /  UN JUGADOR"), 11, PogoColors::BlueLight)]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)
				[
					SNew(STextBlock).Text(T("NO SLEEP")).Font(Font("Bold", 72, -10)).ColorAndOpacity(PogoColors::Cream)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, -18.f, 0.f, 0.f)
				[
					SNew(STextBlock).Text(T("POGO")).Font(Font("Bold", 72, -10)).ColorAndOpacity(FLinearColor(0.2f, 0.42f, 1.f))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 26.f)
				[
					SNew(STextBlock).Text(T("De la vereda a la azotea")).Font(Font("Italic", 19)).ColorAndOpacity(FLinearColor(0.8f, 0.78f, 0.74f))
				]
				+ SVerticalBox::Slot().AutoHeight()[Cards]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 0.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 10.f, 0.f)
					[
						Button(PC, T("OPCIONES"), [PC]() { if (PC.IsValid()) { PC->ShowOptions(false); } }, 0, 200.f)
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						Button(PC, T("SALIR"), [PC]() { if (PC.IsValid()) { PC->QuitGame(); } }, 0, 160.f)
					]
				]
				+ SVerticalBox::Slot().FillHeight(1.f)[SNew(SSpacer)]
				+ SVerticalBox::Slot().AutoHeight()
				[
					Label(T("AVANCE  /  BASTA LLEGAR A LA META. EL TIEMPO Y LAS MARCAS SON DESAFÍOS OPCIONALES."), 10, FLinearColor(0.55f, 0.56f, 0.62f))
				]
			]
		]
	]
	+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(40.f, 34.f)
	[
		Panel(Label(T("ESPACIO / CLIC  cargar y saltar   ·   WASD  dirección   ·   MOUSE  mirar   ·   ESC  pausa"), 11, PogoColors::Cream, "Mono", 60))
	];

	return Screen(Content, [](const FKey&) { return false; });
}

// =============================================================================================== pausa
TSharedRef<SWidget> PogoUI::MakePause(APogoPlayerController* InPC)
{
	FPCWeak PC(InPC);
	const APogoGameMode* GM = InPC->GetPogoGameMode();
	const FPogoCircuitInfo& Info = PogoCircuits::Get(GM ? GM->GetCircuitIndex() : 0);

	TSharedRef<SWidget> Content = SNew(SBorder).BorderImage(Brush(FLinearColor(0.f, 0.f, 0.01f, 0.55f))).HAlign(HAlign_Center).VAlign(VAlign_Center)
	[
		Panel(
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[Label(FText::Format(T("{0}  /  {1}"), Info.Number, Info.Name), 12, PogoColors::BlueLight)]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f, 0.f, 20.f)[SNew(STextBlock).Text(T("PAUSA")).Font(Font("Bold", 52)).ColorAndOpacity(PogoColors::Cream)]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)[Button(PC, T("CONTINUAR"), [PC]() { if (PC.IsValid()) { PC->Resume(); } }, 1)]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)[Button(PC, T("REINICIAR CIRCUITO"), [PC]() { if (PC.IsValid()) { PC->RestartCircuit(); } })]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)[Button(PC, T("OPCIONES"), [PC]() { if (PC.IsValid()) { PC->ShowOptions(true); } })]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)[Button(PC, T("SALIR AL MENÚ"), [PC]() { if (PC.IsValid()) { PC->GoToMenu(); } })]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 16.f, 0.f, 0.f)[Label(T("EL RELOJ ESTÁ DETENIDO  ·  EN JUEGO, R VUELVE AL ÚLTIMO PUNTO"), 10, PogoColors::Gray)],
			FMargin(44.f, 34.f))
	];
	return Screen(Content, [PC](const FKey& K)
	{
		if ((K == EKeys::Escape || K == EKeys::P || K == EKeys::Gamepad_Special_Right) && PC.IsValid())
		{
			PC->Resume();
			return true;
		}
		return false;
	});
}

// =============================================================================================== opciones
TSharedRef<SWidget> PogoUI::MakeOptions(APogoPlayerController* InPC, TFunction<void()> OnBack)
{
	FPCWeak PC(InPC);
	auto GI = [PC]() -> UPogoGameInstance* { return PC.IsValid() ? UPogoGameInstance::Get(PC.Get()) : nullptr; };
	auto Apply = [PC, GI]()
	{
		if (UPogoGameInstance* G = GI())
		{
			G->ApplySettings();
		}
		if (PC.IsValid())
		{
			if (APogoGameMode* M = PC->GetPogoGameMode())
			{
				M->ApplyAudioVolumes();
			}
		}
	};

	TSharedRef<SVerticalBox> Rows = SNew(SVerticalBox);
	auto AddSlider = [&](const FText& Name, float Min, float Max, TFunction<float&(FPogoSettings&)> Field, TFunction<FText(float)> Fmt)
	{
		Rows->AddSlot().AutoHeight().Padding(0.f, 6.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(0.42f).VAlign(VAlign_Center)[Label(Name, 12)]
			+ SHorizontalBox::Slot().FillWidth(0.42f).VAlign(VAlign_Center).Padding(10.f, 0.f)
			[
				SNew(SSlider)
				.Value_Lambda([GI, Field, Min, Max]() { UPogoGameInstance* G = GI(); return G ? (Field(G->Settings) - Min) / (Max - Min) : 0.f; })
				.OnValueChanged_Lambda([GI, Field, Min, Max, Apply](float V) { if (UPogoGameInstance* G = GI()) { Field(G->Settings) = Min + V * (Max - Min); Apply(); } })
				.SliderBarColor(FLinearColor(0.2f, 0.22f, 0.3f))
				.SliderHandleColor(FLinearColor(0.15f, 0.38f, 1.f))
			]
			+ SHorizontalBox::Slot().FillWidth(0.16f).VAlign(VAlign_Center).HAlign(HAlign_Right)
			[
				SNew(STextBlock).Font(Font("Mono", 12)).ColorAndOpacity(PogoColors::BlueLight)
				.Text_Lambda([GI, Field, Fmt]() { UPogoGameInstance* G = GI(); return G ? Fmt(Field(G->Settings)) : FText::GetEmpty(); })
			]
		];
	};
	auto AddCheck = [&](const FText& Name, TFunction<bool&(FPogoSettings&)> Field)
	{
		Rows->AddSlot().AutoHeight().Padding(0.f, 6.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(0.84f).VAlign(VAlign_Center)[Label(Name, 12)]
			+ SHorizontalBox::Slot().FillWidth(0.16f).VAlign(VAlign_Center).HAlign(HAlign_Right)
			[
				SNew(SCheckBox)
				.IsChecked_Lambda([GI, Field]() { UPogoGameInstance* G = GI(); return G && Field(G->Settings) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([GI, Field, Apply, PC](ECheckBoxState S) { if (UPogoGameInstance* G = GI()) { Field(G->Settings) = S == ECheckBoxState::Checked; Apply(); Click(PC); } })
			]
		];
	};
	auto Pct = [](float V) { return FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(V * 100.f))); };

	AddSlider(T("CAMPO DE VISIÓN"), 70.f, 115.f, [](FPogoSettings& S) -> float& { return S.FOV; }, [](float V) { return FText::FromString(FString::Printf(TEXT("%d°"), FMath::RoundToInt(V))); });
	AddSlider(T("BALANCEO DE CÁMARA"), 0.f, 1.f, [](FPogoSettings& S) -> float& { return S.Sway; }, Pct);
	AddSlider(T("SENSIBILIDAD DEL MOUSE"), 0.2f, 3.f, [](FPogoSettings& S) -> float& { return S.MouseSensitivity; }, [](float V) { return FText::FromString(FString::Printf(TEXT("%.1f"), V)); });
	AddSlider(T("RESOLUCIÓN INTERNA (PIXELADO PS1)"), 25.f, 100.f, [](FPogoSettings& S) -> float& { return S.PixelScale; }, [](float V) { return FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(V))); });
	AddSlider(T("VOLUMEN MÚSICA"), 0.f, 1.f, [](FPogoSettings& S) -> float& { return S.MusicVolume; }, Pct);
	AddSlider(T("VOLUMEN EFECTOS"), 0.f, 1.f, [](FPogoSettings& S) -> float& { return S.SfxVolume; }, Pct);
	AddCheck(T("MOSTRAR CRONÓMETRO"), [](FPogoSettings& S) -> bool& { return S.bShowTimer; });
	AddCheck(T("ANILLO DE CAÍDA PREVISTA"), [](FPogoSettings& S) -> bool& { return S.bShowRing; });
	AddCheck(T("INVERTIR EJE VERTICAL"), [](FPogoSettings& S) -> bool& { return S.bInvertY; });

	TSharedRef<bool> bConfirm = MakeShared<bool>(false);
	TSharedRef<SWidget> Content = SNew(SBorder).BorderImage(Brush(FLinearColor(0.f, 0.f, 0.01f, 0.6f))).HAlign(HAlign_Center).VAlign(VAlign_Center)
	[
		SNew(SBox).WidthOverride(760.f)
		[
			Panel(
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 16.f)[SNew(STextBlock).Text(T("OPCIONES")).Font(Font("Bold", 40)).ColorAndOpacity(PogoColors::Cream)]
				+ SVerticalBox::Slot().AutoHeight()[Rows]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 22.f, 0.f, 0.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()[Button(PC, T("VOLVER"), [OnBack]() { OnBack(); }, 1, 220.f)]
					+ SHorizontalBox::Slot().FillWidth(1.f)[SNew(SSpacer)]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SButton)
						.ButtonStyle(&ButtonStyle(0))
						.ContentPadding(FMargin(18.f, 13.f))
						.OnClicked_Lambda([PC, GI, bConfirm]()
						{
							Click(PC);
							if (!*bConfirm)
							{
								*bConfirm = true;
							}
							else if (UPogoGameInstance* G = GI())
							{
								G->ResetProgress();
								*bConfirm = false;
							}
							return FReply::Handled();
						})
						[
							SNew(STextBlock).Font(Font("Bold", 13, 60)).ColorAndOpacity(FLinearColor(0.9f, 0.45f, 0.4f))
							.Text_Lambda([bConfirm]() { return *bConfirm ? T("¿SEGURO? CLIC DE NUEVO") : T("BORRAR PROGRESO"); })
						]
					]
				],
				FMargin(44.f, 34.f))
		]
	];
	return Screen(Content, [OnBack](const FKey& K)
	{
		if (K == EKeys::Escape || K == EKeys::Gamepad_FaceButton_Right)
		{
			OnBack();
			return true;
		}
		return false;
	});
}

// =============================================================================================== resultados
TSharedRef<SWidget> PogoUI::MakeResults(APogoPlayerController* InPC, const FPogoRunResult& R)
{
	FPCWeak PC(InPC);
	const FPogoCircuitInfo& Info = PogoCircuits::Get(R.Circuit);
	const bool bHasNext = R.Circuit >= 0 && R.Circuit + 1 < PogoCircuits::Num;
	const UPogoGameInstance* GI = UPogoGameInstance::Get(InPC);
	const bool bNextUnlocked = bHasNext && GI && GI->IsUnlocked(R.Circuit + 1);

	auto Row = [](const FText& Name, const FText& Value, const FLinearColor& Color)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f)[Label(Name, 13, FLinearColor(0.7f, 0.7f, 0.76f))]
			+ SHorizontalBox::Slot().AutoWidth()[Label(Value, 15, Color)];
	};

	FText Extra = FText::GetEmpty();
	if (!bHasNext && R.Circuit >= 0)
	{
		Extra = T("COMPLETASTE LA SERIE DE CIRCUITOS");
	}
	else if (R.bUnlockedNext)
	{
		Extra = FText::Format(T("SE ABRIÓ: {0}"), PogoCircuits::Get(R.Circuit + 1).Name);
	}

	TSharedRef<SHorizontalBox> Buttons = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 10.f, 0.f)[Button(PC, T("REPETIR"), [PC]() { if (PC.IsValid()) { PC->RestartCircuit(); } }, bNextUnlocked ? 0 : 1, 200.f)];
	if (bNextUnlocked)
	{
		const int32 Next = R.Circuit + 1;
		Buttons->AddSlot().AutoWidth().Padding(0.f, 0.f, 10.f, 0.f)[Button(PC, T("SIGUIENTE CIRCUITO"), [PC, Next]() { if (PC.IsValid()) { PC->PlayCircuit(Next); } }, 1, 280.f)];
	}
	Buttons->AddSlot().AutoWidth()[Button(PC, T("MENÚ"), [PC]() { if (PC.IsValid()) { PC->GoToMenu(); } }, 0, 160.f)];

	TSharedRef<SWidget> Content = SNew(SBorder).BorderImage(Brush(FLinearColor(0.f, 0.f, 0.01f, 0.5f))).HAlign(HAlign_Center).VAlign(VAlign_Center)
	[
		SNew(SBox).WidthOverride(720.f)
		[
			Panel(
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[Label(FText::Format(T("{0}  /  {1}"), Info.Number, Info.Name), 12, PogoColors::BlueLight)]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f, 0.f, 0.f)
				[
					SNew(STextBlock).Text(FText::Format(T("{0} ALCANZADA"), Info.GoalName)).Font(Font("Bold", 44)).ColorAndOpacity(PogoColors::Cream)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 18.f)
				[
					SNew(STextBlock).Text(PogoFormat::Time(R.Time)).Font(Font("Mono", 64, 40)).ColorAndOpacity(R.bNewRecord ? PogoColors::Gold : FLinearColor(0.2f, 0.45f, 1.f))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)
				[
					Row(T("RÉCORD PERSONAL"), R.bNewRecord ? FText::Format(T("{0}  ¡NUEVO!"), PogoFormat::Time(R.Best)) : PogoFormat::Time(R.Best), R.bNewRecord ? PogoColors::Gold : PogoColors::Cream)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)[Row(T("CAÍDAS"), FText::AsNumber(R.Falls), PogoColors::Cream)]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)[Row(T("PASADA LIMPIA"), R.bCleanRun ? T("SÍ") : T("NO"), R.bCleanRun ? PogoColors::Gold : PogoColors::Gray)]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)[Row(T("LÍNEA ALTA"), R.bHighLine ? T("COMPLETA") : T("INCOMPLETA"), R.bHighLine ? PogoColors::Gold : PogoColors::Gray)]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 16.f, 0.f, 0.f)[Label(Extra, 12, PogoColors::BlueLight)]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 22.f, 0.f, 0.f)[Buttons],
				FMargin(44.f, 34.f))
		]
	];
	return Screen(Content, [PC, bNextUnlocked, R](const FKey& K)
	{
		if (!PC.IsValid())
		{
			return false;
		}
		if (K == EKeys::R)
		{
			PC->RestartCircuit();
			return true;
		}
		if ((K == EKeys::Enter || K == EKeys::N) && bNextUnlocked)
		{
			PC->PlayCircuit(R.Circuit + 1);
			return true;
		}
		if (K == EKeys::Escape || K == EKeys::M)
		{
			PC->GoToMenu();
			return true;
		}
		return false;
	});
}

#undef T

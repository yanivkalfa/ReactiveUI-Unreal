// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "UetkxInterpElements.h"

#include "RuiCoreElements.h"
#include "RuiSlateElements.h"
#include "UetkxExprVm.h"

namespace
{
	struct FInterpElementRegistry
	{
		TMap<FName, FUetkxInterpElementBuilder> Builders;
		FCriticalSection Lock;

		static FInterpElementRegistry& Get()
		{
			static FInterpElementRegistry Instance;
			return Instance;
		}
	};

	FText AsText(const FRuiValue& Value)
	{
		if (Value.Kind == FRuiValue::EKind::Text)
		{
			return Value.TextValue;
		}
		return FText::FromString(FUetkxExprVm::ToDisplayString(Value));
	}

	FName AsName(const FRuiValue& Value)
	{
		if (Value.Kind == FRuiValue::EKind::Name)
		{
			return Value.NameValue;
		}
		return FName(*FUetkxExprVm::ToDisplayString(Value));
	}

	float AsFloat(const FRuiValue& Value)
	{
		switch (Value.Kind)
		{
		case FRuiValue::EKind::Float:
			return static_cast<float>(Value.FloatValue);
		case FRuiValue::EKind::Int:
			return static_cast<float>(Value.IntValue);
		default:
			return 0.0f;
		}
	}

	bool AsBool(const FRuiValue& Value)
	{
		return FUetkxExprVm::Truthy(Value);
	}

	FVector2D AsVector2(const FRuiValue& Value)
	{
		return Value.Kind == FRuiValue::EKind::Vector2 ? Value.Vector2Value : FVector2D::ZeroVector;
	}

	FLinearColor AsColor(const FRuiValue& Value)
	{
		return Value.Kind == FRuiValue::EKind::Color ? Value.ColorValue : FLinearColor::White;
	}

	/** "12" -> uniform; "12,4" -> (H, V); "l,t,r,b" -> full — the same shapes the codegen's
	 *  FMargin(...) string lowering produces. */
	FMargin AsMargin(const FRuiValue& Value)
	{
		if (Value.Kind == FRuiValue::EKind::Float || Value.Kind == FRuiValue::EKind::Int)
		{
			return FMargin(AsFloat(Value));
		}
		TArray<FString> Parts;
		FUetkxExprVm::ToDisplayString(Value).ParseIntoArray(Parts, TEXT(","), true);
		switch (Parts.Num())
		{
		case 1:
			return FMargin(FCString::Atof(*Parts[0]));
		case 2:
			return FMargin(FCString::Atof(*Parts[0]), FCString::Atof(*Parts[1]));
		case 4:
			return FMargin(FCString::Atof(*Parts[0]), FCString::Atof(*Parts[1]), FCString::Atof(*Parts[2]),
						   FCString::Atof(*Parts[3]));
		default:
			return FMargin();
		}
	}

	template <typename TProps> void ApplyBase(TProps& Props, FUetkxInterpElementInputs& In)
	{
		if (In.Style.IsValid() && !In.Style->IsEmpty())
		{
			Props.Style = In.Style;
		}
		if (In.SlotProps.IsValid() && !In.SlotProps->IsEmpty())
		{
			Props.SlotProps = In.SlotProps;
		}
		Props.Classes = MoveTemp(In.Classes);
	}

	const FRuiValue* Attr(const FUetkxInterpElementInputs& In, const TCHAR* Name)
	{
		return In.Attrs.Find(FName(Name));
	}
	const FRuiCallback* Event(const FUetkxInterpElementInputs& In, const TCHAR* Name)
	{
		return In.Events.Find(FName(Name));
	}
} // namespace

const TSet<FString>& FUetkxInterpElements::StyleKeys()
{
	static const TSet<FString> Keys = {
		TEXT("RenderOpacity"),		  TEXT("Visibility"),	   TEXT("Enabled"),
		TEXT("RenderTranslation"),	  TEXT("RenderScale"),	   TEXT("RenderTransformAngle"),
		TEXT("RenderTransformPivot"), TEXT("ColorAndOpacity"), TEXT("Font.Size"),
		TEXT("Justification"),		  TEXT("AutoWrapText"),	   TEXT("FillColorAndOpacity")};
	return Keys;
}

void FUetkxInterpElements::Register(FName Tag, FUetkxInterpElementBuilder Builder)
{
	FInterpElementRegistry& Reg = FInterpElementRegistry::Get();
	FScopeLock Guard(&Reg.Lock);
	Reg.Builders.Add(Tag, MoveTemp(Builder));
}

bool FUetkxInterpElements::Has(FName Tag)
{
	FInterpElementRegistry& Reg = FInterpElementRegistry::Get();
	FScopeLock Guard(&Reg.Lock);
	return Reg.Builders.Contains(Tag);
}

FRuiNode FUetkxInterpElements::Build(FName Tag, FUetkxInterpElementInputs&& Inputs)
{
	FUetkxInterpElementBuilder Builder;
	{
		FInterpElementRegistry& Reg = FInterpElementRegistry::Get();
		FScopeLock Guard(&Reg.Lock);
		if (const FUetkxInterpElementBuilder* Found = Reg.Builders.Find(Tag))
		{
			Builder = *Found;
		}
	}
	return Builder ? Builder(MoveTemp(Inputs)) : RUI::Fragment({});
}

void FUetkxInterpElements::RegisterBuiltins()
{
	static bool bOnce = false;
	if (bOnce)
	{
		return;
	}
	bOnce = true;

	Register(TEXT("TextBlock"),
			 [](FUetkxInterpElementInputs&& In)
			 {
				 const FRuiValue* Text = Attr(In, TEXT("Text"));
				 FRuiNode Node = RUI::TextBlock(Text ? AsText(*Text) : FText::GetEmpty(), In.Key);
				 const bool bStyled = (In.Style.IsValid() && !In.Style->IsEmpty()) ||
									  (In.SlotProps.IsValid() && !In.SlotProps->IsEmpty()) || In.Classes.Num() > 0;
				 if (bStyled)
				 {
					 TSharedRef<FRuiTextBlockProps> Props =
						 MakeShared<FRuiTextBlockProps>(static_cast<const FRuiTextBlockProps&>(*Node.Props));
					 ApplyBase(*Props, In);
					 Node.Props = Props;
				 }
				 return Node;
			 });

	Register(TEXT("VerticalBox"),
			 [](FUetkxInterpElementInputs&& In)
			 {
				 FRuiVerticalBoxProps P;
				 ApplyBase(P, In);
				 return RUI::Slate::VerticalBox(MoveTemp(P), MoveTemp(In.Children), In.Key);
			 });

	Register(TEXT("HorizontalBox"),
			 [](FUetkxInterpElementInputs&& In)
			 {
				 FRuiHorizontalBoxProps P;
				 ApplyBase(P, In);
				 return RUI::Slate::HorizontalBox(MoveTemp(P), MoveTemp(In.Children), In.Key);
			 });

	Register(TEXT("Overlay"),
			 [](FUetkxInterpElementInputs&& In)
			 {
				 FRuiOverlayProps P;
				 ApplyBase(P, In);
				 return RUI::Slate::Overlay(MoveTemp(P), MoveTemp(In.Children), In.Key);
			 });

	Register(TEXT("Button"),
			 [](FUetkxInterpElementInputs&& In)
			 {
				 FRuiButtonProps P;
				 if (const FRuiCallback* Clicked = Event(In, TEXT("OnClicked")))
				 {
					 P.SetOnClicked(*Clicked);
				 }
				 if (const FRuiValue* Enabled = Attr(In, TEXT("bEnabled")))
				 {
					 P.SetbEnabled(AsBool(*Enabled));
				 }
				 if (const FRuiValue* Padding = Attr(In, TEXT("ContentPadding")))
				 {
					 P.SetContentPadding(AsMargin(*Padding));
				 }
				 ApplyBase(P, In);
				 return RUI::Slate::Button(MoveTemp(P), MoveTemp(In.Children), In.Key);
			 });

	Register(TEXT("Border"),
			 [](FUetkxInterpElementInputs&& In)
			 {
				 FRuiBorderProps P;
				 if (const FRuiValue* Padding = Attr(In, TEXT("Padding")))
				 {
					 P.SetPadding(AsMargin(*Padding));
				 }
				 if (const FRuiValue* Color = Attr(In, TEXT("BorderBackgroundColor")))
				 {
					 P.SetBorderBackgroundColor(AsColor(*Color));
				 }
				 if (const FRuiValue* H = Attr(In, TEXT("HAlign")))
				 {
					 P.SetHAlign(AsName(*H));
				 }
				 if (const FRuiValue* V = Attr(In, TEXT("VAlign")))
				 {
					 P.SetVAlign(AsName(*V));
				 }
				 if (const FRuiValue* Image = Attr(In, TEXT("BorderImage")))
				 {
					 P.SetBorderImage(AsName(*Image));
				 }
				 ApplyBase(P, In);
				 return RUI::Slate::Border(MoveTemp(P), MoveTemp(In.Children), In.Key);
			 });

	Register(TEXT("Box"),
			 [](FUetkxInterpElementInputs&& In)
			 {
				 FRuiBoxProps P;
				 if (const FRuiValue* W = Attr(In, TEXT("WidthOverride")))
				 {
					 P.SetWidthOverride(AsFloat(*W));
				 }
				 if (const FRuiValue* H = Attr(In, TEXT("HeightOverride")))
				 {
					 P.SetHeightOverride(AsFloat(*H));
				 }
				 if (const FRuiValue* MinW = Attr(In, TEXT("MinDesiredWidth")))
				 {
					 P.SetMinDesiredWidth(AsFloat(*MinW));
				 }
				 if (const FRuiValue* MinH = Attr(In, TEXT("MinDesiredHeight")))
				 {
					 P.SetMinDesiredHeight(AsFloat(*MinH));
				 }
				 if (const FRuiValue* MaxW = Attr(In, TEXT("MaxDesiredWidth")))
				 {
					 P.SetMaxDesiredWidth(AsFloat(*MaxW));
				 }
				 if (const FRuiValue* MaxH = Attr(In, TEXT("MaxDesiredHeight")))
				 {
					 P.SetMaxDesiredHeight(AsFloat(*MaxH));
				 }
				 if (const FRuiValue* H = Attr(In, TEXT("HAlign")))
				 {
					 P.SetHAlign(AsName(*H));
				 }
				 if (const FRuiValue* V = Attr(In, TEXT("VAlign")))
				 {
					 P.SetVAlign(AsName(*V));
				 }
				 ApplyBase(P, In);
				 return RUI::Slate::Box(MoveTemp(P), MoveTemp(In.Children), In.Key);
			 });

	Register(TEXT("Image"),
			 [](FUetkxInterpElementInputs&& In)
			 {
				 FRuiImageProps P;
				 if (const FRuiValue* Color = Attr(In, TEXT("ColorAndOpacity")))
				 {
					 P.SetColorAndOpacity(AsColor(*Color));
				 }
				 if (const FRuiValue* Size = Attr(In, TEXT("DesiredSizeOverride")))
				 {
					 P.SetDesiredSizeOverride(AsVector2(*Size));
				 }
				 ApplyBase(P, In);
				 return RUI::Slate::Image(MoveTemp(P), In.Key);
			 });

	Register(TEXT("ScrollBox"),
			 [](FUetkxInterpElementInputs&& In)
			 {
				 FRuiScrollBoxProps P;
				 if (const FRuiValue* Orientation = Attr(In, TEXT("Orientation")))
				 {
					 P.SetOrientation(AsName(*Orientation));
				 }
				 ApplyBase(P, In);
				 return RUI::Slate::ScrollBox(MoveTemp(P), MoveTemp(In.Children), In.Key);
			 });

	Register(TEXT("Spacer"),
			 [](FUetkxInterpElementInputs&& In)
			 {
				 FRuiSpacerProps P;
				 if (const FRuiValue* Size = Attr(In, TEXT("Size")))
				 {
					 P.SetSize(AsVector2(*Size));
				 }
				 ApplyBase(P, In);
				 return RUI::Slate::Spacer(MoveTemp(P), In.Key);
			 });

	Register(TEXT("EditableTextBox"),
			 [](FUetkxInterpElementInputs&& In)
			 {
				 FRuiEditableTextBoxProps P;
				 if (const FRuiValue* Text = Attr(In, TEXT("Text")))
				 {
					 P.SetText(AsText(*Text));
				 }
				 if (const FRuiValue* Hint = Attr(In, TEXT("HintText")))
				 {
					 P.SetHintText(AsText(*Hint));
				 }
				 if (const FRuiValue* ReadOnly = Attr(In, TEXT("bIsReadOnly")))
				 {
					 P.SetbIsReadOnly(AsBool(*ReadOnly));
				 }
				 if (const FRuiCallback* Changed = Event(In, TEXT("OnTextChanged")))
				 {
					 P.SetOnTextChanged(*Changed);
				 }
				 if (const FRuiCallback* Committed = Event(In, TEXT("OnTextCommitted")))
				 {
					 P.SetOnTextCommitted(*Committed);
				 }
				 ApplyBase(P, In);
				 return RUI::Slate::EditableTextBox(MoveTemp(P), In.Key);
			 });

	Register(TEXT("CheckBox"),
			 [](FUetkxInterpElementInputs&& In)
			 {
				 FRuiCheckBoxProps P;
				 if (const FRuiValue* Checked = Attr(In, TEXT("bIsChecked")))
				 {
					 P.SetbIsChecked(AsBool(*Checked));
				 }
				 if (const FRuiCallback* Changed = Event(In, TEXT("OnCheckStateChanged")))
				 {
					 P.SetOnCheckStateChanged(*Changed);
				 }
				 ApplyBase(P, In);
				 return RUI::Slate::CheckBox(MoveTemp(P), MoveTemp(In.Children), In.Key);
			 });

	Register(TEXT("Slider"),
			 [](FUetkxInterpElementInputs&& In)
			 {
				 FRuiSliderProps P;
				 if (const FRuiValue* Value = Attr(In, TEXT("Value")))
				 {
					 P.SetValue(AsFloat(*Value));
				 }
				 if (const FRuiValue* Min = Attr(In, TEXT("MinValue")))
				 {
					 P.SetMinValue(AsFloat(*Min));
				 }
				 if (const FRuiValue* Max = Attr(In, TEXT("MaxValue")))
				 {
					 P.SetMaxValue(AsFloat(*Max));
				 }
				 if (const FRuiValue* Step = Attr(In, TEXT("StepSize")))
				 {
					 P.SetStepSize(AsFloat(*Step));
				 }
				 if (const FRuiCallback* Changed = Event(In, TEXT("OnValueChanged")))
				 {
					 P.SetOnValueChanged(*Changed);
				 }
				 ApplyBase(P, In);
				 return RUI::Slate::Slider(MoveTemp(P), In.Key);
			 });

	Register(TEXT("ProgressBar"),
			 [](FUetkxInterpElementInputs&& In)
			 {
				 FRuiProgressBarProps P;
				 if (const FRuiValue* Percent = Attr(In, TEXT("Percent")))
				 {
					 P.SetPercent(AsFloat(*Percent));
				 }
				 ApplyBase(P, In);
				 return RUI::Slate::ProgressBar(MoveTemp(P), In.Key);
			 });

	Register(TEXT("RuiCanvas"),
			 [](FUetkxInterpElementInputs&& In)
			 {
				 FRuiCanvasProps P;
				 if (const FRuiValue* Redraw = Attr(In, TEXT("RedrawKey")))
				 {
					 P.SetRedrawKey(static_cast<int64>(AsFloat(*Redraw)));
				 }
				 if (const FRuiValue* Size = Attr(In, TEXT("CanvasSize")))
				 {
					 P.SetCanvasSize(AsVector2(*Size));
				 }
				 // DrawFn is identity-typed (TSharedPtr) — not representable in FRuiValue; interpreted
				 // canvases render empty until the next real compile (recorded fallback).
				 ApplyBase(P, In);
				 return RUI::Slate::RuiCanvas(MoveTemp(P), In.Key);
			 });

	// ── Batch 2 (Phase 7 step 8) — the everyday game set ───────────────────────────────────

	Register(TEXT("WidgetSwitcher"),
			 [](FUetkxInterpElementInputs&& In)
			 {
				 FRuiWidgetSwitcherProps P;
				 if (const FRuiValue* Index = Attr(In, TEXT("WidgetIndex")))
				 {
					 P.SetWidgetIndex(static_cast<int32>(AsFloat(*Index)));
				 }
				 ApplyBase(P, In);
				 return RUI::Slate::WidgetSwitcher(MoveTemp(P), MoveTemp(In.Children), In.Key);
			 });

	Register(TEXT("ScaleBox"),
			 [](FUetkxInterpElementInputs&& In)
			 {
				 FRuiScaleBoxProps P;
				 if (const FRuiValue* Stretch = Attr(In, TEXT("Stretch")))
				 {
					 P.SetStretch(AsName(*Stretch));
				 }
				 if (const FRuiValue* Dir = Attr(In, TEXT("StretchDirection")))
				 {
					 P.SetStretchDirection(AsName(*Dir));
				 }
				 ApplyBase(P, In);
				 return RUI::Slate::ScaleBox(MoveTemp(P), MoveTemp(In.Children), In.Key);
			 });

	Register(TEXT("Throbber"),
			 [](FUetkxInterpElementInputs&& In)
			 {
				 FRuiThrobberProps P;
				 if (const FRuiValue* Num = Attr(In, TEXT("NumPieces")))
				 {
					 P.SetNumPieces(static_cast<int32>(AsFloat(*Num)));
				 }
				 if (const FRuiValue* Animate = Attr(In, TEXT("Animate")))
				 {
					 P.SetAnimate(AsName(*Animate));
				 }
				 ApplyBase(P, In);
				 return RUI::Slate::Throbber(MoveTemp(P), In.Key);
			 });

	Register(TEXT("WrapBox"),
			 [](FUetkxInterpElementInputs&& In)
			 {
				 FRuiWrapBoxProps P;
				 if (const FRuiValue* Orientation = Attr(In, TEXT("Orientation")))
				 {
					 P.SetOrientation(AsName(*Orientation));
				 }
				 if (const FRuiValue* WrapSize = Attr(In, TEXT("WrapSize")))
				 {
					 P.SetWrapSize(AsFloat(*WrapSize));
				 }
				 if (const FRuiValue* Padding = Attr(In, TEXT("InnerSlotPadding")))
				 {
					 P.SetInnerSlotPadding(AsVector2(*Padding));
				 }
				 if (const FRuiValue* UseAllotted = Attr(In, TEXT("bUseAllottedSize")))
				 {
					 P.SetbUseAllottedSize(AsBool(*UseAllotted));
				 }
				 ApplyBase(P, In);
				 return RUI::Slate::WrapBox(MoveTemp(P), MoveTemp(In.Children), In.Key);
			 });

	Register(TEXT("MultiLineEditableTextBox"),
			 [](FUetkxInterpElementInputs&& In)
			 {
				 FRuiMultiLineEditableTextBoxProps P;
				 if (const FRuiValue* Text = Attr(In, TEXT("Text")))
				 {
					 P.SetText(AsText(*Text));
				 }
				 if (const FRuiValue* Hint = Attr(In, TEXT("HintText")))
				 {
					 P.SetHintText(AsText(*Hint));
				 }
				 if (const FRuiValue* ReadOnly = Attr(In, TEXT("bIsReadOnly")))
				 {
					 P.SetbIsReadOnly(AsBool(*ReadOnly));
				 }
				 if (const FRuiCallback* Changed = Event(In, TEXT("OnTextChanged")))
				 {
					 P.SetOnTextChanged(*Changed);
				 }
				 if (const FRuiCallback* Committed = Event(In, TEXT("OnTextCommitted")))
				 {
					 P.SetOnTextCommitted(*Committed);
				 }
				 ApplyBase(P, In);
				 return RUI::Slate::MultiLineEditableTextBox(MoveTemp(P), In.Key);
			 });

	Register(TEXT("SearchBox"),
			 [](FUetkxInterpElementInputs&& In)
			 {
				 FRuiSearchBoxProps P;
				 if (const FRuiValue* Text = Attr(In, TEXT("Text")))
				 {
					 P.SetText(AsText(*Text));
				 }
				 if (const FRuiValue* Hint = Attr(In, TEXT("HintText")))
				 {
					 P.SetHintText(AsText(*Hint));
				 }
				 if (const FRuiCallback* Changed = Event(In, TEXT("OnTextChanged")))
				 {
					 P.SetOnTextChanged(*Changed);
				 }
				 if (const FRuiCallback* Committed = Event(In, TEXT("OnTextCommitted")))
				 {
					 P.SetOnTextCommitted(*Committed);
				 }
				 ApplyBase(P, In);
				 return RUI::Slate::SearchBox(MoveTemp(P), In.Key);
			 });

	Register(TEXT("SafeZone"),
			 [](FUetkxInterpElementInputs&& In)
			 {
				 FRuiSafeZoneProps P;
				 if (const FRuiValue* Title = Attr(In, TEXT("bIsTitleSafe")))
				 {
					 P.SetbIsTitleSafe(AsBool(*Title));
				 }
				 if (const FRuiValue* L = Attr(In, TEXT("bPadLeft")))
				 {
					 P.SetbPadLeft(AsBool(*L));
				 }
				 if (const FRuiValue* R = Attr(In, TEXT("bPadRight")))
				 {
					 P.SetbPadRight(AsBool(*R));
				 }
				 if (const FRuiValue* T = Attr(In, TEXT("bPadTop")))
				 {
					 P.SetbPadTop(AsBool(*T));
				 }
				 if (const FRuiValue* B = Attr(In, TEXT("bPadBottom")))
				 {
					 P.SetbPadBottom(AsBool(*B));
				 }
				 ApplyBase(P, In);
				 return RUI::Slate::SafeZone(MoveTemp(P), MoveTemp(In.Children), In.Key);
			 });

	Register(TEXT("DPIScaler"),
			 [](FUetkxInterpElementInputs&& In)
			 {
				 FRuiDPIScalerProps P;
				 if (const FRuiValue* Scale = Attr(In, TEXT("DPIScale")))
				 {
					 P.SetDPIScale(AsFloat(*Scale));
				 }
				 ApplyBase(P, In);
				 return RUI::Slate::DPIScaler(MoveTemp(P), MoveTemp(In.Children), In.Key);
			 });

	Register(TEXT("Separator"),
			 [](FUetkxInterpElementInputs&& In)
			 {
				 FRuiSeparatorProps P;
				 if (const FRuiValue* Orientation = Attr(In, TEXT("Orientation")))
				 {
					 P.SetOrientation(AsName(*Orientation));
				 }
				 if (const FRuiValue* Thickness = Attr(In, TEXT("Thickness")))
				 {
					 P.SetThickness(AsFloat(*Thickness));
				 }
				 if (const FRuiValue* Color = Attr(In, TEXT("ColorAndOpacity")))
				 {
					 P.SetColorAndOpacity(AsColor(*Color));
				 }
				 ApplyBase(P, In);
				 return RUI::Slate::Separator(MoveTemp(P), In.Key);
			 });

	Register(TEXT("SpinBox"),
			 [](FUetkxInterpElementInputs&& In)
			 {
				 FRuiSpinBoxProps P;
				 if (const FRuiValue* Value = Attr(In, TEXT("Value")))
				 {
					 P.SetValue(AsFloat(*Value));
				 }
				 if (const FRuiValue* Min = Attr(In, TEXT("MinValue")))
				 {
					 P.SetMinValue(AsFloat(*Min));
				 }
				 if (const FRuiValue* Max = Attr(In, TEXT("MaxValue")))
				 {
					 P.SetMaxValue(AsFloat(*Max));
				 }
				 if (const FRuiValue* Delta = Attr(In, TEXT("Delta")))
				 {
					 P.SetDelta(AsFloat(*Delta));
				 }
				 if (const FRuiCallback* Changed = Event(In, TEXT("OnValueChanged")))
				 {
					 P.SetOnValueChanged(*Changed);
				 }
				 ApplyBase(P, In);
				 return RUI::Slate::SpinBox(MoveTemp(P), In.Key);
			 });

	Register(TEXT("UniformWrapPanel"),
			 [](FUetkxInterpElementInputs&& In)
			 {
				 FRuiUniformWrapPanelProps P;
				 if (const FRuiValue* Padding = Attr(In, TEXT("SlotPadding")))
				 {
					 P.SetSlotPadding(AsFloat(*Padding));
				 }
				 if (const FRuiValue* H = Attr(In, TEXT("HAlign")))
				 {
					 P.SetHAlign(AsName(*H));
				 }
				 ApplyBase(P, In);
				 return RUI::Slate::UniformWrapPanel(MoveTemp(P), MoveTemp(In.Children), In.Key);
			 });

	Register(TEXT("RichTextBlock"),
			 [](FUetkxInterpElementInputs&& In)
			 {
				 FRuiRichTextBlockProps P;
				 if (const FRuiValue* Text = Attr(In, TEXT("Text")))
				 {
					 P.SetText(AsText(*Text));
				 }
				 if (const FRuiValue* Wrap = Attr(In, TEXT("bAutoWrapText")))
				 {
					 P.SetbAutoWrapText(AsBool(*Wrap));
				 }
				 ApplyBase(P, In);
				 return RUI::Slate::RichTextBlock(MoveTemp(P), In.Key);
			 });

	Register(TEXT("GridPanel"),
			 [](FUetkxInterpElementInputs&& In)
			 {
				 FRuiGridPanelProps P;
				 ApplyBase(P, In);
				 return RUI::Slate::GridPanel(MoveTemp(P), MoveTemp(In.Children), In.Key);
			 });

	Register(TEXT("UniformGridPanel"),
			 [](FUetkxInterpElementInputs&& In)
			 {
				 FRuiUniformGridPanelProps P;
				 if (const FRuiValue* Padding = Attr(In, TEXT("SlotPadding")))
				 {
					 P.SetSlotPadding(AsFloat(*Padding));
				 }
				 if (const FRuiValue* W = Attr(In, TEXT("MinDesiredSlotWidth")))
				 {
					 P.SetMinDesiredSlotWidth(AsFloat(*W));
				 }
				 if (const FRuiValue* H = Attr(In, TEXT("MinDesiredSlotHeight")))
				 {
					 P.SetMinDesiredSlotHeight(AsFloat(*H));
				 }
				 ApplyBase(P, In);
				 return RUI::Slate::UniformGridPanel(MoveTemp(P), MoveTemp(In.Children), In.Key);
			 });
}

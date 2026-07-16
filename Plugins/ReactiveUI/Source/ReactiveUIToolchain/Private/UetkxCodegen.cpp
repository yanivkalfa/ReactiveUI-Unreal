// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "UetkxCodegen.h"

#include "Dom/JsonObject.h"
#include "Misc/Paths.h" // FPaths — seller-repo sentinel probe (D-32a / CG-1)
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UetkxJsxScan.h"
#include "UetkxLexer.h"
#include "UetkxResolve.h"

namespace
{
	// ── the D-33 host vocabulary ─────────────────────────────────────────────────────────
	// Attr categories drive the value conversion + the emit target (setter vs dict).

	enum class EAttrType : uint8
	{
		Text,	 // FText prop     -> Set<Name>(NSLOCTEXT(...)) for str attrs
		Float,	 // float prop
		Int,	 // int32/int64 prop
		Bool,	 // bool prop
		Name,	 // FName prop     -> Set<Name>(FName(TEXT("...")))
		Margin,	 // FMargin prop   -> expr-only for non-uniform; str "a" -> FMargin(a)
		Vector2, // FVector2D prop -> expr-only recommended
		Color,	 // FLinearColor   -> expr-only (hex parsing is post-v1)
		Event,	 // FRuiCallback   -> Set<Name>(FRuiCallback::Create([=]() { expr; }))
		Expr	 // expression-only value (callable/brush/...) -> {expr} required
	};

	struct FTagDef
	{
		FString Factory;   // e.g. "RUI::Slate::Button"
		FString PropsType; // e.g. "FRuiButtonProps"
		bool bChildren = true;
		TMap<FName, EAttrType> Attrs;
		FString SinceUE; // engine-version gate ("5.7" = absent from earlier engines); empty = all
	};

	const TMap<FName, FTagDef>& HostTags()
	{
		static const TMap<FName, FTagDef> Tags = []()
		{
			TMap<FName, FTagDef> M;
			{
				FTagDef T{TEXT("RUI::TextBlock"), FString(), false, {}};
				// TextBlock is special-cased in EmitElement (factory takes the FText directly).
				T.Attrs.Add(TEXT("Text"), EAttrType::Text);
				M.Add(TEXT("TextBlock"), MoveTemp(T));
			}
			{
				FTagDef T{TEXT("RUI::Slate::Button"), TEXT("FRuiButtonProps"), true, {}};
				T.Attrs.Add(TEXT("OnClicked"), EAttrType::Event);
				T.Attrs.Add(TEXT("bEnabled"), EAttrType::Bool);
				T.Attrs.Add(TEXT("ContentPadding"), EAttrType::Margin);
				T.Attrs.Add(TEXT("bIsFocusable"), EAttrType::Bool);
				M.Add(TEXT("Button"), MoveTemp(T));
			}
			M.Add(TEXT("VerticalBox"), {TEXT("RUI::Slate::VerticalBox"), TEXT("FRuiVerticalBoxProps"), true, {}});
			M.Add(TEXT("HorizontalBox"), {TEXT("RUI::Slate::HorizontalBox"), TEXT("FRuiHorizontalBoxProps"), true, {}});
			M.Add(TEXT("Overlay"), {TEXT("RUI::Slate::Overlay"), TEXT("FRuiOverlayProps"), true, {}});
			M.Add(TEXT("Canvas"), {TEXT("RUI::Slate::Canvas"), TEXT("FRuiCanvasPanelProps"), true, {}});
			M.Add(TEXT("ConstraintCanvas"),
				  {TEXT("RUI::Slate::ConstraintCanvas"), TEXT("FRuiConstraintCanvasProps"), true, {}});
			{
				FTagDef T{TEXT("RUI::Slate::Splitter"), TEXT("FRuiSplitterProps"), true, {}};
				T.Attrs.Add(TEXT("Orientation"), EAttrType::Name);
				T.Attrs.Add(TEXT("PhysicalSplitterHandleSize"), EAttrType::Float);
				T.Attrs.Add(TEXT("OnSplitterFinishedResizing"), EAttrType::Event);
				M.Add(TEXT("Splitter"), MoveTemp(T));
			}
			{
				FTagDef T{TEXT("RUI::Slate::Splitter2x2"), TEXT("FRuiSplitter2x2Props"), true, {}};
				T.Attrs.Add(TEXT("Percentages"), EAttrType::Expr);
				M.Add(TEXT("Splitter2x2"), MoveTemp(T));
			}
			{
				FTagDef T{TEXT("RUI::Slate::MenuAnchor"), TEXT("FRuiMenuAnchorProps"), true, {}};
				T.Attrs.Add(TEXT("bIsOpen"), EAttrType::Bool);
				T.Attrs.Add(TEXT("Placement"), EAttrType::Name);
				T.Attrs.Add(TEXT("bFitInWindow"), EAttrType::Bool);
				T.Attrs.Add(TEXT("OnMenuOpenChanged"), EAttrType::Event);
				M.Add(TEXT("MenuAnchor"), MoveTemp(T));
			}
			{
				FTagDef T{TEXT("RUI::Slate::WindowTitleBarArea"), TEXT("FRuiWindowTitleBarAreaProps"), true, {}};
				T.Attrs.Add(TEXT("HAlign"), EAttrType::Name);
				T.Attrs.Add(TEXT("VAlign"), EAttrType::Name);
				T.Attrs.Add(TEXT("Padding"), EAttrType::Margin);
				T.Attrs.Add(TEXT("RequestToggleFullscreen"), EAttrType::Event);
				M.Add(TEXT("WindowTitleBarArea"), MoveTemp(T));
			}
			{
				FTagDef T{TEXT("RUI::Slate::NumericDropDown"), TEXT("FRuiNumericDropDownProps"), false, {}};
				T.Attrs.Add(TEXT("Values"), EAttrType::Expr);
				T.Attrs.Add(TEXT("Labels"), EAttrType::Expr);
				T.Attrs.Add(TEXT("Value"), EAttrType::Float);
				T.Attrs.Add(TEXT("bShowNamedValue"), EAttrType::Bool);
				T.Attrs.Add(TEXT("OnValueChanged"), EAttrType::Event);
				M.Add(TEXT("NumericDropDown"), MoveTemp(T));
			}
			{
				FTagDef T{TEXT("RUI::Slate::BreadcrumbTrail"), TEXT("FRuiBreadcrumbTrailProps"), false, {}};
				T.Attrs.Add(TEXT("Crumbs"), EAttrType::Expr);
				T.Attrs.Add(TEXT("bShowLeadingDelimiter"), EAttrType::Bool);
				T.Attrs.Add(TEXT("OnCrumbClicked"), EAttrType::Event);
				M.Add(TEXT("BreadcrumbTrail"), MoveTemp(T));
			}
			M.Add(TEXT("NotificationList"),
				  {TEXT("RUI::Slate::NotificationList"), TEXT("FRuiNotificationListProps"), false, {}});
			{
				FTagDef T{TEXT("RUI::Slate::SearchableComboBox"), TEXT("FRuiSearchableComboBoxProps"), false, {}};
				T.SinceUE = TEXT("5.7"); // SSearchableComboBox does not exist in 5.6
				T.Attrs.Add(TEXT("Options"), EAttrType::Expr);
				T.Attrs.Add(TEXT("SelectedItem"), EAttrType::Text);
				T.Attrs.Add(TEXT("OnSelectionChanged"), EAttrType::Event);
				M.Add(TEXT("SearchableComboBox"), MoveTemp(T));
			}
			{
				FTagDef T{TEXT("RUI::Slate::LinkedBox"), TEXT("FRuiLinkedBoxProps"), true, {}};
				T.Attrs.Add(TEXT("GroupKey"), EAttrType::Name);
				M.Add(TEXT("LinkedBox"), MoveTemp(T));
			}
			M.Add(TEXT("VirtualJoystick"),
				  {TEXT("RUI::Slate::VirtualJoystick"), TEXT("FRuiVirtualJoystickProps"), false, {}});
			{
				FTagDef T{TEXT("RUI::Slate::VectorInputBox"), TEXT("FRuiVectorInputBoxProps"), false, {}};
				T.Attrs.Add(TEXT("X"), EAttrType::Float);
				T.Attrs.Add(TEXT("Y"), EAttrType::Float);
				T.Attrs.Add(TEXT("Z"), EAttrType::Float);
				T.Attrs.Add(TEXT("bColorAxisLabels"), EAttrType::Bool);
				T.Attrs.Add(TEXT("OnXChanged"), EAttrType::Event);
				T.Attrs.Add(TEXT("OnYChanged"), EAttrType::Event);
				T.Attrs.Add(TEXT("OnZChanged"), EAttrType::Event);
				M.Add(TEXT("VectorInputBox"), MoveTemp(T));
			}
			{
				FTagDef T{TEXT("RUI::Slate::RotatorInputBox"), TEXT("FRuiRotatorInputBoxProps"), false, {}};
				T.Attrs.Add(TEXT("Roll"), EAttrType::Float);
				T.Attrs.Add(TEXT("Pitch"), EAttrType::Float);
				T.Attrs.Add(TEXT("Yaw"), EAttrType::Float);
				T.Attrs.Add(TEXT("bColorAxisLabels"), EAttrType::Bool);
				T.Attrs.Add(TEXT("OnRollChanged"), EAttrType::Event);
				T.Attrs.Add(TEXT("OnPitchChanged"), EAttrType::Event);
				T.Attrs.Add(TEXT("OnYawChanged"), EAttrType::Event);
				M.Add(TEXT("RotatorInputBox"), MoveTemp(T));
			}
			{
				FTagDef T{TEXT("RUI::Slate::ColorBlock"), TEXT("FRuiColorBlockProps"), false, {}};
				T.Attrs.Add(TEXT("Color"), EAttrType::Color);
				T.Attrs.Add(TEXT("Size"), EAttrType::Vector2);
				T.Attrs.Add(TEXT("bUseSRGB"), EAttrType::Bool);
				T.Attrs.Add(TEXT("bShowBackgroundForAlpha"), EAttrType::Bool);
				T.Attrs.Add(TEXT("bColorIsHSV"), EAttrType::Bool);
				T.Attrs.Add(TEXT("AlphaDisplayMode"), EAttrType::Name);
				M.Add(TEXT("ColorBlock"), MoveTemp(T));
			}
			{
				FTagDef T{TEXT("RUI::Slate::SimpleGradient"), TEXT("FRuiSimpleGradientProps"), false, {}};
				T.Attrs.Add(TEXT("StartColor"), EAttrType::Color);
				T.Attrs.Add(TEXT("EndColor"), EAttrType::Color);
				T.Attrs.Add(TEXT("Orientation"), EAttrType::Name);
				T.Attrs.Add(TEXT("bHasAlphaBackground"), EAttrType::Bool);
				M.Add(TEXT("SimpleGradient"), MoveTemp(T));
			}
			{
				FTagDef T{TEXT("RUI::Slate::ComplexGradient"), TEXT("FRuiComplexGradientProps"), false, {}};
				T.Attrs.Add(TEXT("GradientColors"), EAttrType::Expr);
				T.Attrs.Add(TEXT("Orientation"), EAttrType::Name);
				T.Attrs.Add(TEXT("bHasAlphaBackground"), EAttrType::Bool);
				T.Attrs.Add(TEXT("DesiredSizeOverride"), EAttrType::Vector2);
				M.Add(TEXT("ComplexGradient"), MoveTemp(T));
			}
			{
				FTagDef T{TEXT("RUI::Slate::Hyperlink"), TEXT("FRuiHyperlinkProps"), false, {}};
				T.Attrs.Add(TEXT("Text"), EAttrType::Text);
				T.Attrs.Add(TEXT("Padding"), EAttrType::Margin);
				T.Attrs.Add(TEXT("OnNavigate"), EAttrType::Event);
				M.Add(TEXT("Hyperlink"), MoveTemp(T));
			}
			M.Add(TEXT("EnableBox"), {TEXT("RUI::Slate::EnableBox"), TEXT("FRuiEnableBoxProps"), true, {}});
			M.Add(TEXT("ScissorRectBox"),
				  {TEXT("RUI::Slate::ScissorRectBox"), TEXT("FRuiScissorRectBoxProps"), true, {}});
			{
				FTagDef T{TEXT("RUI::Slate::BackgroundBlur"), TEXT("FRuiBackgroundBlurProps"), true, {}};
				T.Attrs.Add(TEXT("BlurStrength"), EAttrType::Float);
				T.Attrs.Add(TEXT("BlurRadius"), EAttrType::Int);
				T.Attrs.Add(TEXT("bApplyAlphaToBlur"), EAttrType::Bool);
				T.Attrs.Add(TEXT("Padding"), EAttrType::Margin);
				M.Add(TEXT("BackgroundBlur"), MoveTemp(T));
			}
			{
				FTagDef T{TEXT("RUI::Slate::InvalidationPanel"), TEXT("FRuiInvalidationPanelProps"), true, {}};
				T.Attrs.Add(TEXT("bCanCache"), EAttrType::Bool);
				M.Add(TEXT("InvalidationPanel"), MoveTemp(T));
			}
			{
				FTagDef T{TEXT("RUI::Slate::VolumeControl"), TEXT("FRuiVolumeControlProps"), false, {}};
				T.Attrs.Add(TEXT("Volume"), EAttrType::Float);
				T.Attrs.Add(TEXT("bMuted"), EAttrType::Bool);
				T.Attrs.Add(TEXT("OnVolumeChanged"), EAttrType::Event);
				T.Attrs.Add(TEXT("OnMuteChanged"), EAttrType::Event);
				M.Add(TEXT("VolumeControl"), MoveTemp(T));
			}
			{
				FTagDef T{TEXT("RUI::Slate::TextScroller"), TEXT("FRuiTextScrollerProps"), true, {}};
				T.Attrs.Add(TEXT("Speed"), EAttrType::Float);
				T.Attrs.Add(TEXT("StartDelay"), EAttrType::Float);
				T.Attrs.Add(TEXT("EndDelay"), EAttrType::Float);
				T.Attrs.Add(TEXT("ScrollOrientation"), EAttrType::Name);
				M.Add(TEXT("TextScroller"), MoveTemp(T));
			}
			{
				FTagDef T{TEXT("RUI::Slate::RadialBox"), TEXT("FRuiRadialBoxProps"), true, {}};
				T.Attrs.Add(TEXT("PreferredWidth"), EAttrType::Float);
				T.Attrs.Add(TEXT("bUseAllottedWidth"), EAttrType::Bool);
				T.Attrs.Add(TEXT("StartingAngle"), EAttrType::Float);
				T.Attrs.Add(TEXT("bDistributeItemsEvenly"), EAttrType::Bool);
				T.Attrs.Add(TEXT("AngleBetweenItems"), EAttrType::Float);
				T.Attrs.Add(TEXT("SectorCentralAngle"), EAttrType::Float);
				M.Add(TEXT("RadialBox"), MoveTemp(T));
			}
			{
				FTagDef T{TEXT("RUI::Slate::ColorWheel"), TEXT("FRuiColorWheelProps"), false, {}};
				T.Attrs.Add(TEXT("SelectedColor"), EAttrType::Color);
				T.Attrs.Add(TEXT("OnValueChanged"), EAttrType::Event);
				T.Attrs.Add(TEXT("OnMouseCaptureBegin"), EAttrType::Event);
				T.Attrs.Add(TEXT("OnMouseCaptureEnd"), EAttrType::Event);
				M.Add(TEXT("ColorWheel"), MoveTemp(T));
			}
			{
				FTagDef T{TEXT("RUI::Slate::ColorSpectrum"), TEXT("FRuiColorSpectrumProps"), false, {}};
				T.Attrs.Add(TEXT("SelectedColor"), EAttrType::Color);
				T.Attrs.Add(TEXT("OnValueChanged"), EAttrType::Event);
				T.Attrs.Add(TEXT("OnMouseCaptureBegin"), EAttrType::Event);
				T.Attrs.Add(TEXT("OnMouseCaptureEnd"), EAttrType::Event);
				M.Add(TEXT("ColorSpectrum"), MoveTemp(T));
			}
			{
				FTagDef T{TEXT("RUI::Slate::LayeredImage"), TEXT("FRuiLayeredImageProps"), false, {}};
				T.Attrs.Add(TEXT("ColorAndOpacity"), EAttrType::Color);
				T.Attrs.Add(TEXT("DesiredSizeOverride"), EAttrType::Vector2);
				T.Attrs.Add(TEXT("Image"), EAttrType::Expr);
				T.Attrs.Add(TEXT("Layers"), EAttrType::Expr);
				M.Add(TEXT("LayeredImage"), MoveTemp(T));
			}
			{
				FTagDef T{TEXT("RUI::Slate::InputKeySelector"), TEXT("FRuiInputKeySelectorProps"), false, {}};
				T.Attrs.Add(TEXT("SelectedKey"), EAttrType::Name);
				T.Attrs.Add(TEXT("KeySelectionText"), EAttrType::Text);
				T.Attrs.Add(TEXT("NoKeySpecifiedText"), EAttrType::Text);
				T.Attrs.Add(TEXT("bAllowModifierKeys"), EAttrType::Bool);
				T.Attrs.Add(TEXT("bAllowGamepadKeys"), EAttrType::Bool);
				T.Attrs.Add(TEXT("bEscapeCancelsSelection"), EAttrType::Bool);
				T.Attrs.Add(TEXT("OnKeySelected"), EAttrType::Event);
				T.Attrs.Add(TEXT("OnIsSelectingKeyChanged"), EAttrType::Event);
				M.Add(TEXT("InputKeySelector"), MoveTemp(T));
			}
			{
				FTagDef T{TEXT("RUI::Slate::EditableText"), TEXT("FRuiEditableTextProps"), false, {}};
				T.Attrs.Add(TEXT("Text"), EAttrType::Text);
				T.Attrs.Add(TEXT("HintText"), EAttrType::Text);
				T.Attrs.Add(TEXT("bIsReadOnly"), EAttrType::Bool);
				T.Attrs.Add(TEXT("bIsPassword"), EAttrType::Bool);
				T.Attrs.Add(TEXT("MinDesiredWidth"), EAttrType::Float);
				T.Attrs.Add(TEXT("OnTextChanged"), EAttrType::Event);
				T.Attrs.Add(TEXT("OnTextCommitted"), EAttrType::Event);
				M.Add(TEXT("EditableText"), MoveTemp(T));
			}
			{
				FTagDef T{
					TEXT("RUI::Slate::InlineEditableTextBlock"), TEXT("FRuiInlineEditableTextBlockProps"), false, {}};
				T.Attrs.Add(TEXT("Text"), EAttrType::Text);
				T.Attrs.Add(TEXT("HintText"), EAttrType::Text);
				T.Attrs.Add(TEXT("bIsReadOnly"), EAttrType::Bool);
				T.Attrs.Add(TEXT("WrapTextAt"), EAttrType::Float);
				T.Attrs.Add(TEXT("bMultiLine"), EAttrType::Bool);
				T.Attrs.Add(TEXT("OnTextCommitted"), EAttrType::Event);
				M.Add(TEXT("InlineEditableTextBlock"), MoveTemp(T));
			}
			{
				FTagDef T{TEXT("RUI::Slate::VirtualKeyboardEntry"), TEXT("FRuiVirtualKeyboardEntryProps"), false, {}};
				T.Attrs.Add(TEXT("Text"), EAttrType::Text);
				T.Attrs.Add(TEXT("HintText"), EAttrType::Text);
				T.Attrs.Add(TEXT("bIsReadOnly"), EAttrType::Bool);
				T.Attrs.Add(TEXT("KeyboardType"), EAttrType::Name);
				T.Attrs.Add(TEXT("OnTextChanged"), EAttrType::Event);
				T.Attrs.Add(TEXT("OnTextCommitted"), EAttrType::Event);
				M.Add(TEXT("VirtualKeyboardEntry"), MoveTemp(T));
			}
			{
				FTagDef T{TEXT("RUI::Slate::ColorGradingWheel"), TEXT("FRuiColorGradingWheelProps"), false, {}};
				T.Attrs.Add(TEXT("SelectedColor"), EAttrType::Color);
				T.Attrs.Add(TEXT("DesiredWheelSize"), EAttrType::Int);
				T.Attrs.Add(TEXT("ExponentDisplacement"), EAttrType::Float);
				T.Attrs.Add(TEXT("OnValueChanged"), EAttrType::Event);
				T.Attrs.Add(TEXT("OnMouseCaptureBegin"), EAttrType::Event);
				T.Attrs.Add(TEXT("OnMouseCaptureEnd"), EAttrType::Event);
				M.Add(TEXT("ColorGradingWheel"), MoveTemp(T));
			}
			{
				FTagDef T{TEXT("RUI::Slate::ExpandableButton"), TEXT("FRuiExpandableButtonProps"), true, {}};
				T.Attrs.Add(TEXT("CollapsedText"), EAttrType::Text);
				T.Attrs.Add(TEXT("ExpandedText"), EAttrType::Text);
				T.Attrs.Add(TEXT("bIsExpanded"), EAttrType::Bool);
				T.Attrs.Add(TEXT("OnExpansionClicked"), EAttrType::Event);
				T.Attrs.Add(TEXT("OnCloseClicked"), EAttrType::Event);
				M.Add(TEXT("ExpandableButton"), MoveTemp(T));
			}
			{
				FTagDef T{TEXT("RUI::Slate::Border"), TEXT("FRuiBorderProps"), true, {}};
				T.Attrs.Add(TEXT("Padding"), EAttrType::Margin);
				T.Attrs.Add(TEXT("BorderBackgroundColor"), EAttrType::Color);
				T.Attrs.Add(TEXT("HAlign"), EAttrType::Name);
				T.Attrs.Add(TEXT("VAlign"), EAttrType::Name);
				T.Attrs.Add(TEXT("BorderImage"), EAttrType::Name);
				M.Add(TEXT("Border"), MoveTemp(T));
			}
			{
				FTagDef T{TEXT("RUI::Slate::Box"), TEXT("FRuiBoxProps"), true, {}};
				for (const TCHAR* P : {TEXT("WidthOverride"), TEXT("HeightOverride"), TEXT("MinDesiredWidth"),
									   TEXT("MinDesiredHeight"), TEXT("MaxDesiredWidth"), TEXT("MaxDesiredHeight")})
				{
					T.Attrs.Add(P, EAttrType::Float);
				}
				T.Attrs.Add(TEXT("HAlign"), EAttrType::Name);
				T.Attrs.Add(TEXT("VAlign"), EAttrType::Name);
				M.Add(TEXT("Box"), MoveTemp(T));
			}
			{
				FTagDef T{TEXT("RUI::Slate::Image"), TEXT("FRuiImageProps"), false, {}};
				T.Attrs.Add(TEXT("ColorAndOpacity"), EAttrType::Color);
				T.Attrs.Add(TEXT("DesiredSizeOverride"), EAttrType::Vector2);
				T.Attrs.Add(TEXT("Image"), EAttrType::Expr); // asset brush (TSharedPtr<FSlateBrush> expr — D-17)
				M.Add(TEXT("Image"), MoveTemp(T));
			}
			{
				FTagDef T{TEXT("RUI::Slate::ScrollBox"), TEXT("FRuiScrollBoxProps"), true, {}};
				T.Attrs.Add(TEXT("Orientation"), EAttrType::Name);
				T.Attrs.Add(TEXT("bAllowOverscroll"), EAttrType::Bool);
				T.Attrs.Add(TEXT("bAnimateWheelScrolling"), EAttrType::Bool);
				T.Attrs.Add(TEXT("WheelScrollMultiplier"), EAttrType::Float);
				M.Add(TEXT("ScrollBox"), MoveTemp(T));
			}
			{
				FTagDef T{TEXT("RUI::Slate::Spacer"), TEXT("FRuiSpacerProps"), false, {}};
				T.Attrs.Add(TEXT("Size"), EAttrType::Vector2);
				M.Add(TEXT("Spacer"), MoveTemp(T));
			}
			{
				FTagDef T{TEXT("RUI::Slate::EditableTextBox"), TEXT("FRuiEditableTextBoxProps"), false, {}};
				T.Attrs.Add(TEXT("Text"), EAttrType::Text);
				T.Attrs.Add(TEXT("HintText"), EAttrType::Text);
				T.Attrs.Add(TEXT("bIsReadOnly"), EAttrType::Bool);
				T.Attrs.Add(TEXT("OnTextChanged"), EAttrType::Event);
				T.Attrs.Add(TEXT("OnTextCommitted"), EAttrType::Event);
				M.Add(TEXT("EditableTextBox"), MoveTemp(T));
			}
			{
				FTagDef T{TEXT("RUI::Slate::CheckBox"), TEXT("FRuiCheckBoxProps"), true, {}};
				T.Attrs.Add(TEXT("bIsChecked"), EAttrType::Bool);
				T.Attrs.Add(TEXT("OnCheckStateChanged"), EAttrType::Event);
				M.Add(TEXT("CheckBox"), MoveTemp(T));
			}
			{
				FTagDef T{TEXT("RUI::Slate::Slider"), TEXT("FRuiSliderProps"), false, {}};
				for (const TCHAR* P : {TEXT("Value"), TEXT("MinValue"), TEXT("MaxValue"), TEXT("StepSize")})
				{
					T.Attrs.Add(P, EAttrType::Float);
				}
				T.Attrs.Add(TEXT("OnValueChanged"), EAttrType::Event);
				T.Attrs.Add(TEXT("Orientation"), EAttrType::Name);
				T.Attrs.Add(TEXT("bLocked"), EAttrType::Bool);
				T.Attrs.Add(TEXT("bIndentHandle"), EAttrType::Bool);
				T.Attrs.Add(TEXT("SliderBarColor"), EAttrType::Color);
				T.Attrs.Add(TEXT("SliderHandleColor"), EAttrType::Color);
				M.Add(TEXT("Slider"), MoveTemp(T));
			}
			{
				FTagDef T{TEXT("RUI::Slate::ProgressBar"), TEXT("FRuiProgressBarProps"), false, {}};
				T.Attrs.Add(TEXT("Percent"), EAttrType::Float);
				T.Attrs.Add(TEXT("BarFillType"), EAttrType::Name);
				M.Add(TEXT("ProgressBar"), MoveTemp(T));
			}
			{
				FTagDef T{TEXT("RUI::Slate::RuiCanvas"), TEXT("FRuiCanvasProps"), false, {}};
				T.Attrs.Add(TEXT("RedrawKey"), EAttrType::Int);
				T.Attrs.Add(TEXT("CanvasSize"), EAttrType::Vector2);
				T.Attrs.Add(TEXT("DrawFn"), EAttrType::Expr); // identity semantics — {expr} only
				M.Add(TEXT("RuiCanvas"), MoveTemp(T));
			}
			// ── Batch 2 (Phase 7 step 8) — the everyday game set ────────────────────────────
			{
				FTagDef T{TEXT("RUI::Slate::WidgetSwitcher"), TEXT("FRuiWidgetSwitcherProps"), true, {}};
				T.Attrs.Add(TEXT("WidgetIndex"), EAttrType::Int);
				M.Add(TEXT("WidgetSwitcher"), MoveTemp(T));
			}
			{
				FTagDef T{TEXT("RUI::Slate::ScaleBox"), TEXT("FRuiScaleBoxProps"), true, {}};
				T.Attrs.Add(TEXT("Stretch"), EAttrType::Name);
				T.Attrs.Add(TEXT("StretchDirection"), EAttrType::Name);
				T.Attrs.Add(TEXT("HAlign"), EAttrType::Name);
				T.Attrs.Add(TEXT("VAlign"), EAttrType::Name);
				M.Add(TEXT("ScaleBox"), MoveTemp(T));
			}
			{
				FTagDef T{TEXT("RUI::Slate::Throbber"), TEXT("FRuiThrobberProps"), false, {}};
				T.Attrs.Add(TEXT("NumPieces"), EAttrType::Int);
				T.Attrs.Add(TEXT("Animate"), EAttrType::Name);
				M.Add(TEXT("Throbber"), MoveTemp(T));
			}
			{
				FTagDef T{TEXT("RUI::Slate::WrapBox"), TEXT("FRuiWrapBoxProps"), true, {}};
				T.Attrs.Add(TEXT("Orientation"), EAttrType::Name);
				T.Attrs.Add(TEXT("WrapSize"), EAttrType::Float);
				T.Attrs.Add(TEXT("InnerSlotPadding"), EAttrType::Vector2);
				T.Attrs.Add(TEXT("bUseAllottedSize"), EAttrType::Bool);
				M.Add(TEXT("WrapBox"), MoveTemp(T));
			}
			{
				FTagDef T{
					TEXT("RUI::Slate::MultiLineEditableTextBox"), TEXT("FRuiMultiLineEditableTextBoxProps"), false, {}};
				T.Attrs.Add(TEXT("Text"), EAttrType::Text);
				T.Attrs.Add(TEXT("HintText"), EAttrType::Text);
				T.Attrs.Add(TEXT("bIsReadOnly"), EAttrType::Bool);
				T.Attrs.Add(TEXT("OnTextChanged"), EAttrType::Event);
				T.Attrs.Add(TEXT("OnTextCommitted"), EAttrType::Event);
				M.Add(TEXT("MultiLineEditableTextBox"), MoveTemp(T));
			}
			{
				FTagDef T{TEXT("RUI::Slate::SearchBox"), TEXT("FRuiSearchBoxProps"), false, {}};
				T.Attrs.Add(TEXT("Text"), EAttrType::Text);
				T.Attrs.Add(TEXT("HintText"), EAttrType::Text);
				T.Attrs.Add(TEXT("OnTextChanged"), EAttrType::Event);
				T.Attrs.Add(TEXT("OnTextCommitted"), EAttrType::Event);
				M.Add(TEXT("SearchBox"), MoveTemp(T));
			}
			{
				FTagDef T{TEXT("RUI::Slate::SafeZone"), TEXT("FRuiSafeZoneProps"), true, {}};
				for (const TCHAR* P :
					 {TEXT("bIsTitleSafe"), TEXT("bPadLeft"), TEXT("bPadRight"), TEXT("bPadTop"), TEXT("bPadBottom")})
				{
					T.Attrs.Add(P, EAttrType::Bool);
				}
				M.Add(TEXT("SafeZone"), MoveTemp(T));
			}
			{
				FTagDef T{TEXT("RUI::Slate::DPIScaler"), TEXT("FRuiDPIScalerProps"), true, {}};
				T.Attrs.Add(TEXT("DPIScale"), EAttrType::Float);
				M.Add(TEXT("DPIScaler"), MoveTemp(T));
			}
			{
				FTagDef T{TEXT("RUI::Slate::Separator"), TEXT("FRuiSeparatorProps"), false, {}};
				T.Attrs.Add(TEXT("Orientation"), EAttrType::Name);
				T.Attrs.Add(TEXT("Thickness"), EAttrType::Float);
				T.Attrs.Add(TEXT("ColorAndOpacity"), EAttrType::Color);
				M.Add(TEXT("Separator"), MoveTemp(T));
			}
			{
				FTagDef T{TEXT("RUI::Slate::SpinBox"), TEXT("FRuiSpinBoxProps"), false, {}};
				for (const TCHAR* P : {TEXT("Value"), TEXT("MinValue"), TEXT("MaxValue"), TEXT("Delta")})
				{
					T.Attrs.Add(P, EAttrType::Float);
				}
				T.Attrs.Add(TEXT("OnValueChanged"), EAttrType::Event);
				M.Add(TEXT("SpinBox"), MoveTemp(T));
			}
			{
				FTagDef T{TEXT("RUI::Slate::UniformWrapPanel"), TEXT("FRuiUniformWrapPanelProps"), true, {}};
				T.Attrs.Add(TEXT("SlotPadding"), EAttrType::Float);
				T.Attrs.Add(TEXT("HAlign"), EAttrType::Name);
				M.Add(TEXT("UniformWrapPanel"), MoveTemp(T));
			}
			{
				FTagDef T{TEXT("RUI::Slate::RichTextBlock"), TEXT("FRuiRichTextBlockProps"), false, {}};
				T.Attrs.Add(TEXT("Text"), EAttrType::Text);
				T.Attrs.Add(TEXT("bAutoWrapText"), EAttrType::Bool);
				M.Add(TEXT("RichTextBlock"), MoveTemp(T));
			}
			M.Add(TEXT("GridPanel"), {TEXT("RUI::Slate::GridPanel"), TEXT("FRuiGridPanelProps"), true, {}});
			{
				FTagDef T{TEXT("RUI::Slate::UniformGridPanel"), TEXT("FRuiUniformGridPanelProps"), true, {}};
				for (const TCHAR* P : {TEXT("SlotPadding"), TEXT("MinDesiredSlotWidth"), TEXT("MinDesiredSlotHeight")})
				{
					T.Attrs.Add(P, EAttrType::Float);
				}
				M.Add(TEXT("UniformGridPanel"), MoveTemp(T));
			}
			return M;
		}();
		return Tags;
	}

	const TSet<FString>& StyleKeys()
	{
		static const TSet<FString> Keys = {TEXT("RenderOpacity"),
										   TEXT("Visibility"),
										   TEXT("Enabled"),
										   TEXT("RenderTranslation"),
										   TEXT("RenderScale"),
										   TEXT("RenderTransformAngle"),
										   TEXT("RenderTransformPivot"),
										   TEXT("ColorAndOpacity"),
										   TEXT("Font.Size"),
										   TEXT("Justification"),
										   TEXT("AutoWrapText"),
										   TEXT("FillColorAndOpacity"),
										   TEXT("Clipping"),
										   TEXT("ToolTipText"),
										   TEXT("LineHeightPercentage"),
										   TEXT("OverflowPolicy")};
		return Keys;
	}

	bool IsStyleKey(const FString& Name)
	{
		return StyleKeys().Contains(Name);
	}

	FString CppStringLiteral(const FString& S)
	{
		FString Out = S;
		Out.ReplaceInline(TEXT("\\"), TEXT("\\\\"));
		Out.ReplaceInline(TEXT("\""), TEXT("\\\""));
		Out.ReplaceInline(TEXT("\n"), TEXT("\\n"));
		Out.ReplaceInline(TEXT("\t"), TEXT("\\t"));
		Out.ReplaceInline(TEXT("\r"), TEXT("\\r"));
		return Out;
	}

	/** Hook auto-prefix over verbatim C++ (shared by component setup/exprs AND `hook` decl
	 *  bodies): bare BUILT-IN hook calls become Ctx.-qualified; bare USER hook calls
	 *  (`Use<Upper>...` not in the built-in table, incl. `NS::Use...`) get `Ctx` injected as
	 *  their first argument — user hooks are plain functions taking FRuiContext& first (the
	 *  documented divergence from Unity's ambient statics). Member access (`.`/`->`) blocks
	 *  both transforms. `Qualified` (M5) maps a same-file PRIVATE decl name to its detail-namespace
	 *  prefix (`RuiPriv_<Basename>::`); a private hook call or a private `Module::` qual is rewritten
	 *  to reach into that namespace. */
	FString PrefixHookCalls(const FString& Code, const TMap<FString, FString>& Qualified = {})
	{
		const TArray<int32> Src = FUetkxLexer::ToCodePoints(Code);
		FString Out;
		int32 i = 0;
		const int32 N = Src.Num();
		while (i < N)
		{
			const int32 j = FUetkxLexer::SkipNoncode(Src, i);
			if (j != i)
			{
				Out += FUetkxLexer::FromCodePoints(Src, i, j - i);
				i = j;
				continue;
			}
			// member/scope access scan-back, shared by both branches
			auto ScanBack = [&Src](int32 From, bool& bOutMember, bool& bOutScope)
			{
				bOutMember = false;
				bOutScope = false;
				for (int32 k = From - 1; k >= 0; --k)
				{
					const int32 P = Src[k];
					if (P == ' ' || P == '\t')
					{
						continue;
					}
					bOutMember = (P == '.') || (P == '>' && k > 0 && Src[k - 1] == '-');
					bOutScope = (P == ':');
					break;
				}
			};
			bool bMatched = false;
			const bool bWordStart = (i == 0) || !FUetkxLexer::IsIdentCode(Src[i - 1]);
			if (bWordStart && (Src[i] == 'U' || Src[i] == 'P'))
			{
				for (const FString& Hook : FUetkxFileScan::HookNames())
				{
					// UseSignal/UseSignalKey are RUI:: free functions taking Ctx as an
					// argument — never Ctx.-prefixable (authors write the qualified form).
					if (Hook == TEXT("UseSignal") || Hook == TEXT("UseSignalKey"))
					{
						continue;
					}
					if (FUetkxLexer::KeywordAt(Src, i, *Hook))
					{
						bool bMember = false, bScope = false;
						ScanBack(i, bMember, bScope);
						if (!bMember && !bScope)
						{
							Out += TEXT("Ctx.");
						}
						Out += Hook;
						i += Hook.Len();
						bMatched = true;
						break;
					}
				}
			}
			// user hook: Use<Upper>… + not built-in + call syntax → inject Ctx as first arg
			if (!bMatched && bWordStart && i + 3 < N && Src[i] == 'U' && Src[i + 1] == 's' && Src[i + 2] == 'e' &&
				Src[i + 3] >= 'A' && Src[i + 3] <= 'Z')
			{
				int32 e = i;
				while (e < N && FUetkxLexer::IsIdentCode(Src[e]))
				{
					++e;
				}
				const FString Ident = FUetkxLexer::FromCodePoints(Src, i, e - i);
				bool bMember = false, bScope = false;
				ScanBack(i, bMember, bScope);
				int32 p = e;
				while (p < N && (Src[p] == ' ' || Src[p] == '\t'))
				{
					++p;
				}
				if (!FUetkxFileScan::HookNames().Contains(Ident) && !bMember && p < N && Src[p] == '(')
				{
					int32 q = p + 1;
					while (q < N && (Src[q] == ' ' || Src[q] == '\t' || Src[q] == '\n' || Src[q] == '\r'))
					{
						++q;
					}
					const bool bEmptyArgs = (q < N && Src[q] == ')');
					if (const FString* Prefix = Qualified.Find(Ident)) // private same-file hook
					{
						Out += *Prefix;
					}
					Out += Ident;
					Out += bEmptyArgs ? TEXT("(Ctx") : TEXT("(Ctx, ");
					i = p + 1;
					bMatched = true;
				}
			}
			// private same-file `Module::` qual → RuiPriv_<Basename>::Module:: (M5). Only fires for
			// an identifier the file declares privately; ambient namespaces are untouched.
			if (!bMatched && bWordStart && !Qualified.IsEmpty() && FUetkxLexer::IsIdentCode(Src[i]) &&
				!(Src[i] >= '0' && Src[i] <= '9'))
			{
				int32 e = i;
				while (e < N && FUetkxLexer::IsIdentCode(Src[e]))
				{
					++e;
				}
				int32 p = e;
				while (p < N && (Src[p] == ' ' || Src[p] == '\t'))
				{
					++p;
				}
				bool bMember = false, bScope = false;
				ScanBack(i, bMember, bScope);
				const FString Ident = FUetkxLexer::FromCodePoints(Src, i, e - i);
				const FString* Prefix = Qualified.Find(Ident);
				if (Prefix && !bMember && !bScope && p + 1 < N && Src[p] == ':' && Src[p + 1] == ':')
				{
					Out += *Prefix;
					Out += Ident;
					i = e;
					bMatched = true;
				}
			}
			if (!bMatched)
			{
				Out += FUetkxLexer::FromCodePoints(Src, i, 1);
				++i;
			}
		}
		return Out;
	}

	/** The per-file detail namespace private declarations live in (A5e): `RuiPriv_<Basename>`, with
	 *  any non-identifier characters in the basename (companion dots) folded to `_`. Two files' same-
	 *  named private decls never collide in the aggregator TU (the compile-time half of privacy). */
	FString PrivNamespaceFor(const FString& Basename)
	{
		FString S = Basename;
		for (int32 i = 0; i < S.Len(); ++i)
		{
			if (!FUetkxLexer::IsIdentCode(S[i]))
			{
				S[i] = '_';
			}
		}
		return TEXT("RuiPriv_") + S;
	}

	/** Line-mapping context for the `#line` directives (M7): the file's line-start table, the
	 *  project-relative .uetkx path a breakpoint binds to, and the .inl's own name for the restore
	 *  directive. Disabled (no directives) when no ProjectRelPath was supplied. */
	struct FLineCtx
	{
		TArray<int32> LineStarts;
		FString ProjRel;
		FString InlName;
		bool bEnabled = false;
	};

	/** Wrap a verbatim user-code region (component setup / hook body / module body) in `#line`
	 *  directives so a VS breakpoint in the .uetkx binds to it. The region is spliced with its line
	 *  count preserved (re-indent `\n`->`\n\t` adds no lines), so the mapping stays line-for-line.
	 *  The restore directive uses a @@R@@ placeholder fixed up once the whole .inl is assembled.
	 *  Column drift (the extra indent) + single-line attr/event exprs are accepted limits (M7). */
	FString WithLine(const FString& RegionText, int32 SrcLine, const FLineCtx& Ctx)
	{
		if (!Ctx.bEnabled || RegionText.IsEmpty())
		{
			return RegionText;
		}
		return FString::Printf(TEXT("#line %d \"%s\"\n"), FMath::Max(1, SrcLine), *Ctx.ProjRel) + RegionText +
			   FString::Printf(TEXT("#line @@R@@ \"%s\"\n"), *Ctx.InlName);
	}

	/** The 1-based source line of a verbatim region's first NON-whitespace char (the trim skips
	 *  leading blank lines, so the `#line` must point past them). Region = the raw slice; RegionAt =
	 *  its code-point offset in the file. */
	int32 SrcLineOfRegion(const FString& Region, int32 RegionAt, const FLineCtx& Ctx)
	{
		const int32 LeadWs = Region.Len() - Region.TrimStart().Len();
		return FUetkxLexer::LineOf(Ctx.LineStarts, RegionAt + LeadWs);
	}

	/** A declaration split into the aggregator's two phases (M6). The DECL phase (complete props
	 *  structs + defaulted wrapper decls + hook fwd-decls + module bodies) is `#include`d for EVERY
	 *  file before ANY body — so cross-file component references (incl. CYCLES) are all forward-
	 *  declared before use. The BODY phase carries impls + default-FREE wrapper defs + registrations
	 *  (+ hook bodies). Modules live entirely in the DECL phase (their member defaults may reference
	 *  imported module constants). */
	struct FEmittedDecl
	{
		FString DeclPhase;
		FString BodyPhase;
	};

	/** Wrap both phases of a private declaration in the per-file detail namespace (A5e). */
	void WrapPrivate(FEmittedDecl& E, const FString& PrivNs)
	{
		const FString Open = FString::Printf(TEXT("namespace %s\n{\n"), *PrivNs);
		const FString Close = FString::Printf(TEXT("} // namespace %s\n"), *PrivNs);
		if (!E.DeclPhase.IsEmpty())
		{
			E.DeclPhase = Open + E.DeclPhase + Close;
		}
		if (!E.BodyPhase.IsEmpty())
		{
			E.BodyPhase = Open + E.BodyPhase + Close;
		}
	}

	/** Re-indent a verbatim user region: insert a tab after every newline that is OUTSIDE a
	 *  string/char/raw-string/comment token, so re-indentation never mutates multi-line string-literal
	 *  CONTENT (bughunt CG-2 — the old blanket `Replace("\n","\n\t")` injected a tab inside raw strings,
	 *  silently changing the runtime value). Every newline is preserved, so #line mapping is unaffected. */
	FString IndentRegion(const FString& Body)
	{
		const TArray<int32> Src = FUetkxLexer::ToCodePoints(Body);
		const int32 N = Src.Num();
		FString Out;
		int32 i = 0;
		while (i < N)
		{
			const int32 j = FUetkxLexer::SkipNoncode(Src, i);
			if (j != i)
			{
				Out += FUetkxLexer::FromCodePoints(Src, i, j - i); // token verbatim — no indent injected
				i = j;
				continue;
			}
			const int32 C = Src[i++];
			Out.AppendChar(static_cast<TCHAR>(C));
			if (C == static_cast<int32>('\n'))
			{
				Out.AppendChar(TEXT('\t'));
			}
		}
		return Out;
	}

	/** The canonical (case-exact) tag name of a host def — its Factory's last `::` segment, which equals
	 *  the tag key for every host tag (verified). Derived from the Factory LITERAL, so the casing is
	 *  reliable (unlike FName::ToString, which reflects the FName pool's first-seen casing). Used to
	 *  reject a mis-cased tag (<Textblock/>) case-sensitively — HostTags()'s FName match is not (CG-3). */
	FString CanonicalTagName(const FTagDef& Tag)
	{
		int32 Colon = INDEX_NONE;
		return Tag.Factory.FindLastChar(TEXT(':'), Colon) ? Tag.Factory.RightChop(Colon + 1) : Tag.Factory;
	}

	/** A `hook` declaration → an inline free function taking FRuiContext& first (built-in hook
	 *  calls in the body Ctx.-prefixed, nested user hooks Ctx-injected). DECL phase = the forward
	 *  declaration; BODY phase = the definition. A non-exported hook wraps in the detail namespace. */
	FEmittedDecl EmitHookInl(const FUetkxHookDecl& Hook, const FString& PrivNs, const TMap<FString, FString>& Qualified,
							 const FLineCtx& Line)
	{
		const FString Ret = Hook.Ret.IsEmpty() ? FString(TEXT("void")) : Hook.Ret;
		const FString Sig = FString::Printf(TEXT("inline %s %s(FRuiContext& Ctx%s%s)"), *Ret, *Hook.Name,
											Hook.Params.IsEmpty() ? TEXT("") : TEXT(", "), *Hook.Params);
		FEmittedDecl E;
		E.DeclPhase = Sig + TEXT(";\n");
		FString Def = Sig + TEXT("\n{\n");
		const FString Body = PrefixHookCalls(Hook.Body.TrimStartAndEnd(), Qualified);
		if (!Body.IsEmpty())
		{
			Def += WithLine(TEXT("\t") + IndentRegion(Body) + TEXT("\n"), SrcLineOfRegion(Hook.Body, Hook.BodyAt, Line),
							Line);
		}
		Def += TEXT("}\n");
		E.BodyPhase = Def;
		if (!Hook.bExported)
		{
			WrapPrivate(E, PrivNs);
		}
		return E;
	}

	/** A `module` declaration → a namespace holding its verbatim C++ body, emitted ENTIRELY in the
	 *  DECL phase (before any struct that might default from its constants). A non-exported module
	 *  nests inside the per-file detail namespace so same-named private modules never collide. */
	FEmittedDecl EmitModuleInl(const FUetkxModuleDecl& Module, const FString& PrivNs, const FLineCtx& Line)
	{
		FString Out = FString::Printf(TEXT("namespace %s\n{\n"), *Module.Name);
		const FString Body = Module.Body.TrimStartAndEnd();
		if (!Body.IsEmpty())
		{
			Out += WithLine(TEXT("\t") + IndentRegion(Body) + TEXT("\n"),
							SrcLineOfRegion(Module.Body, Module.BodyAt, Line), Line);
		}
		Out += FString::Printf(TEXT("} // namespace %s\n"), *Module.Name);
		FEmittedDecl E;
		E.DeclPhase = Out;
		if (!Module.bExported)
		{
			WrapPrivate(E, PrivNs);
		}
		return E;
	}

	/** The one emitter (per component). */
	class FEmitter
	{
	public:
		FEmitter(const FString& InBasename, const FUetkxComponentDecl& InDecl, TArray<FUetkxDiag>& InDiags,
				 TSet<FString>& InUses, TMap<FString, int32>& InUseAts, const TMap<FString, FString>& InQualified,
				 const FLineCtx& InLine)
			: Basename(InBasename), Decl(InDecl), Diags(InDiags), Uses(InUses), UseAts(InUseAts),
			  Qualified(InQualified), Line(InLine)
		{
		}

		FEmittedDecl Emit();
		bool HasError() const { return bError; }

	private:
		void Fail(const TCHAR* Code, const FString& Msg, int32 At)
		{
			FUetkxDiag D;
			D.Code = Code;
			D.Severity = 0;
			D.Message = Msg;
			D.Offset = At;
			Diags.Add(MoveTemp(D));
			bError = true;
		}

		FString NsLocText(const FString& Value)
		{
			return FString::Printf(TEXT("NSLOCTEXT(\"Uetkx.%s\", \"%s_%d\", \"%s\")"), *Basename, *Decl.Name,
								   ++TextKeyCounter, *CppStringLiteral(Value));
		}

		/** Hook auto-prefix (built-in → Ctx.*, user hooks → Ctx first arg), plus same-file PRIVATE
		 *  reference qualification (private hook calls + `Module::` quals → RuiPriv_<Basename>::…). */
		FString PrefixHooks(const FString& Code) { return PrefixHookCalls(Code, Qualified); }

		/** An embedded expression: rewrite nested markup ranges (jsx scan) to element exprs. */
		FString EmitExpr(const FString& Expr, int32 AbsAt)
		{
			const TArray<int32> Src = FUetkxLexer::ToCodePoints(Expr);
			TArray<FUetkxMarkupRange> Ranges = FUetkxJsxScan::FindMarkupRanges(Src, 0, Src.Num());
			if (Ranges.IsEmpty())
			{
				return PrefixHooks(Expr);
			}
			FString Out;
			int32 Cursor = 0;
			for (const FUetkxMarkupRange& Range : Ranges)
			{
				if (Range.End == -1)
				{
					Fail(TEXT("UETKX2508"), TEXT("unbalanced markup inside expression"), AbsAt);
					return Expr;
				}
				// Short-circuit markup desugars in place (wave G — the Unity Phase 1.5 port;
				// UETKX3002 retired): `cond && <X/>` -> `cond ? <X/> : FRuiNode()` and
				// `cond || <X/>` -> `cond ? FRuiNode() : <X/>` (a default FRuiNode is an empty
				// fragment — renders nothing). `&&`/`||` bind tighter than `?:`, so the bare
				// condition text is safe on the left of the emitted ternary.
				const bool bShortCircuit = !Range.Op.IsEmpty();
				Out += PrefixHooks(
					FUetkxLexer::FromCodePoints(Src, Cursor, (bShortCircuit ? Range.OpPos : Range.Start) - Cursor));
				FUetkxMarkup Parser;
				FUetkxParseResult Pr = Parser.Parse(Src, Range.Start, Range.End);
				if (!Pr.IsOk() || Pr.Nodes.Num() != 1)
				{
					Fail(TEXT("UETKX2508"), TEXT("invalid markup inside expression"), AbsAt);
					return Expr;
				}
				const FString Node = EmitNodeExpr(*Pr.Nodes[0], AbsAt);
				if (bShortCircuit)
				{
					Out += Range.Op == TEXT("&&") ? FString::Printf(TEXT(" ? %s : FRuiNode()"), *Node)
												  : FString::Printf(TEXT(" ? FRuiNode() : %s"), *Node);
				}
				else
				{
					Out += Node;
				}
				Cursor = Range.End;
			}
			Out += PrefixHooks(FUetkxLexer::FromCodePoints(Src, Cursor, Src.Num() - Cursor));
			return Out;
		}

		/** One node as a C++ FRuiNode expression. */
		FString EmitNodeExpr(const FUetkxNode& Node, int32 AbsAt);

		/** Children statements appending into `Ch`. */
		void EmitChildren(FString& Out, const TArray<TSharedPtr<FUetkxNode>>& Children, const FString& Indent,
						  int32 AbsAt);

		/** A directive body: C++ statements ending in `return ( <markup> )` — leading
		 *  statements splice verbatim, the returned markup lowers to Ch.Add(...). */
		void EmitBody(FString& Out, const FString& BodyMarkup, const FString& Indent, int32 AbsAt);

		const FString& Basename;
		const FUetkxComponentDecl& Decl;
		TArray<FUetkxDiag>& Diags;
		TSet<FString>& Uses;					 // component tags this component references (aggregator topo order)
		TMap<FString, int32>& UseAts;			 // tag -> first reference offset (strict-import diagnostics, M4)
		const TMap<FString, FString>& Qualified; // private same-file decl name -> RuiPriv_<Basename>::name
		const FLineCtx& Line;					 // #line directive context (M7)
		int32 TextKeyCounter = 0;
		bool bError = false;
	};

	FString FEmitter::EmitNodeExpr(const FUetkxNode& Node, int32 AbsAt)
	{
		switch (Node.Type)
		{
		case EUetkxNodeType::Text:
			return FString::Printf(TEXT("RUI::TextBlock(%s)"), *NsLocText(Node.Value));
		case EUetkxNodeType::Expr:
			return FString::Printf(TEXT("(%s)"), *EmitExpr(Node.Code, AbsAt));
		case EUetkxNodeType::Comment:
			return FString(); // comments emit nothing
		case EUetkxNodeType::Frag:
		{
			FString Out = TEXT("[&]() -> FRuiNode {\n\t\tTArray<FRuiNode> Ch;\n");
			EmitChildren(Out, Node.Children, TEXT("\t\t"), AbsAt);
			FString Key = TEXT("FRuiKey()");
			for (const FUetkxAttr& Attr : Node.Attrs)
			{
				if (Attr.Name == TEXT("key"))
				{
					Key = Attr.Kind == EUetkxAttrKind::Expr
							  ? FString::Printf(TEXT("FRuiKey(%s)"), *EmitExpr(Attr.Value, AbsAt))
							  : FString::Printf(TEXT("FRuiKey(FName(TEXT(\"%s\")))"), *CppStringLiteral(Attr.Value));
				}
			}
			Out += FString::Printf(TEXT("\t\treturn RUI::Fragment(MoveTemp(Ch), %s);\n\t}()"), *Key);
			return Out;
		}
		case EUetkxNodeType::If:
		case EUetkxNodeType::For:
		case EUetkxNodeType::While:
		case EUetkxNodeType::Match:
		{
			// A directive as a ROOT/expression position lowers to a fragment of its output.
			FString Out = TEXT("[&]() -> FRuiNode {\n\t\tTArray<FRuiNode> Ch;\n");
			TArray<TSharedPtr<FUetkxNode>> Self;
			Self.Add(MakeShared<FUetkxNode>(Node));
			EmitChildren(Out, Self, TEXT("\t\t"), AbsAt);
			Out += TEXT("\t\treturn Ch.Num() == 1 ? MoveTemp(Ch[0]) : RUI::Fragment(MoveTemp(Ch));\n\t}()");
			return Out;
		}
		case EUetkxNodeType::El:
			break;
		}

		// ── element ────────────────────────────────────────────────────────────────────────
		const FTagDef* Tag = HostTags().Find(FName(*Node.Tag));
		// FName match is case-INSENSITIVE; host tags are case-sensitive (1:1 with the Slate class, D-33).
		// A mis-cased host tag (<Textblock/>) must NOT resolve to the canonical widget — it would emit
		// uncompilable C++ (TextBlock has no props struct) with no diagnostic (bughunt CG-3). Compare the
		// authored tag CASE-SENSITIVELY against the matched def's canonical name (its Factory's last `::`
		// segment, a literal — reliable, unlike FName::ToString / a case-insensitive TSet<FString>).
		if (Tag != nullptr && !CanonicalTagName(*Tag).Equals(Node.Tag, ESearchCase::CaseSensitive))
		{
			Fail(TEXT("UETKX0105"),
				 FString::Printf(TEXT("unknown tag <%s> — host tags are case-sensitive (1:1 with the Slate class)"),
								 *Node.Tag),
				 AbsAt + Node.At);
			return FString(TEXT("FRuiNode()"));
		}
		const bool bComponent = Tag == nullptr;
		if (bComponent && !(Node.Tag[0] >= 'A' && Node.Tag[0] <= 'Z'))
		{
			Fail(TEXT("UETKX0105"), FString::Printf(TEXT("unknown tag <%s>"), *Node.Tag), AbsAt + Node.At);
			return FString(TEXT("FRuiNode()"));
		}
		if (bComponent)
		{
			Uses.Add(Node.Tag);
			if (!UseAts.Contains(Node.Tag))
			{
				UseAts.Add(Node.Tag, AbsAt + Node.At);
			}
		}

		// TextBlock special case: single Text attr routes through the factory directly.
		if (!bComponent && Node.Tag == TEXT("TextBlock"))
		{
			FString TextExpr = TEXT("FText::GetEmpty()");
			FString Key = TEXT("FRuiKey()");
			bool bStyled = false;
			FString StyleStmts;
			FString ClassStmts;
			FString RefStmts;
			for (const FUetkxAttr& Attr : Node.Attrs)
			{
				if (Attr.Kind == EUetkxAttrKind::Comment)
				{
					continue;
				}
				if (Attr.Name == TEXT("Text"))
				{
					TextExpr = Attr.Kind == EUetkxAttrKind::Str
								   ? NsLocText(Attr.Value)
								   : FString::Printf(TEXT("(%s)"), *EmitExpr(Attr.Value, AbsAt));
				}
				else if (Attr.Name == TEXT("key"))
				{
					Key = Attr.Kind == EUetkxAttrKind::Expr
							  ? FString::Printf(TEXT("FRuiKey(%s)"), *EmitExpr(Attr.Value, AbsAt))
							  : FString::Printf(TEXT("FRuiKey(FName(TEXT(\"%s\")))"), *CppStringLiteral(Attr.Value));
				}
				else if (Attr.Name == TEXT("classes"))
				{
					bStyled = true;
					if (Attr.Kind == EUetkxAttrKind::Expr)
					{
						ClassStmts += FString::Printf(TEXT("\t\t__P->Classes = (%s);\n"), *EmitExpr(Attr.Value, AbsAt));
					}
					else
					{
						TArray<FString> ClassNames;
						Attr.Value.ParseIntoArrayWS(ClassNames);
						for (const FString& ClassName : ClassNames)
						{
							ClassStmts += FString::Printf(TEXT("\t\t__P->Classes.Add(FName(TEXT(\"%s\")));\n"),
														  *CppStringLiteral(ClassName));
						}
					}
				}
				else if (Attr.Name == TEXT("Ref"))
				{
					// universal reserved prop — see the element path below; expr-only.
					if (Attr.Kind == EUetkxAttrKind::Expr)
					{
						bStyled = true;
						RefStmts += FString::Printf(TEXT("\t\t__P->Ref = (%s);\n"), *EmitExpr(Attr.Value, AbsAt));
					}
					else
					{
						Fail(TEXT("UETKX0105"),
							 TEXT("attribute 'Ref' on <TextBlock> needs an {expr} value (a ref callable)"),
							 AbsAt + Attr.At);
					}
				}
				else if (IsStyleKey(Attr.Name) || Attr.Name.StartsWith(TEXT("Slot.")))
				{
					bStyled = true;
					const bool bSlot = Attr.Name.StartsWith(TEXT("Slot."));
					const FString Value =
						Attr.Kind == EUetkxAttrKind::Expr
							? FString::Printf(TEXT("FRuiValue(%s)"), *EmitExpr(Attr.Value, AbsAt))
							: FString::Printf(TEXT("FRuiValue(TEXT(\"%s\"))"), *CppStringLiteral(Attr.Value));
					StyleStmts += FString::Printf(TEXT("\t\t__%s->Add(FName(TEXT(\"%s\")), %s);\n"),
												  bSlot ? TEXT("Slot") : TEXT("Style"), *Attr.Name, *Value);
				}
				else
				{
					Fail(TEXT("UETKX0105"), FString::Printf(TEXT("unknown attribute '%s' on <TextBlock>"), *Attr.Name),
						 AbsAt + Attr.At);
				}
			}
			if (!bStyled)
			{
				return FString::Printf(TEXT("RUI::TextBlock(%s, %s)"), *TextExpr, *Key);
			}
			FString Out = TEXT("[&]() -> FRuiNode {\n");
			Out += FString::Printf(TEXT("\t\tFRuiNode __N = RUI::TextBlock(%s, %s);\n"), *TextExpr, *Key);
			Out += TEXT("\t\tTSharedRef<FRuiTextBlockProps> __P = "
						"MakeShared<FRuiTextBlockProps>(static_cast<const FRuiTextBlockProps&>(*__N.Props));\n");
			Out += TEXT("\t\tTSharedRef<FRuiStyleDict> __Style = MakeShared<FRuiStyleDict>();\n");
			Out += TEXT("\t\tTSharedRef<FRuiStyleDict> __Slot = MakeShared<FRuiStyleDict>();\n");
			Out += ClassStmts;
			Out += RefStmts;
			Out += StyleStmts;
			Out += TEXT("\t\tif (!__Style->IsEmpty()) { __P->Style = __Style; }\n");
			Out += TEXT("\t\tif (!__Slot->IsEmpty()) { __P->SlotProps = __Slot; }\n");
			Out += TEXT("\t\t__N.Props = __P;\n\t\treturn __N;\n\t}()");
			return Out;
		}

		// A same-file PRIVATE component lives in the per-file detail namespace, so its call site
		// qualifies both the props struct and the wrapper (RuiPriv_<Basename>::…).
		const FString* Priv = bComponent ? Qualified.Find(Node.Tag) : nullptr;
		const FString Prefix = Priv ? *Priv : FString();
		const FString PropsType =
			bComponent ? FString::Printf(TEXT("%sF%sUetkxProps"), *Prefix, *Node.Tag) : Tag->PropsType;
		const FString Factory = bComponent ? Prefix + Node.Tag : Tag->Factory;

		FString Out = TEXT("[&]() -> FRuiNode {\n");
		Out += FString::Printf(TEXT("\t\t%s P;\n"), *PropsType);
		FString Key = TEXT("FRuiKey()");
		FString StyleStmts;
		for (const FUetkxAttr& Attr : Node.Attrs)
		{
			if (Attr.Kind == EUetkxAttrKind::Comment)
			{
				continue;
			}
			if (Attr.Name == TEXT("key"))
			{
				Key = Attr.Kind == EUetkxAttrKind::Expr
						  ? FString::Printf(TEXT("FRuiKey(%s)"), *EmitExpr(Attr.Value, AbsAt))
						  : FString::Printf(TEXT("FRuiKey(FName(TEXT(\"%s\")))"), *CppStringLiteral(Attr.Value));
				continue;
			}
			if (Attr.Kind == EUetkxAttrKind::Spread)
			{
				Fail(TEXT("UETKX3003"), TEXT("spread attributes are post-v1 in .uetkx"), AbsAt + Attr.At);
				continue;
			}
			if (Attr.Name == TEXT("classes"))
			{
				// universal reserved prop: `classes="a b"` (static) or `classes={ expr }`
				// where the expression yields a TArray<FName>.
				if (Attr.Kind == EUetkxAttrKind::Expr)
				{
					Out += FString::Printf(TEXT("\t\tP.Classes = (%s);\n"), *EmitExpr(Attr.Value, AbsAt));
				}
				else
				{
					TArray<FString> ClassNames;
					Attr.Value.ParseIntoArrayWS(ClassNames);
					for (const FString& ClassName : ClassNames)
					{
						Out += FString::Printf(TEXT("\t\tP.Classes.Add(FName(TEXT(\"%s\")));\n"),
											   *CppStringLiteral(ClassName));
					}
				}
				continue;
			}
			if (Attr.Name == TEXT("Ref"))
			{
				// universal reserved prop: `Ref={ expr }` — the props-level host-handle capture
				// (React ref lifecycle: called with the handle on attach, cleared on detach). The
				// expression yields anything assignable to FRuiPropsBase::Ref — a
				// TFunction<void(const FRuiHostHandle&)> lambda or a RUI::Slate::UseFocus
				// handle's `.Ref`. Expr-only: a string form has no meaning for a callable.
				if (Attr.Kind == EUetkxAttrKind::Expr)
				{
					Out += FString::Printf(TEXT("\t\tP.Ref = (%s);\n"), *EmitExpr(Attr.Value, AbsAt));
				}
				else
				{
					Fail(TEXT("UETKX0105"),
						 FString::Printf(TEXT("attribute 'Ref' on <%s> needs an {expr} value (a ref callable)"),
										 *Node.Tag),
						 AbsAt + Attr.At);
				}
				continue;
			}
			if (IsStyleKey(Attr.Name) || Attr.Name.StartsWith(TEXT("Slot.")))
			{
				const bool bSlot = Attr.Name.StartsWith(TEXT("Slot."));
				const FString Value =
					Attr.Kind == EUetkxAttrKind::Expr
						? FString::Printf(TEXT("FRuiValue(%s)"), *EmitExpr(Attr.Value, AbsAt))
						: FString::Printf(TEXT("FRuiValue(TEXT(\"%s\"))"), *CppStringLiteral(Attr.Value));
				StyleStmts += FString::Printf(TEXT("\t\t__%s->Add(FName(TEXT(\"%s\")), %s);\n"),
											  bSlot ? TEXT("Slot") : TEXT("Style"), *Attr.Name, *Value);
				continue;
			}
			if (bComponent)
			{
				// component props are plain fields on the generated struct
				const FString Value = Attr.Kind == EUetkxAttrKind::Expr ? EmitExpr(Attr.Value, AbsAt)
									  : Attr.Kind == EUetkxAttrKind::Bool
										  ? FString(TEXT("true"))
										  : FString::Printf(TEXT("TEXT(\"%s\")"), *CppStringLiteral(Attr.Value));
				Out += FString::Printf(TEXT("\t\tP.%s = %s;\n"), *Attr.Name, *Value);
				continue;
			}
			const EAttrType* AttrType = Tag->Attrs.Find(FName(*Attr.Name));
			if (AttrType == nullptr)
			{
				Fail(TEXT("UETKX0105"), FString::Printf(TEXT("unknown attribute '%s' on <%s>"), *Attr.Name, *Node.Tag),
					 AbsAt + Attr.At);
				continue;
			}
			FString Value;
			if (Attr.Kind == EUetkxAttrKind::Expr)
			{
				// Events: the handler body sees the payload as `Value` (FRuiValue — text/bool/
				// float of the widget event); zero-payload events simply ignore it.
				Value = *AttrType == EAttrType::Event
							? FString::Printf(TEXT("FRuiCallback::Create([=](const FRuiValue& Value) { %s; })"),
											  *PrefixHooks(Attr.Value))
							: FString::Printf(TEXT("(%s)"), *EmitExpr(Attr.Value, AbsAt));
			}
			else if (Attr.Kind == EUetkxAttrKind::Bool)
			{
				Value = TEXT("true");
			}
			else
			{
				switch (*AttrType)
				{
				case EAttrType::Text:
					Value = NsLocText(Attr.Value);
					break;
				case EAttrType::Name:
					Value = FString::Printf(TEXT("FName(TEXT(\"%s\"))"), *CppStringLiteral(Attr.Value));
					break;
				case EAttrType::Float:
				case EAttrType::Int:
					Value = Attr.Value;
					break;
				case EAttrType::Bool:
					Value = Attr.Value;
					break;
				case EAttrType::Margin:
					Value = FString::Printf(TEXT("FMargin(%s)"), *Attr.Value);
					break;
				default:
					Fail(TEXT("UETKX0105"),
						 FString::Printf(TEXT("attribute '%s' on <%s> needs an {expr} value (no string form)"),
										 *Attr.Name, *Node.Tag),
						 AbsAt + Attr.At);
					continue;
				}
			}
			Out += FString::Printf(TEXT("\t\tP.Set%s(%s);\n"), *Attr.Name, *Value);
		}
		if (!StyleStmts.IsEmpty())
		{
			Out += TEXT("\t\tTSharedRef<FRuiStyleDict> __Style = MakeShared<FRuiStyleDict>();\n");
			Out += TEXT("\t\tTSharedRef<FRuiStyleDict> __Slot = MakeShared<FRuiStyleDict>();\n");
			Out += StyleStmts;
			Out += TEXT("\t\tif (!__Style->IsEmpty()) { P.Style = __Style; }\n");
			Out += TEXT("\t\tif (!__Slot->IsEmpty()) { P.SlotProps = __Slot; }\n");
		}
		// Leaf widgets take (Props, Key) — no children argument (the factory arity is the
		// vocabulary's bChildren flag; components always take children via their wrapper).
		if (!bComponent && !Tag->bChildren)
		{
			for (const TSharedPtr<FUetkxNode>& Child : Node.Children)
			{
				if (Child.IsValid() && Child->Type != EUetkxNodeType::Comment)
				{
					Fail(TEXT("UETKX3005"),
						 FString::Printf(TEXT("<%s> is a leaf widget and does not accept children"), *Node.Tag),
						 AbsAt + Node.At);
					break;
				}
			}
			Out += FString::Printf(TEXT("\t\treturn %s(MoveTemp(P), %s);\n\t}()"), *Factory, *Key);
			return Out;
		}
		Out += TEXT("\t\tTArray<FRuiNode> Ch;\n");
		EmitChildren(Out, Node.Children, TEXT("\t\t"), AbsAt);
		Out += FString::Printf(TEXT("\t\treturn %s(MoveTemp(P), MoveTemp(Ch), %s);\n\t}()"), *Factory, *Key);
		return Out;
	}

	void FEmitter::EmitChildren(FString& Out, const TArray<TSharedPtr<FUetkxNode>>& Children, const FString& Indent,
								int32 AbsAt)
	{
		for (const TSharedPtr<FUetkxNode>& ChildPtr : Children)
		{
			if (!ChildPtr.IsValid())
			{
				continue;
			}
			const FUetkxNode& Child = *ChildPtr;
			switch (Child.Type)
			{
			case EUetkxNodeType::Comment:
				break;
			case EUetkxNodeType::If:
			{
				for (int32 b = 0; b < Child.Branches.Num(); ++b)
				{
					const FUetkxIfBranch& Branch = Child.Branches[b];
					Out += FString::Printf(TEXT("%s%s (%s)\n%s{\n"), *Indent, b == 0 ? TEXT("if") : TEXT("else if"),
										   *EmitExpr(Branch.Cond, AbsAt), *Indent);
					EmitBody(Out, Branch.BodyMarkup, Indent + TEXT("\t"), AbsAt + Child.At);
					Out += Indent + TEXT("}\n");
				}
				if (Child.ElseBody.IsSet())
				{
					Out += Indent + TEXT("else\n") + Indent + TEXT("{\n");
					EmitBody(Out, Child.ElseBody.GetValue(), Indent + TEXT("\t"), AbsAt + Child.At);
					Out += Indent + TEXT("}\n");
				}
				break;
			}
			case EUetkxNodeType::For:
			case EUetkxNodeType::While:
			{
				Out += FString::Printf(TEXT("%s%s (%s)\n%s{\n"), *Indent,
									   Child.Type == EUetkxNodeType::For ? TEXT("for") : TEXT("while"),
									   *PrefixHooks(Child.Header), *Indent);
				EmitBody(Out, Child.BodyMarkup, Indent + TEXT("\t"), AbsAt + Child.At);
				Out += Indent + TEXT("}\n");
				break;
			}
			case EUetkxNodeType::Match:
			{
				Out += FString::Printf(TEXT("%sswitch (%s)\n%s{\n"), *Indent, *EmitExpr(Child.Subject, AbsAt), *Indent);
				for (const FUetkxMatchCase& Case : Child.Cases)
				{
					Out += FString::Printf(TEXT("%scase %s:\n%s{\n"), *Indent, *Case.Value, *Indent);
					EmitBody(Out, Case.BodyMarkup, Indent + TEXT("\t"), AbsAt + Child.At);
					Out += Indent + TEXT("\tbreak;\n") + Indent + TEXT("}\n");
				}
				if (Child.DefaultBody.IsSet())
				{
					Out += Indent + TEXT("default:\n") + Indent + TEXT("{\n");
					EmitBody(Out, Child.DefaultBody.GetValue(), Indent + TEXT("\t"), AbsAt + Child.At);
					Out += Indent + TEXT("\tbreak;\n") + Indent + TEXT("}\n");
				}
				Out += Indent + TEXT("}\n");
				break;
			}
			default:
			{
				// TD-015: `{children}` SPLICES the component's forwarded children (Ch.Append) rather
				// than adding a single node — lets a component wrap arbitrary children.
				if (Child.Type == EUetkxNodeType::Expr && Child.Code.TrimStartAndEnd() == TEXT("children"))
				{
					Out += Indent + TEXT("Ch.Append(children);\n");
					break;
				}
				const FString Expr = EmitNodeExpr(Child, AbsAt);
				if (!Expr.IsEmpty())
				{
					Out += FString::Printf(TEXT("%sCh.Add(%s);\n"), *Indent, *Expr);
				}
				break;
			}
			}
		}
	}

	void FEmitter::EmitBody(FString& Out, const FString& BodyMarkup, const FString& Indent, int32 AbsAt)
	{
		const TArray<int32> Body = FUetkxLexer::ToCodePoints(BodyMarkup);
		// the (last) top-level markup return inside the body — bodies are code blocks whose
		// markup arrives via `return ( ... )` (family 0.7 semantics); ONE splitter of record.
		const FUetkxSplitReturn Split = FUetkxFileScan::SplitMarkupReturn(Body, /*bRequireMarkupPeek*/ false);
		const int32 RetAt = Split.ReturnAt;
		const int32 MStart = Split.MStart;
		const int32 MEnd = Split.MEnd;
		if (!Split.bOk)
		{
			// no return -> pure statements (rare; e.g. a guard `continue;`) — splice verbatim
			Out += Indent + PrefixHooks(BodyMarkup).TrimStartAndEnd() + TEXT("\n");
			return;
		}
		const FString Lead = FUetkxLexer::FromCodePoints(Body, 0, RetAt).TrimStartAndEnd();
		if (!Lead.IsEmpty())
		{
			Out += Indent + PrefixHooks(Lead) + TEXT("\n");
		}
		FUetkxMarkup Parser;
		FUetkxParseResult Pr = Parser.Parse(Body, MStart, MEnd);
		if (!Pr.IsOk())
		{
			Fail(*Pr.ErrorCode, Pr.ErrorMsg, AbsAt);
			return;
		}
		for (const TSharedPtr<FUetkxNode>& Node : Pr.Nodes)
		{
			if (Node.IsValid() && Node->Type != EUetkxNodeType::Comment)
			{
				if (Node->Type == EUetkxNodeType::Expr && Node->Code.TrimStartAndEnd() == TEXT("children"))
				{
					Out += Indent + TEXT("Ch.Append(children);\n"); // TD-015 children splice (directive body)
					continue;
				}
				Out += FString::Printf(TEXT("%sCh.Add(%s);\n"), *Indent, *EmitNodeExpr(*Node, AbsAt));
			}
		}
	}

	FEmittedDecl FEmitter::Emit()
	{
		FEmittedDecl E;
		const FString PropsType = FString::Printf(TEXT("F%sUetkxProps"), *Decl.Name);

		// ── DECL phase: the COMPLETE props struct (call sites construct it BY VALUE, so it must be
		// fully defined before every caller — a forward declaration is not enough).
		FString Struct = FString::Printf(TEXT("struct %s final : public FRuiPropsBase\n{\n"), *PropsType);
		for (const FUetkxParam& Param : Decl.Params)
		{
			if (Param.Type.IsEmpty())
			{
				Fail(TEXT("UETKX3004"),
					 FString::Printf(TEXT("param `%s` needs a type: `%s: <Type>`"), *Param.Name, *Param.Name),
					 Decl.NameAt);
				continue;
			}
			Struct +=
				FString::Printf(TEXT("\t%s %s%s;\n"), *Param.Type, *Param.Name,
								Param.Default.IsEmpty() ? TEXT("{}") : *FString::Printf(TEXT(" = %s"), *Param.Default));
		}
		Struct += TEXT("\n\tvirtual bool Equals(const FRuiPropsBase& OtherBase) const override\n\t{\n");
		Struct +=
			FString::Printf(TEXT("\t\tconst %s& O = static_cast<const %s&>(OtherBase);\n"), *PropsType, *PropsType);
		Struct += TEXT("\t\tbool bEq = BaseFieldsEqual(O);\n");
		for (const FUetkxParam& Param : Decl.Params)
		{
			if (Param.Type == TEXT("FText"))
			{
				Struct += FString::Printf(
					TEXT("\t\tbEq = bEq && (%s.IdenticalTo(O.%s) || %s.ToString() == O.%s.ToString());\n"), *Param.Name,
					*Param.Name, *Param.Name, *Param.Name);
			}
			else
			{
				Struct += FString::Printf(TEXT("\t\tbEq = bEq && (%s == O.%s);\n"), *Param.Name, *Param.Name);
			}
		}
		Struct += TEXT("\t\treturn bEq;\n\t}\n};\n");

		// The wrapper's default arguments live on EXACTLY the DECL-phase forward declaration; the
		// BODY-phase definition repeats the signature WITHOUT defaults (repeating them is a C++
		// redefinition error; omitting them on the definition is legal).
		const FString WrapDecl = FString::Printf(
			TEXT("inline FRuiNode %s(%s InProps = %s(), TArray<FRuiNode> InChildren = TArray<FRuiNode>(), FRuiKey "
				 "InKey = FRuiKey());\n"),
			*Decl.Name, *PropsType, *PropsType);
		E.DeclPhase = Struct + WrapDecl;

		// ── BODY phase: the impl (markup lowering — MUST run to populate Uses/UseAts), the identity
		// + hook-signature registrations, the default-free wrapper definition, and (exported only)
		// the named-factory self-registration.
		FString Impl = FString::Printf(
			TEXT("static FRuiNodeArray %s_UetkxImpl(FRuiContext& Ctx, const %s& Props, const TArray<FRuiNode>& "
				 "children)\n{\n"),
			*Decl.Name, *PropsType);
		for (const FUetkxParam& Param : Decl.Params)
		{
			Impl += FString::Printf(TEXT("\tconst auto& %s = Props.%s;\n"), *Param.Name, *Param.Name);
		}
		if (Decl.Returns.Num() <= 1)
		{
			// Single markup return — the legacy shape (byte-stable goldens).
			const FString Setup = Decl.Setup.TrimStartAndEnd();
			if (!Setup.IsEmpty())
			{
				Impl += WithLine(TEXT("\t") + IndentRegion(PrefixHooks(Setup)) + TEXT("\n"),
								 SrcLineOfRegion(Decl.Setup, Decl.SetupAt, Line), Line);
			}
			Impl += FString::Printf(TEXT("\treturn { %s };\n}\n"), *EmitNodeExpr(*Decl.Root, Decl.BodyAt));
		}
		else
		{
			// Wave G early returns — the family's verbatim-emit model (Unity SpliceSetupCodeMarkup):
			// the body's C++ splices VERBATIM (its own control flow branches); every markup
			// `return ( <X/> )` span lowers to `return { <element expr> };` in place.
			const TArray<int32> Body = FUetkxLexer::ToCodePoints(Decl.Body);
			int32 Cursor = 0;
			for (const FUetkxReturnSpan& Span : Decl.Returns)
			{
				const FString Raw = FUetkxLexer::FromCodePoints(Body, Cursor, Span.ReturnAt - Cursor);
				const FString Segment = Raw.TrimStartAndEnd();
				if (!Segment.IsEmpty())
				{
					Impl += WithLine(TEXT("\t") + IndentRegion(PrefixHooks(Segment)) + TEXT("\n"),
									 SrcLineOfRegion(Raw, Decl.BodyAt + Cursor, Line), Line);
				}
				Impl += FString::Printf(TEXT("\treturn { %s };\n"), *EmitNodeExpr(*Span.Root, Decl.BodyAt));
				// step past the authored `;` (we emit our own)
				Cursor = Span.AfterParen;
				while (Cursor < Body.Num() &&
					   (Body[Cursor] == ' ' || Body[Cursor] == '\t' || Body[Cursor] == '\n' || Body[Cursor] == '\r'))
				{
					++Cursor;
				}
				if (Cursor < Body.Num() && Body[Cursor] == ';')
				{
					++Cursor;
				}
			}
			const FString RawTail = FUetkxLexer::FromCodePoints(Body, Cursor, Body.Num() - Cursor);
			const FString Tail = RawTail.TrimStartAndEnd();
			if (!Tail.IsEmpty())
			{
				Impl += WithLine(TEXT("\t") + IndentRegion(PrefixHooks(Tail)) + TEXT("\n"),
								 SrcLineOfRegion(RawTail, Decl.BodyAt + Cursor, Line), Line);
			}
			Impl += TEXT("}\n");
		}
		Impl += FString::Printf(TEXT("static const FName G%sUetkxId = RUI::RegisterComponentId((void*)&%s_UetkxImpl, "
									 "FName(TEXT(\"%s\")));\n"),
								*Decl.Name, *Decl.Name, *Decl.Name);
		Impl += FString::Printf(TEXT("static constexpr uint32 %s_RUI_HOOK_SIG = 0x%08Xu;\n"), *Decl.Name,
								FUetkxFileScan::HookSignature(Decl.HookCalls));
		Impl += FString::Printf(TEXT("inline FRuiNode %s(%s InProps, TArray<FRuiNode> InChildren, FRuiKey "
									 "InKey)\n{\n\treturn RUI::FC(&%s_UetkxImpl, MoveTemp(InProps), "
									 "MoveTemp(InChildren), InKey);\n}\n"),
								*Decl.Name, *PropsType, *Decl.Name);
		if (Decl.bExported)
		{
			Impl += FString::Printf(TEXT("static const bool G%sUetkxFactoryReg = "
										 "RUI::RegisterNamedFactory(FName(TEXT(\"%s\")), []() { return %s(); });\n"),
									*Decl.Name, *Decl.Name, *Decl.Name);
		}
		E.BodyPhase = Impl;

		// A PRIVATE component is TREE-SHAKEN (no named factory, above) and both phases wrap in the
		// per-file detail namespace (A5e). RegisterComponentId is KEPT either way (HMR identity).
		if (!Decl.bExported)
		{
			WrapPrivate(E, PrivNamespaceFor(Basename));
		}
		return E;
	}
} // namespace

bool FUetkxCodegen::IsSellerRepo()
{
	// The sentinel lives at the seller monorepo's project root, ABOVE Plugins/ReactiveUI/, so it is
	// never part of the Fab plugin package a customer installs (D-32a). Cached: a process is entirely
	// in one repo or the other. Tests that need the other banner pass an explicit override instead.
	static const bool bSeller = FPaths::FileExists(FPaths::ProjectDir() / TEXT(".rui-seller-repo"));
	return bSeller;
}

FString FUetkxCodegen::GeneratedCopyrightLine(const FString& Basename, TOptional<bool> bSellerRepoOverride)
{
	const bool bSeller = bSellerRepoOverride.IsSet() ? bSellerRepoOverride.GetValue() : IsSellerRepo();
	if (bSeller)
	{
		return TEXT("// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.\n");
	}
	return FString::Printf(
		TEXT("// Generated by ReactiveUI from %s.uetkx — this generated code belongs to your project.\n"), *Basename);
}

FUetkxCompileOutput FUetkxCodegen::CompileSource(const FString& Source, const FString& Basename,
												 const FString& ProjectRelPath, const IUetkxImportResolver* Resolver,
												 TOptional<bool> bSellerRepoOverride)
{
	FUetkxCompileOutput Out;
	FUetkxFileScanResult Scan = FUetkxFileScan::Scan(Source, Basename);
	Out.Diags = Scan.Diags;
	Out.ExportHash = FUetkxResolve::ExportHash(FUetkxFileScan::ScanPreamble(Source)); // reverse-staleness key (M8)
	if (Scan.HasError())
	{
		return Out;
	}

	FString Inl;
	Inl += TEXT("// AUTO-GENERATED by RUICompile — DO NOT EDIT. Source: ") + Basename + TEXT(".uetkx\n");
	// D-32(a): seller copyright inside this repo, neutral "belongs to your project" banner in a
	// customer project (bughunt CG-1 — was an unconditional seller line stamped into customer output).
	Inl += GeneratedCopyrightLine(Basename, bSellerRepoOverride) + TEXT("\n");
	for (const FString& Include : Scan.PreambleIncludes)
	{
		Inl += Include + TEXT("\n");
	}
	// `import "@Header.h"` host includes (INCLUDE_RETIREMENT_PLAN.md §B) — same emission point,
	// verbatim `#include`, so raw-#include-only fixtures stay byte-identical.
	for (const FUetkxImportDecl& Imp : Scan.Imports)
	{
		if (Imp.bHostInclude)
		{
			Inl += FString::Printf(TEXT("#include \"%s\"\n"), *Imp.Specifier);
		}
	}
	Inl += TEXT("\n");

	// Same-file PRIVATE decls (A5e) get a detail-namespace prefix so their same-file references
	// reach into the wrapper: name -> `RuiPriv_<Basename>::`. Exported decls stay at file scope.
	const FString PrivNs = PrivNamespaceFor(Basename);
	TMap<FString, FString> Qualified;
	for (const FUetkxComponentDecl& D : Scan.Components)
	{
		if (!D.bExported)
		{
			Qualified.Add(D.Name, PrivNs + TEXT("::"));
		}
	}
	for (const FUetkxHookDecl& D : Scan.Hooks)
	{
		if (!D.bExported)
		{
			Qualified.Add(D.Name, PrivNs + TEXT("::"));
		}
	}
	for (const FUetkxModuleDecl& D : Scan.Modules)
	{
		if (!D.bExported)
		{
			Qualified.Add(D.Name, PrivNs + TEXT("::"));
		}
	}

	// #line mapping (M7): breakpoints in the .uetkx bind to the generated body. ProjRel is the
	// machine-stable path (M3); fixtures/tests fall back to `<Basename>.uetkx`.
	FLineCtx Line;
	Line.LineStarts = FUetkxLexer::BuildLineStarts(FUetkxLexer::ToCodePoints(Source));
	Line.ProjRel = ProjectRelPath.IsEmpty() ? Basename + TEXT(".uetkx") : ProjectRelPath;
	Line.InlName = Basename + TEXT(".uetkx.inl");
	Line.bEnabled = true;

	// De-binarized emit (mixed-decl v1) split into the two aggregator phases (M6). Modules emit into
	// the DECL phase FIRST (member defaults may reference them). Components + hooks emit in SOURCE
	// order into both phases. The whole .inl is wrapped `#if defined(RUI_UETKX_DECL_PHASE) … #else …
	// #endif` so the aggregator can include it once per phase.
	TSet<FString> Uses;
	TMap<FString, int32> UseAts;
	FString ModuleDecls, OtherDecls, Bodies;
	for (const TPair<EUetkxDeclKind, int32>& Entry : Scan.Order)
	{
		switch (Entry.Key)
		{
		case EUetkxDeclKind::Component:
		{
			const FUetkxComponentDecl& Decl = Scan.Components[Entry.Value];
			FEmitter Emitter(Basename, Decl, Out.Diags, Uses, UseAts, Qualified, Line);
			const FEmittedDecl E = Emitter.Emit();
			if (Emitter.HasError())
			{
				return Out;
			}
			OtherDecls += E.DeclPhase + TEXT("\n");
			Bodies += E.BodyPhase + TEXT("\n");
			Out.ComponentNames.Add(Decl.Name);
			if (Out.HookSig == 0)
			{
				Out.HookSig = FUetkxFileScan::HookSignature(Decl.HookCalls);
			}
			break;
		}
		case EUetkxDeclKind::Hook:
		{
			const FEmittedDecl E = EmitHookInl(Scan.Hooks[Entry.Value], PrivNs, Qualified, Line);
			OtherDecls += E.DeclPhase + TEXT("\n");
			Bodies += E.BodyPhase + TEXT("\n");
			Out.ComponentNames.Add(Scan.Hooks[Entry.Value].Name);
			break;
		}
		case EUetkxDeclKind::Module:
			ModuleDecls += EmitModuleInl(Scan.Modules[Entry.Value], PrivNs, Line).DeclPhase + TEXT("\n");
			Out.ComponentNames.Add(Scan.Modules[Entry.Value].Name);
			break;
		}
	}
	Inl += TEXT("#if defined(RUI_UETKX_DECL_PHASE)\n");
	Inl += ModuleDecls + OtherDecls;
	Inl += TEXT("#else\n");
	Inl += Bodies;
	Inl += TEXT("#endif\n");

	// Fix up the `#line` restore placeholders (@@R@@) now the whole .inl is assembled: a restore
	// directive on .inl line L points the NEXT line back to the .inl at L+1 (so generated code after
	// a user region maps to the .inl, not the .uetkx).
	if (Inl.Contains(TEXT("@@R@@")))
	{
		TArray<FString> Lines;
		Inl.ParseIntoArray(Lines, TEXT("\n"), false);
		for (int32 i = 0; i < Lines.Num(); ++i)
		{
			if (Lines[i].Contains(TEXT("@@R@@")))
			{
				Lines[i] = Lines[i].Replace(TEXT("@@R@@"), *FString::FromInt(i + 2));
			}
		}
		Inl = FString::Join(Lines, TEXT("\n"));
	}
	// Import resolution + STRICT usage diagnostics (M4). Runs only when a resolver is supplied
	// (driver / fixture harness); tests without one get syntax-only compilation. Resolution
	// happens AFTER emit so the component-tag reference set (UseAts) is available; a resolution
	// ERROR discards the .inl (bOk stays false) so no under-resolved code is committed.
	if (Resolver != nullptr)
	{
		const FString ImporterPath = ProjectRelPath.IsEmpty() ? Basename + TEXT(".uetkx") : ProjectRelPath;
		FUetkxResolve::Apply(Scan, UseAts, ImporterPath, *Resolver, Out.Diags, Out.DepHashes);
		for (const FUetkxDiag& Diag : Out.Diags)
		{
			if (Diag.Severity == 0)
			{
				return Out;
			}
		}
	}

	// EXPORTED decl names — the cross-file-addressable bindings the 2106 global ledger keys on
	// (private decls may collide across files by construction, A5e).
	for (const FUetkxComponentDecl& D : Scan.Components)
	{
		if (D.bExported)
		{
			Out.ExportedNames.Add(D.Name);
		}
	}
	for (const FUetkxHookDecl& D : Scan.Hooks)
	{
		if (D.bExported)
		{
			Out.ExportedNames.Add(D.Name);
		}
	}
	for (const FUetkxModuleDecl& D : Scan.Modules)
	{
		if (D.bExported)
		{
			Out.ExportedNames.Add(D.Name);
		}
	}

	// a component never depends on ITSELF for ordering (recursion is legal in one TU)
	for (const FString& Name : Out.ComponentNames)
	{
		Uses.Remove(Name);
	}
	Out.Uses = Uses.Array();
	Out.Uses.Sort();
	Out.bSupportFile = Scan.Components.IsEmpty(); // no markup → HMR rebuild note, not interp swap
	Out.bOk = true;
	Out.Inl = MoveTemp(Inl);
	return Out;
}

FString FUetkxCodegen::ExportSchemaJson()
{
	auto TypeName = [](EAttrType Type) -> const TCHAR*
	{
		switch (Type)
		{
		case EAttrType::Text:
			return TEXT("text");
		case EAttrType::Float:
			return TEXT("float");
		case EAttrType::Int:
			return TEXT("int");
		case EAttrType::Bool:
			return TEXT("bool");
		case EAttrType::Name:
			return TEXT("name");
		case EAttrType::Margin:
			return TEXT("margin");
		case EAttrType::Vector2:
			return TEXT("vector2");
		case EAttrType::Color:
			return TEXT("color");
		case EAttrType::Event:
			return TEXT("event");
		case EAttrType::Expr:
			return TEXT("expr");
		}
		return TEXT("expr");
	};

	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("v"), 1);

	TSharedRef<FJsonObject> Elements = MakeShared<FJsonObject>();
	TArray<FName> TagNames;
	HostTags().GetKeys(TagNames);
	TagNames.Sort(FNameLexicalLess());
	for (const FName& TagName : TagNames)
	{
		const FTagDef& Tag = HostTags()[TagName];
		TSharedRef<FJsonObject> El = MakeShared<FJsonObject>();
		El->SetStringField(TEXT("factory"), Tag.Factory);
		El->SetBoolField(TEXT("children"), Tag.bChildren);
		if (!Tag.SinceUE.IsEmpty())
		{
			El->SetStringField(TEXT("sinceUE"), Tag.SinceUE);
		}
		TSharedRef<FJsonObject> Attrs = MakeShared<FJsonObject>();
		TArray<FName> AttrNames;
		Tag.Attrs.GetKeys(AttrNames);
		AttrNames.Sort(FNameLexicalLess());
		for (const FName& AttrName : AttrNames)
		{
			Attrs->SetStringField(AttrName.ToString(), TypeName(Tag.Attrs[AttrName]));
		}
		El->SetObjectField(TEXT("attrs"), Attrs);
		Elements->SetObjectField(TagName.ToString(), El);
	}
	Root->SetObjectField(TEXT("elements"), Elements);

	TArray<FString> SortedStyleKeys = StyleKeys().Array();
	SortedStyleKeys.Sort();
	TArray<TSharedPtr<FJsonValue>> StyleArray;
	for (const FString& Key : SortedStyleKeys)
	{
		StyleArray.Add(MakeShared<FJsonValueString>(Key));
	}
	Root->SetArrayField(TEXT("styleKeys"), StyleArray);

	// Slot.* is an OPEN prefix (any name routes to the slot dict); these are the D-33 canon.
	Root->SetStringField(TEXT("slotPrefix"), TEXT("Slot."));
	TArray<TSharedPtr<FJsonValue>> SlotArray;
	for (const TCHAR* Key :
		 {TEXT("Slot.Padding"), TEXT("Slot.HAlign"), TEXT("Slot.VAlign"), TEXT("Slot.Fill"), TEXT("Slot.ZOrder"),
		  TEXT("Slot.Position"), TEXT("Slot.Size"), TEXT("Slot.Offset"), TEXT("Slot.Anchors"), TEXT("Slot.Alignment"),
		  TEXT("Slot.AutoSize"), TEXT("Slot.Role"), TEXT("Slot.SizeRule"), TEXT("Slot.SizeValue"), TEXT("Slot.MinSize"),
		  TEXT("Slot.Resizable")})
	{
		SlotArray.Add(MakeShared<FJsonValueString>(Key));
	}
	Root->SetArrayField(TEXT("slotKeys"), SlotArray);

	TArray<TSharedPtr<FJsonValue>> HookArray;
	for (const FString& Hook : FUetkxFileScan::HookNames())
	{
		HookArray.Add(MakeShared<FJsonValueString>(Hook));
	}
	Root->SetArrayField(TEXT("hooks"), HookArray);

	// TD-016: per-event payload KIND — the FRuiValue field an event handler's `Value` carries, so
	// the LSP can complete `Value.<Field>` typed by the event (text -> Value.TextValue, etc.). Keyed
	// by event attr name (payload is per event name in v1). "void" = zero-payload (OnClicked).
	auto EventPayloadKind = [](const FString& EventName) -> const TCHAR*
	{
		if (EventName == TEXT("OnTextChanged") || EventName == TEXT("OnTextCommitted"))
		{
			return TEXT("text");
		}
		if (EventName == TEXT("OnCheckStateChanged"))
		{
			return TEXT("bool");
		}
		if (EventName == TEXT("OnValueChanged"))
		{
			return TEXT("float");
		}
		return TEXT("void");
	};
	TSharedRef<FJsonObject> EventPayloads = MakeShared<FJsonObject>();
	TSet<FString> SeenEvents;
	for (const FName& TagName : TagNames)
	{
		const FTagDef& Tag = HostTags()[TagName];
		TArray<FName> EvAttrNames;
		Tag.Attrs.GetKeys(EvAttrNames);
		EvAttrNames.Sort(FNameLexicalLess());
		for (const FName& AttrName : EvAttrNames)
		{
			if (Tag.Attrs[AttrName] == EAttrType::Event)
			{
				const FString EvName = AttrName.ToString();
				if (!SeenEvents.Contains(EvName))
				{
					SeenEvents.Add(EvName);
					EventPayloads->SetStringField(EvName, EventPayloadKind(EvName));
				}
			}
		}
	}
	Root->SetObjectField(TEXT("eventPayloads"), EventPayloads);

	FString Out;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
	FJsonSerializer::Serialize(Root, Writer);
	return Out;
}

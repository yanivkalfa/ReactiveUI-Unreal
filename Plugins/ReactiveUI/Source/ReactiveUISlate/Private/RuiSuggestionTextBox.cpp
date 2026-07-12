// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// TD-012 tail — SSuggestionTextBox wrapper. OnShowingSuggestions delegates to ComputeSuggestions
// (case-insensitive substring match over the candidate list); Text is controlled (D-16 caret rule).

#include "RuiSuggestionTextBox.h"

#include "RuiElementAdapter.h"
#include "Widgets/Input/SSuggestionTextBox.h"

void SRuiSuggestionTextBox::Construct(const FArguments& InArgs)
{
	// clang-format off
	ChildSlot
	[
		SAssignNew(Box, SSuggestionTextBox)
		.HintText(InArgs._HintText)
		.OnShowingSuggestions(this, &SRuiSuggestionTextBox::HandleShowingSuggestions)
		.OnTextChanged(this, &SRuiSuggestionTextBox::HandleTextChanged)
		.OnTextCommitted(this, &SRuiSuggestionTextBox::HandleTextCommitted)
	];
	// clang-format on
}

void SRuiSuggestionTextBox::SetText(const FText& InText)
{
	// D-16 caret rule: compare against the widget's LIVE text so typing survives the round-trip.
	if (Box.IsValid() && !Box->GetText().EqualTo(InText))
	{
		Box->SetText(InText);
	}
}

FText SRuiSuggestionTextBox::GetText() const
{
	return Box.IsValid() ? Box->GetText() : FText::GetEmpty();
}

TArray<FString> SRuiSuggestionTextBox::ComputeSuggestions(const FString& Input) const
{
	TArray<FString> Out;
	if (Input.IsEmpty())
	{
		return Out; // nothing typed -> no suggestions
	}
	for (const FString& Candidate : Suggestions)
	{
		if (Candidate.Contains(Input, ESearchCase::IgnoreCase))
		{
			Out.Add(Candidate);
		}
	}
	return Out;
}

void SRuiSuggestionTextBox::HandleShowingSuggestions(const FString& Input, TArray<FString>& OutSuggestions)
{
	OutSuggestions = ComputeSuggestions(Input);
}

void SRuiSuggestionTextBox::HandleTextChanged(const FText& InText)
{
	if (OnTextChangedCb.IsBound())
	{
		OnTextChangedCb.Execute(FRuiValue(InText));
	}
}

void SRuiSuggestionTextBox::HandleTextCommitted(const FText& InText, ETextCommit::Type)
{
	if (OnTextCommittedCb.IsBound())
	{
		OnTextCommittedCb.Execute(FRuiValue(InText));
	}
}

// ─────────────────────────────────────────────────────────────────────────────────────────────
// Adapter (Leaf)
// ─────────────────────────────────────────────────────────────────────────────────────────────

class FRuiSuggestionTextBoxAdapter final : public IRuiElementAdapter
{
public:
	virtual ERuiChildKind GetChildKind() const override { return ERuiChildKind::Leaf; }
	virtual bool IsPoolable() const override { return false; }

	virtual TSharedRef<SWidget> CreateWidget(const FRuiPropsBase& Props, const TSharedPtr<FRuiEventProxy>&) override
	{
		const FRuiSuggestionTextBoxProps& P = static_cast<const FRuiSuggestionTextBoxProps&>(Props);
		return SNew(SRuiSuggestionTextBox).HintText(P.HasHintText() ? P.HintText : FText::GetEmpty());
	}

	virtual void ApplyDiff(SWidget& Widget, const FRuiPropsBase* Old, const FRuiPropsBase& New) override
	{
		SRuiSuggestionTextBox& W = static_cast<SRuiSuggestionTextBox&>(Widget);
		const FRuiSuggestionTextBoxProps& N = static_cast<const FRuiSuggestionTextBoxProps&>(New);
		const FRuiSuggestionTextBoxProps* O = static_cast<const FRuiSuggestionTextBoxProps*>(Old);
		if (N.HasSuggestions() && (O == nullptr || !O->HasSuggestions() || !(N.Suggestions == O->Suggestions)))
		{
			W.SetSuggestionsList(N.Suggestions);
		}
		if (N.HasText())
		{
			W.SetText(N.Text); // controlled (skip-when-equal against live text inside)
		}
		if (N.HasOnTextChanged())
		{
			W.SetOnTextChanged(N.OnTextChanged);
		}
		if (N.HasOnTextCommitted())
		{
			W.SetOnTextCommitted(N.OnTextCommitted);
		}
	}
};

// ─────────────────────────────────────────────────────────────────────────────────────────────
// Type, factory, registration
// ─────────────────────────────────────────────────────────────────────────────────────────────

namespace RUI::Slate
{
	FRuiElementTypeId SuggestionTextBoxType()
	{
		return RUI::InternElementType(FName(TEXT("SuggestionTextBox")));
	}

	FRuiNode SuggestionTextBox(FRuiSuggestionTextBoxProps Props, FRuiKey Key)
	{
		FRuiNode Node;
		Node.Kind = ERuiNodeKind::Host;
		Node.ElementType = SuggestionTextBoxType();
		Node.Props = MakeShared<FRuiSuggestionTextBoxProps>(MoveTemp(Props));
		Node.Key = Key;
		return Node;
	}

	namespace Detail
	{
		void RegisterSuggestionTextBoxAdapter()
		{
			RegisterAdapter(SuggestionTextBoxType(), MakeUnique<FRuiSuggestionTextBoxAdapter>());
		}
	} // namespace Detail
} // namespace RUI::Slate

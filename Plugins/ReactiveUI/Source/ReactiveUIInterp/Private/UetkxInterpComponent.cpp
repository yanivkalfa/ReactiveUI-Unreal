// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "UetkxInterpComponent.h"

#include "RuiContext.h"
#include "RuiCoreElements.h" // RUI::TextBlock
#include "RuiPropsBase.h"
#include "UetkxInterpElements.h"
#include "UetkxLexer.h"
#include "UetkxMarkup.h"

namespace
{
	const TSet<FString>& InterpStyleKeys()
	{
		return FUetkxInterpElements::StyleKeys();
	}

	const TSet<FString>& EventAttrNames()
	{
		static const TSet<FString> Names = {TEXT("OnClicked"), TEXT("OnTextChanged"), TEXT("OnTextCommitted"),
											TEXT("OnCheckStateChanged"), TEXT("OnValueChanged")};
		return Names;
	}

	FRuiKey MakeKey(const FRuiValue& Value)
	{
		switch (Value.Kind)
		{
		case FRuiValue::EKind::Int:
			return FRuiKey(static_cast<int32>(Value.IntValue));
		case FRuiValue::EKind::Name:
			return FRuiKey(Value.NameValue);
		case FRuiValue::EKind::String:
			return FRuiKey(FName(*Value.StringValue));
		default:
			return FRuiKey();
		}
	}

	/** Split C++ text into top-level statements on `;` (brace/paren/bracket-aware, string-
	 *  safe via the lexer). */
	TArray<FString> SplitStatements(const FString& Code)
	{
		TArray<FString> Out;
		const TArray<int32> Cp = FUetkxLexer::ToCodePoints(Code);
		const int32 N = Cp.Num();
		int32 Depth = 0;
		int32 Start = 0;
		int32 i = 0;
		while (i < N)
		{
			const int32 j = FUetkxLexer::SkipNoncode(Cp, i);
			if (j != i)
			{
				i = j;
				continue;
			}
			const int32 C = Cp[i];
			if (C == '{' || C == '(' || C == '[')
			{
				++Depth;
			}
			else if (C == '}' || C == ')' || C == ']')
			{
				--Depth;
			}
			else if (C == ';' && Depth == 0)
			{
				Out.Add(FUetkxLexer::FromCodePoints(Cp, Start, i - Start).TrimStartAndEnd());
				Start = i + 1;
			}
			++i;
		}
		const FString Tail = FUetkxLexer::FromCodePoints(Cp, Start, N - Start).TrimStartAndEnd();
		if (!Tail.IsEmpty())
		{
			Out.Add(Tail);
		}
		return Out;
	}
} // namespace

void FUetkxInterpDef::Note(const FString& Message)
{
	Notes.AddUnique(Message);
}

FUetkxInterpDef::FInterpEvent FUetkxInterpDef::PrepareEvent(const FString& ExprText)
{
	FInterpEvent Event;
	// the interpretable event shape: `SetterName(arg?)` over a known state setter
	const FString Trimmed = ExprText.TrimStartAndEnd();
	int32 ParenAt = INDEX_NONE;
	if (!Trimmed.FindChar('(', ParenAt) || !Trimmed.EndsWith(TEXT(")")))
	{
		Note(FString::Printf(TEXT("event `%s` is not a setter call — rebuild required for full behavior"), *Trimmed));
		return Event;
	}
	const FString Callee = Trimmed.Left(ParenAt).TrimStartAndEnd();
	if (!KnownSetters.Contains(Callee))
	{
		Note(FString::Printf(TEXT("event handler `%s` is not a state setter — rebuild required for full behavior"),
							 *Callee));
		return Event;
	}
	const FString ArgText = Trimmed.Mid(ParenAt + 1, Trimmed.Len() - ParenAt - 2).TrimStartAndEnd();
	if (!ArgText.IsEmpty())
	{
		FString Why;
		Event.Arg = FUetkxExprVm::Parse(ArgText, &Why);
		if (!Event.Arg.IsValid())
		{
			Note(FString::Printf(TEXT("event arg `%s`: %s — rebuild required"), *ArgText, *Why));
			return Event;
		}
	}
	Event.SetterName = Callee;
	Event.bValid = true;
	return Event;
}

void FUetkxInterpDef::ParseBindings(const FString& Code, TArray<FAliasOp>& OutOps, bool bAllowHooks)
{
	for (const FString& Statement : SplitStatements(Code))
	{
		if (Statement.IsEmpty() || Statement.StartsWith(TEXT("//")))
		{
			continue;
		}
		// `auto [A, B] = UseState<...>(init)` (possibly Ctx.-qualified)
		if (bAllowHooks && Statement.StartsWith(TEXT("auto [")))
		{
			const int32 Close = Statement.Find(TEXT("]"));
			int32 EqAt = Statement.Find(TEXT("="), ESearchCase::CaseSensitive, ESearchDir::FromStart, Close);
			if (Close != INDEX_NONE && EqAt != INDEX_NONE)
			{
				TArray<FString> Names;
				Statement.Mid(6, Close - 6).ParseIntoArray(Names, TEXT(","), true);
				const FString Rhs = Statement.Mid(EqAt + 1).TrimStartAndEnd();
				FString HookName = Rhs;
				HookName.RemoveFromStart(TEXT("Ctx."));
				if (Names.Num() == 2 && HookName.StartsWith(TEXT("UseState")))
				{
					FHookOp Op;
					Op.Kind = TEXT("UseState");
					Op.ValueName = Names[0].TrimStartAndEnd();
					Op.SetterName = Names[1].TrimStartAndEnd();
					const int32 ArgOpen = HookName.Find(TEXT("("));
					if (ArgOpen != INDEX_NONE && HookName.EndsWith(TEXT(")")))
					{
						const FString Init = HookName.Mid(ArgOpen + 1, HookName.Len() - ArgOpen - 2);
						if (!Init.TrimStartAndEnd().IsEmpty())
						{
							FString Why;
							Op.Init = FUetkxExprVm::Parse(Init, &Why);
							if (!Op.Init.IsValid())
							{
								Note(FString::Printf(TEXT("UseState init `%s`: %s — Null under interp"),
													 *Init.TrimStartAndEnd(), *Why));
							}
						}
					}
					KnownSetters.Add(Op.SetterName);
					HookPlan.Add(MoveTemp(Op));
					continue;
				}
				Note(FString::Printf(TEXT("hook binding `%s` not interpreted"), *Statement));
				continue;
			}
		}
		// other hook calls (UseEffect/UseMemo/...) — structural no-ops under interp
		{
			FString Bare = Statement;
			Bare.RemoveFromStart(TEXT("Ctx."));
			bool bHookStmt = false;
			for (const FString& Hook : FUetkxFileScan::HookNames())
			{
				if (Bare.StartsWith(Hook) || Bare.Contains(TEXT("::") + Hook) ||
					Bare.Contains(TEXT(" ") + Hook + TEXT("(")) || Bare.Contains(TEXT("= ") + Hook + TEXT("<")))
				{
					bHookStmt = true;
					break;
				}
			}
			if (bHookStmt)
			{
				Note(TEXT("effects/memo hooks are not interpreted — rebuild for full behavior"));
				continue;
			}
		}
		// `TYPE NAME = RHS` value/setter aliases
		{
			const int32 EqAt = Statement.Find(TEXT(" = "));
			if (EqAt != INDEX_NONE)
			{
				FString Left = Statement.Left(EqAt).TrimStartAndEnd();
				const FString Rhs = Statement.Mid(EqAt + 3).TrimStartAndEnd();
				int32 LastSpace = INDEX_NONE;
				Left.FindLastChar(' ', LastSpace);
				const FString VarName = LastSpace == INDEX_NONE ? Left : Left.Mid(LastSpace + 1);
				if (!VarName.IsEmpty() && VarName[0] != '*' && VarName[0] != '&')
				{
					if (KnownSetters.Contains(Rhs))
					{
						FAliasOp Op;
						Op.Name = VarName;
						Op.SetterOf = Rhs;
						KnownSetters.Add(VarName);
						OutOps.Add(MoveTemp(Op));
						continue;
					}
					FString Why;
					TSharedPtr<FUetkxExprNode> Expr = FUetkxExprVm::Parse(Rhs, &Why);
					if (Expr.IsValid())
					{
						FAliasOp Op;
						Op.Name = VarName;
						Op.ValueExpr = Expr;
						OutOps.Add(MoveTemp(Op));
						continue;
					}
				}
			}
		}
		Note(FString::Printf(TEXT("setup statement not interpreted: `%s`"), *Statement.Left(60)));
	}
}

void FUetkxInterpDef::ParseSetup(const FString& Setup)
{
	ParseBindings(Setup, SetupAliases, /*bAllowHooks*/ true);
}

TSharedPtr<FUetkxInterpDef::FPrepBody> FUetkxInterpDef::PrepareBody(const FString& BodyMarkup)
{
	TSharedPtr<FPrepBody> Body = MakeShared<FPrepBody>();
	const TArray<int32> Cp = FUetkxLexer::ToCodePoints(BodyMarkup);
	const FUetkxSplitReturn Split = FUetkxFileScan::SplitMarkupReturn(Cp, /*bRequireMarkupPeek*/ false);
	if (!Split.bOk)
	{
		Note(TEXT("directive body without a markup return — skipped under interp"));
		return Body;
	}
	const FString Lead = FUetkxLexer::FromCodePoints(Cp, 0, Split.ReturnAt);
	if (!Lead.TrimStartAndEnd().IsEmpty())
	{
		ParseBindings(Lead, Body->Lead, /*bAllowHooks*/ false);
	}
	FUetkxMarkup Parser;
	FUetkxParseResult Result = Parser.Parse(Cp, Split.MStart, Split.MEnd);
	if (!Result.IsOk())
	{
		Note(FString::Printf(TEXT("directive markup error %s — skipped under interp"), *Result.ErrorCode));
		return Body;
	}
	for (const TSharedPtr<FUetkxNode>& Node : Result.Nodes)
	{
		if (Node.IsValid() && Node->Type != EUetkxNodeType::Comment)
		{
			if (TSharedPtr<FPrepNode> Prepared = PrepareNode(*Node))
			{
				Body->Nodes.Add(Prepared);
			}
		}
	}
	return Body;
}

TSharedPtr<FUetkxInterpDef::FPrepNode> FUetkxInterpDef::PrepareNode(const FUetkxNode& Node)
{
	TSharedPtr<FPrepNode> Prep = MakeShared<FPrepNode>();
	switch (Node.Type)
	{
	case EUetkxNodeType::Comment:
		return nullptr;
	case EUetkxNodeType::Text:
		Prep->Kind = FPrepNode::EKind::Text;
		Prep->Text = Node.Value;
		return Prep;
	case EUetkxNodeType::Expr:
	{
		Prep->Kind = FPrepNode::EKind::Expr;
		FString Why;
		Prep->Expr = FUetkxExprVm::Parse(Node.Code, &Why);
		if (!Prep->Expr.IsValid())
		{
			Note(FString::Printf(TEXT("{expr} `%s`: %s — skipped under interp"), *Node.Code.TrimStartAndEnd(), *Why));
			return nullptr;
		}
		return Prep;
	}
	case EUetkxNodeType::Frag:
	{
		Prep->Kind = FPrepNode::EKind::Frag;
		for (const TSharedPtr<FUetkxNode>& Child : Node.Children)
		{
			if (Child.IsValid() && Child->Type != EUetkxNodeType::Comment)
			{
				if (TSharedPtr<FPrepNode> Prepared = PrepareNode(*Child))
				{
					Prep->Children.Add(Prepared);
				}
			}
		}
		return Prep;
	}
	case EUetkxNodeType::If:
	{
		Prep->Kind = FPrepNode::EKind::If;
		for (const FUetkxIfBranch& Branch : Node.Branches)
		{
			FPrepNode::FPrepBranch PrepBranch;
			FString Why;
			PrepBranch.Cond = FUetkxExprVm::Parse(Branch.Cond, &Why);
			if (!PrepBranch.Cond.IsValid())
			{
				Note(FString::Printf(TEXT("@if condition `%s`: %s — branch skipped"), *Branch.Cond.TrimStartAndEnd(),
									 *Why));
			}
			PrepBranch.Body = PrepareBody(Branch.BodyMarkup);
			Prep->Branches.Add(MoveTemp(PrepBranch));
		}
		if (Node.ElseBody.IsSet())
		{
			Prep->ElseBody = PrepareBody(Node.ElseBody.GetValue());
		}
		return Prep;
	}
	case EUetkxNodeType::For:
	case EUetkxNodeType::While:
	{
		Prep->Kind = Node.Type == EUetkxNodeType::For ? FPrepNode::EKind::For : FPrepNode::EKind::While;
		Prep->LoopBody = PrepareBody(Node.BodyMarkup);
		if (Node.Type == EUetkxNodeType::While)
		{
			FString Why;
			Prep->LoopCond = FUetkxExprVm::Parse(Node.Header, &Why);
			if (!Prep->LoopCond.IsValid())
			{
				Note(FString::Printf(TEXT("@while condition: %s — skipped"), *Why));
				return nullptr;
			}
			return Prep;
		}
		// C-style header: `TYPE NAME = init; cond; ++NAME` (the interpretable loop shape)
		TArray<FString> Parts;
		Node.Header.ParseIntoArray(Parts, TEXT(";"), false);
		if (Parts.Num() == 3)
		{
			const FString Init = Parts[0].TrimStartAndEnd();
			const int32 EqAt = Init.Find(TEXT("="));
			if (EqAt != INDEX_NONE)
			{
				FString Left = Init.Left(EqAt).TrimStartAndEnd();
				int32 LastSpace = INDEX_NONE;
				Left.FindLastChar(' ', LastSpace);
				Prep->LoopVar = LastSpace == INDEX_NONE ? Left : Left.Mid(LastSpace + 1);
				FString Why;
				Prep->LoopInit = FUetkxExprVm::Parse(Init.Mid(EqAt + 1), &Why);
				Prep->LoopCond = FUetkxExprVm::Parse(Parts[1], &Why);
				const FString Step = Parts[2].TrimStartAndEnd();
				if (Step == TEXT("++") + Prep->LoopVar || Step == Prep->LoopVar + TEXT("++"))
				{
					Prep->LoopStep = 1;
				}
				else if (Step == TEXT("--") + Prep->LoopVar || Step == Prep->LoopVar + TEXT("--"))
				{
					Prep->LoopStep = -1;
				}
			}
		}
		if (Prep->LoopVar.IsEmpty() || !Prep->LoopInit.IsValid() || !Prep->LoopCond.IsValid() || Prep->LoopStep == 0)
		{
			Note(FString::Printf(TEXT("@for header `%s` is not the interpretable C-style shape — skipped"),
								 *Node.Header.TrimStartAndEnd()));
			return nullptr;
		}
		return Prep;
	}
	case EUetkxNodeType::Match:
	{
		Prep->Kind = FPrepNode::EKind::Match;
		FString Why;
		Prep->Subject = FUetkxExprVm::Parse(Node.Subject, &Why);
		if (!Prep->Subject.IsValid())
		{
			Note(FString::Printf(TEXT("@match subject: %s — skipped"), *Why));
			return nullptr;
		}
		for (const FUetkxMatchCase& Case : Node.Cases)
		{
			FPrepNode::FPrepCase PrepCase;
			PrepCase.Value = FUetkxExprVm::Parse(Case.Value, &Why);
			if (!PrepCase.Value.IsValid())
			{
				Note(FString::Printf(TEXT("@case value: %s — case skipped"), *Why));
				continue;
			}
			PrepCase.Body = PrepareBody(Case.BodyMarkup);
			Prep->Cases.Add(MoveTemp(PrepCase));
		}
		if (Node.DefaultBody.IsSet())
		{
			Prep->DefaultBody = PrepareBody(Node.DefaultBody.GetValue());
		}
		return Prep;
	}
	case EUetkxNodeType::El:
		break;
	}

	// element / cross-component reference
	Prep->Tag = FName(*Node.Tag);
	Prep->Kind = FUetkxInterpElements::Has(Prep->Tag) ? FPrepNode::EKind::El : FPrepNode::EKind::Component;
	for (const FUetkxAttr& Attr : Node.Attrs)
	{
		if (Attr.Kind == EUetkxAttrKind::Comment)
		{
			continue;
		}
		if (Attr.Kind == EUetkxAttrKind::Spread)
		{
			Note(TEXT("spread attrs are post-v1 — skipped"));
			continue;
		}
		if (Attr.Name == TEXT("key"))
		{
			if (Attr.Kind == EUetkxAttrKind::Expr)
			{
				FString Why;
				Prep->KeyExpr = FUetkxExprVm::Parse(Attr.Value, &Why);
				if (!Prep->KeyExpr.IsValid())
				{
					Note(FString::Printf(TEXT("key expr: %s — unkeyed under interp"), *Why));
				}
			}
			else
			{
				Prep->LiteralKey = FRuiValue(FName(*Attr.Value));
				Prep->bHasLiteralKey = true;
			}
			continue;
		}
		if (Attr.Name == TEXT("classes"))
		{
			if (Attr.Kind == EUetkxAttrKind::Str)
			{
				TArray<FString> ClassNames;
				Attr.Value.ParseIntoArrayWS(ClassNames);
				for (const FString& ClassName : ClassNames)
				{
					Prep->Classes.Add(FName(*ClassName));
				}
			}
			else
			{
				Note(TEXT("classes={expr} not interpreted — static classes only"));
			}
			continue;
		}
		const bool bStyleKey = InterpStyleKeys().Contains(Attr.Name) || Attr.Name.StartsWith(TEXT("Slot."));
		if (EventAttrNames().Contains(Attr.Name) && !bStyleKey)
		{
			if (Attr.Kind == EUetkxAttrKind::Expr)
			{
				Prep->Events.Add(MakeTuple(FName(*Attr.Name), PrepareEvent(Attr.Value)));
			}
			continue;
		}
		if (Attr.Kind == EUetkxAttrKind::Bool)
		{
			(bStyleKey ? Prep->LiteralStyle : Prep->LiteralAttrs).Add(MakeTuple(FName(*Attr.Name), FRuiValue(true)));
			continue;
		}
		if (Attr.Kind == EUetkxAttrKind::Str)
		{
			(bStyleKey ? Prep->LiteralStyle : Prep->LiteralAttrs)
				.Add(MakeTuple(FName(*Attr.Name), FRuiValue(Attr.Value)));
			continue;
		}
		FString Why;
		TSharedPtr<FUetkxExprNode> Expr = FUetkxExprVm::Parse(Attr.Value, &Why);
		if (!Expr.IsValid())
		{
			Note(FString::Printf(TEXT("attr %s: %s — dropped under interp"), *Attr.Name, *Why));
			continue;
		}
		(bStyleKey ? Prep->ExprStyle : Prep->ExprAttrs).Add(MakeTuple(FName(*Attr.Name), Expr));
	}
	for (const TSharedPtr<FUetkxNode>& Child : Node.Children)
	{
		if (Child.IsValid() && Child->Type != EUetkxNodeType::Comment)
		{
			if (TSharedPtr<FPrepNode> Prepared = PrepareNode(*Child))
			{
				Prep->Children.Add(Prepared);
			}
		}
	}
	if (Prep->Kind == FPrepNode::EKind::Component && (Prep->LiteralAttrs.Num() > 0 || Prep->ExprAttrs.Num() > 0))
	{
		Note(FString::Printf(TEXT("<%s> props are not passed under interp (default props via the named factory)"),
							 *Prep->Tag.ToString()));
	}
	return Prep;
}

TSharedPtr<FUetkxInterpDef> FUetkxInterpDef::Build(const FUetkxComponentDecl& Decl)
{
	FUetkxInterpElements::RegisterBuiltins();
	TSharedPtr<FUetkxInterpDef> Def = MakeShared<FUetkxInterpDef>();
	Def->Name = Decl.Name;
	Def->HookSig = FUetkxFileScan::HookSignature(Decl.HookCalls);
	for (const FUetkxParam& Param : Decl.Params)
	{
		TSharedPtr<FUetkxExprNode> Default;
		if (!Param.Default.IsEmpty())
		{
			FString Why;
			Default = FUetkxExprVm::Parse(Param.Default, &Why);
		}
		Def->ParamDefaults.Add(MakeTuple(Param.Name, Default));
	}
	if (Decl.Params.Num() > 0)
	{
		Def->Note(TEXT("params render their DEFAULTS under interp (compiled props are opaque)"));
	}
	Def->ParseSetup(Decl.Setup);
	for (const TSharedPtr<FUetkxNode>& Node : Decl.WindowNodes)
	{
		if (Node.IsValid() && Node->Type != EUetkxNodeType::Comment)
		{
			if (TSharedPtr<FPrepNode> Prepared = Def->PrepareNode(*Node))
			{
				Def->Window.Add(Prepared);
			}
		}
	}
	return Def;
}

void FUetkxInterpDef::RunAliases(const TArray<FAliasOp>& Ops, FUetkxExprEnv& Env, FRenderCtx& Render)
{
	for (const FAliasOp& Op : Ops)
	{
		if (!Op.SetterOf.IsEmpty())
		{
			if (const TRuiSetter<FRuiValue>* Setter = Render.Setters.Find(Op.SetterOf))
			{
				Render.Setters.Add(Op.Name, *Setter);
			}
			continue;
		}
		if (Op.ValueExpr.IsValid())
		{
			Env.Vars.Add(Op.Name, FUetkxExprVm::Eval(*Op.ValueExpr, Env, nullptr));
		}
	}
}

FRuiNode FUetkxInterpDef::EmitElement(const FPrepNode& Node, FUetkxExprEnv& Env, FRenderCtx& Render)
{
	FUetkxInterpElementInputs Inputs;
	for (const TTuple<FName, FRuiValue>& Attr : Node.LiteralAttrs)
	{
		Inputs.Attrs.Add(Attr.Get<0>(), Attr.Get<1>());
	}
	for (const TTuple<FName, TSharedPtr<FUetkxExprNode>>& Attr : Node.ExprAttrs)
	{
		Inputs.Attrs.Add(Attr.Get<0>(), FUetkxExprVm::Eval(*Attr.Get<1>(), Env, nullptr));
	}
	if (Node.LiteralStyle.Num() > 0 || Node.ExprStyle.Num() > 0)
	{
		TSharedRef<FRuiStyleDict> Style = MakeShared<FRuiStyleDict>();
		TSharedRef<FRuiStyleDict> Slot = MakeShared<FRuiStyleDict>();
		auto Route = [&Style, &Slot](FName Key, FRuiValue Value)
		{
			if (Key.ToString().StartsWith(TEXT("Slot.")))
			{
				Slot->Add(Key, MoveTemp(Value));
			}
			else
			{
				Style->Add(Key, MoveTemp(Value));
			}
		};
		for (const TTuple<FName, FRuiValue>& Pair : Node.LiteralStyle)
		{
			Route(Pair.Get<0>(), Pair.Get<1>());
		}
		for (const TTuple<FName, TSharedPtr<FUetkxExprNode>>& Pair : Node.ExprStyle)
		{
			Route(Pair.Get<0>(), FUetkxExprVm::Eval(*Pair.Get<1>(), Env, nullptr));
		}
		if (!Style->IsEmpty())
		{
			Inputs.Style = Style;
		}
		if (!Slot->IsEmpty())
		{
			Inputs.SlotProps = Slot;
		}
	}
	Inputs.Classes = Node.Classes;
	for (const TTuple<FName, FInterpEvent>& Pair : Node.Events)
	{
		const FInterpEvent& Event = Pair.Get<1>();
		if (!Event.bValid)
		{
			continue;
		}
		const TRuiSetter<FRuiValue>* Setter = Render.Setters.Find(Event.SetterName);
		if (!Setter)
		{
			continue;
		}
		// captured by VALUE: the render-time env snapshot — the same semantics as compiled
		// code capturing locals by value.
		TMap<FString, FRuiValue> EnvSnapshot = Env.Vars;
		for (const FUetkxExprEnv* Scope = Env.Parent; Scope; Scope = Scope->Parent)
		{
			for (const TPair<FString, FRuiValue>& Var : Scope->Vars)
			{
				if (!EnvSnapshot.Contains(Var.Key))
				{
					EnvSnapshot.Add(Var.Key, Var.Value);
				}
			}
		}
		TRuiSetter<FRuiValue> SetterCopy = *Setter;
		TSharedPtr<FUetkxExprNode> Arg = Event.Arg;
		Inputs.Events.Add(Pair.Get<0>(),
						  FRuiCallback::Create(
							  [SetterCopy, Arg, EnvSnapshot](const FRuiValue& Payload)
							  {
								  FUetkxExprEnv FireEnv;
								  FireEnv.Vars = EnvSnapshot;
								  FireEnv.Vars.Add(TEXT("Value"), Payload);
								  SetterCopy(Arg.IsValid() ? FUetkxExprVm::Eval(*Arg, FireEnv, nullptr) : Payload);
							  }));
	}
	if (Node.KeyExpr.IsValid())
	{
		Inputs.Key = MakeKey(FUetkxExprVm::Eval(*Node.KeyExpr, Env, nullptr));
	}
	else if (Node.bHasLiteralKey)
	{
		Inputs.Key = MakeKey(Node.LiteralKey);
	}
	EmitNodes(Node.Children, Env, Render, Inputs.Children);

	if (Node.Kind == FPrepNode::EKind::Component)
	{
		FRuiNode Ref = RUI::Named(Node.Tag);
		Ref.Key = Inputs.Key;
		return Ref;
	}
	return FUetkxInterpElements::Build(Node.Tag, MoveTemp(Inputs));
}

void FUetkxInterpDef::EmitBody(const FPrepBody& Body, FUetkxExprEnv& Env, FRenderCtx& Render, TArray<FRuiNode>& Out)
{
	FUetkxExprEnv Scope;
	Scope.Parent = &Env;
	RunAliases(Body.Lead, Scope, Render);
	EmitNodes(Body.Nodes, Scope, Render, Out);
}

void FUetkxInterpDef::EmitNodes(const TArray<TSharedPtr<FPrepNode>>& Nodes, FUetkxExprEnv& Env, FRenderCtx& Render,
								TArray<FRuiNode>& Out)
{
	for (const TSharedPtr<FPrepNode>& NodePtr : Nodes)
	{
		if (!NodePtr.IsValid())
		{
			continue;
		}
		const FPrepNode& Node = *NodePtr;
		switch (Node.Kind)
		{
		case FPrepNode::EKind::El:
		case FPrepNode::EKind::Component:
			Out.Add(EmitElement(Node, Env, Render));
			break;
		case FPrepNode::EKind::Frag:
		{
			TArray<FRuiNode> Children;
			EmitNodes(Node.Children, Env, Render, Children);
			Out.Add(RUI::Fragment(MoveTemp(Children)));
			break;
		}
		case FPrepNode::EKind::Text:
			Out.Add(RUI::TextBlock(FText::FromString(Node.Text)));
			break;
		case FPrepNode::EKind::Expr:
			Out.Add(RUI::TextBlock(
				FText::FromString(FUetkxExprVm::ToDisplayString(FUetkxExprVm::Eval(*Node.Expr, Env, nullptr)))));
			break;
		case FPrepNode::EKind::If:
		{
			bool bTaken = false;
			for (const FPrepNode::FPrepBranch& Branch : Node.Branches)
			{
				if (Branch.Cond.IsValid() && FUetkxExprVm::Truthy(FUetkxExprVm::Eval(*Branch.Cond, Env, nullptr)))
				{
					EmitBody(*Branch.Body, Env, Render, Out);
					bTaken = true;
					break;
				}
			}
			if (!bTaken && Node.ElseBody.IsValid())
			{
				EmitBody(*Node.ElseBody, Env, Render, Out);
			}
			break;
		}
		case FPrepNode::EKind::For:
		{
			FUetkxExprEnv LoopEnv;
			LoopEnv.Parent = &Env;
			FRuiValue Var = FUetkxExprVm::Eval(*Node.LoopInit, Env, nullptr);
			int32 Guard = 0;
			while (Guard++ < 10000)
			{
				LoopEnv.Vars.Add(Node.LoopVar, Var);
				if (!FUetkxExprVm::Truthy(FUetkxExprVm::Eval(*Node.LoopCond, LoopEnv, nullptr)))
				{
					break;
				}
				EmitBody(*Node.LoopBody, LoopEnv, Render, Out);
				if (Var.Kind == FRuiValue::EKind::Int)
				{
					Var = FRuiValue(Var.IntValue + Node.LoopStep);
				}
				else
				{
					break; // non-integer loop var — cannot step
				}
			}
			break;
		}
		case FPrepNode::EKind::While:
		{
			int32 Guard = 0;
			while (Guard++ < 10000 && FUetkxExprVm::Truthy(FUetkxExprVm::Eval(*Node.LoopCond, Env, nullptr)))
			{
				EmitBody(*Node.LoopBody, Env, Render, Out);
			}
			break;
		}
		case FPrepNode::EKind::Match:
		{
			const FRuiValue Subject = FUetkxExprVm::Eval(*Node.Subject, Env, nullptr);
			bool bMatched = false;
			for (const FPrepNode::FPrepCase& Case : Node.Cases)
			{
				if (Subject == FUetkxExprVm::Eval(*Case.Value, Env, nullptr))
				{
					EmitBody(*Case.Body, Env, Render, Out);
					bMatched = true;
					break;
				}
			}
			if (!bMatched && Node.DefaultBody.IsValid())
			{
				EmitBody(*Node.DefaultBody, Env, Render, Out);
			}
			break;
		}
		}
	}
}

FRuiNodeArray FUetkxInterpDef::Invoke(FRuiContext& Ctx)
{
	FUetkxExprEnv Env;
	FRenderCtx Render;
	Render.Ctx = &Ctx;

	for (const TTuple<FString, TSharedPtr<FUetkxExprNode>>& Param : ParamDefaults)
	{
		Env.Vars.Add(Param.Get<0>(),
					 Param.Get<1>().IsValid() ? FUetkxExprVm::Eval(*Param.Get<1>(), Env, nullptr) : FRuiValue());
	}
	for (const FHookOp& Op : HookPlan)
	{
		const FRuiValue Init = Op.Init.IsValid() ? FUetkxExprVm::Eval(*Op.Init, Env, nullptr) : FRuiValue();
		TTuple<FRuiValue, TRuiSetter<FRuiValue>> Slot = Ctx.UseState<FRuiValue>(Init);
		Env.Vars.Add(Op.ValueName, Slot.Get<0>());
		Render.Setters.Add(Op.SetterName, Slot.Get<1>());
	}
	RunAliases(SetupAliases, Env, Render);

	TArray<FRuiNode> Out;
	EmitNodes(Window, Env, Render, Out);
	return FRuiNodeArray(MoveTemp(Out));
}

TSharedPtr<FRuiComponentInvoke> FUetkxInterpDef::MakeInvoke()
{
	TSharedPtr<FUetkxInterpDef> Self = AsShared();
	return MakeShared<FRuiComponentInvoke>(
		[Self](FRuiContext& Ctx, const FRuiPropsBase*, const TArray<FRuiNode>&) -> FRuiNodeArray
		{ return Self->Invoke(Ctx); });
}

FRuiNode FUetkxInterpDef::MakeNode()
{
	FRuiNode Node;
	Node.Kind = ERuiNodeKind::Function;
	Node.ComponentId = FName(*Name);
	Node.Props = MakeShared<const FRuiEmptyProps>();
	Node.Invoke = MakeInvoke();
	return Node;
}

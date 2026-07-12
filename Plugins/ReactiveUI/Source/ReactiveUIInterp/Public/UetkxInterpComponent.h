// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
//
// FUetkxInterpDef — a component definition prepared for interpretation: the markup AST
// lowered ONCE into a prepared tree (attr/style/key/condition expressions VM-parsed, events
// bound to state setters, directive bodies split), then rendered per pass by walking it with
// an environment of props defaults + hook-state values + setup aliases + loop variables.
//
// Interpretable surface (D-20, honest): markup structure, literal + VM-subset expressions,
// UseState hooks (FRuiValue-typed slots; other hook kinds are structural no-ops under
// interp — effects run at the next real compile), setter-call event handlers
// (`SetCount(Count + 1)`), C-style @for / @while / @if / @match in the VM subset, setup
// value aliases (`const int32 Now = Count;`) and setter aliases (`TFunction<...> Set =
// SetCount;`). Everything else is SKIPPED with a note collected on the definition — the HMR
// status line surfaces "rebuild required for full behavior".

#pragma once

#include "CoreMinimal.h"
#include "RuiHooksInternal.h" // TRuiSetter
#include "RuiNode.h"
#include "UetkxExprVm.h"
#include "UetkxFileScan.h"

class REACTIVEUIINTERP_API FUetkxInterpDef : public TSharedFromThis<FUetkxInterpDef>
{
public:
	FString Name;
	uint32 HookSig = 0;
	TArray<FString> Notes; // fallback notes (collected at build; reported by HMR)

	/** Edit-time preview safety (bughunt P2): when set, a referenced COMPILED child component
	 *  (`<Child/>`) renders as an inert `<Child/>` placeholder instead of resolving to the live
	 *  compiled factory — so its UseEffect/UseLayoutEffect bodies never run in the editor (mirrors
	 *  URuiHostWidget's design-time guard, which the in-editor preview otherwise bypasses). The
	 *  previewed component's own markup + interp state still render live. */
	bool bStubComponents = false;

	static TSharedPtr<FUetkxInterpDef> Build(const FUetkxComponentDecl& Decl);

	/** The reconciler-facing invoke (swapped in via RUI::SetComponentOverride). */
	TSharedPtr<FRuiComponentInvoke> MakeInvoke();

	/** A mountable node for hot-LINKED components (RUI::Named path). */
	FRuiNode MakeNode();

private:
	struct FAliasOp
	{
		FString Name;
		TSharedPtr<FUetkxExprNode> ValueExpr; // value alias (evaluated sequentially)
		FString SetterOf;					  // setter alias: Name forwards to this setter
	};

	struct FInterpEvent
	{
		FString SetterName;				// resolved state setter (possibly via alias)
		TSharedPtr<FUetkxExprNode> Arg; // nullptr = pass the payload through
		bool bValid = false;
	};

	struct FPrepBody;

	struct FPrepNode
	{
		enum class EKind : uint8
		{
			El,
			Component,
			Frag,
			Text,
			Expr,
			If,
			For,
			While,
			Match
		};
		EKind Kind = EKind::El;
		FName Tag;

		TArray<TTuple<FName, FRuiValue>> LiteralAttrs;
		TArray<TTuple<FName, TSharedPtr<FUetkxExprNode>>> ExprAttrs;
		TArray<TTuple<FName, FRuiValue>> LiteralStyle; // includes Slot.* (routed by prefix)
		TArray<TTuple<FName, TSharedPtr<FUetkxExprNode>>> ExprStyle;
		TArray<FName> Classes;
		TArray<TTuple<FName, FInterpEvent>> Events;
		TSharedPtr<FUetkxExprNode> KeyExpr;
		FRuiValue LiteralKey; // str-form key
		bool bHasLiteralKey = false;
		TArray<TSharedPtr<FPrepNode>> Children;

		FString Text;					 // Text kind
		TSharedPtr<FUetkxExprNode> Expr; // Expr kind

		// If
		struct FPrepBranch
		{
			TSharedPtr<FUetkxExprNode> Cond;
			TSharedPtr<FPrepBody> Body;
		};
		TArray<FPrepBranch> Branches;
		TSharedPtr<FPrepBody> ElseBody;

		// For / While
		FString LoopVar;
		TSharedPtr<FUetkxExprNode> LoopInit;
		TSharedPtr<FUetkxExprNode> LoopCond;
		int64 LoopStep = 0; // +1 / -1; 0 = unsupported header (skipped, noted)
		TSharedPtr<FPrepBody> LoopBody;

		// Match
		TSharedPtr<FUetkxExprNode> Subject;
		struct FPrepCase
		{
			TSharedPtr<FUetkxExprNode> Value;
			TSharedPtr<FPrepBody> Body;
		};
		TArray<FPrepCase> Cases;
		TSharedPtr<FPrepBody> DefaultBody;
	};

	struct FPrepBody
	{
		TArray<FAliasOp> Lead;
		TArray<TSharedPtr<FPrepNode>> Nodes;
	};

	struct FHookOp
	{
		FString Kind;	   // "UseState" executes; others structural no-ops
		FString ValueName; // structured-binding names
		FString SetterName;
		TSharedPtr<FUetkxExprNode> Init;
	};

	// definition data
	TArray<TTuple<FString, TSharedPtr<FUetkxExprNode>>> ParamDefaults;
	TArray<FHookOp> HookPlan;
	TArray<FAliasOp> SetupAliases;
	TSet<FString> KnownSetters; // bound + aliased setter names (event resolution)
	TArray<TSharedPtr<FPrepNode>> Window;

	// build helpers
	void ParseSetup(const FString& Setup);
	void ParseBindings(const FString& Code, TArray<FAliasOp>& OutOps, bool bAllowHooks);
	TSharedPtr<FPrepNode> PrepareNode(const struct FUetkxNode& Node);
	TSharedPtr<FPrepBody> PrepareBody(const FString& BodyMarkup);
	FInterpEvent PrepareEvent(const FString& ExprText);
	void Note(const FString& Message);

	// render
	struct FRenderCtx
	{
		class FRuiContext* Ctx = nullptr;
		TMap<FString, TRuiSetter<FRuiValue>> Setters;
	};
	void EmitNodes(const TArray<TSharedPtr<FPrepNode>>& Nodes, FUetkxExprEnv& Env, FRenderCtx& Render,
				   TArray<FRuiNode>& Out);
	void EmitBody(const FPrepBody& Body, FUetkxExprEnv& Env, FRenderCtx& Render, TArray<FRuiNode>& Out);
	FRuiNode EmitElement(const FPrepNode& Node, FUetkxExprEnv& Env, FRenderCtx& Render);
	void RunAliases(const TArray<FAliasOp>& Ops, FUetkxExprEnv& Env, FRenderCtx& Render);

	FRuiNodeArray Invoke(class FRuiContext& Ctx);
};

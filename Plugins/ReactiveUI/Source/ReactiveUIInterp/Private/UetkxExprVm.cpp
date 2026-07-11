// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "UetkxExprVm.h"

#include "Misc/DefaultValueHelper.h"

namespace
{
	struct FVmRegistry
	{
		TMap<FName, TFunction<FRuiValue(TArrayView<const FRuiValue>)>> Fns;
		FCriticalSection Lock;

		static FVmRegistry& Get()
		{
			static FVmRegistry Instance;
			return Instance;
		}
	};

	// ── recursive-descent parser over TCHARs ───────────────────────────────────────────────

	class FParser
	{
	public:
		FParser(const FString& InSrc, FString* InWhy) : Src(InSrc), Why(InWhy) {}

		TSharedPtr<FUetkxExprNode> Run()
		{
			TSharedPtr<FUetkxExprNode> Node = ParseTernary();
			SkipWs();
			if (Node.IsValid() && Pos < Src.Len())
			{
				return Fail(FString::Printf(TEXT("unexpected `%c`"), Src[Pos]));
			}
			return Node;
		}

	private:
		const FString& Src;
		FString* Why;
		int32 Pos = 0;

		TSharedPtr<FUetkxExprNode> Fail(const FString& Reason)
		{
			if (Why && Why->IsEmpty())
			{
				*Why = Reason;
			}
			return nullptr;
		}

		void SkipWs()
		{
			while (Pos < Src.Len() && FChar::IsWhitespace(Src[Pos]))
			{
				++Pos;
			}
		}

		bool Peek(const TCHAR* Token)
		{
			SkipWs();
			const int32 Len = FCString::Strlen(Token);
			if (Pos + Len > Src.Len())
			{
				return false;
			}
			return FCString::Strncmp(*Src + Pos, Token, Len) == 0;
		}

		bool Accept(const TCHAR* Token)
		{
			if (Peek(Token))
			{
				Pos += FCString::Strlen(Token);
				return true;
			}
			return false;
		}

		static bool IsIdentStart(TCHAR C) { return FChar::IsAlpha(C) || C == '_'; }
		static bool IsIdentChar(TCHAR C) { return FChar::IsAlnum(C) || C == '_'; }

		FString ReadIdent()
		{
			SkipWs();
			const int32 Start = Pos;
			while (Pos < Src.Len() && IsIdentChar(Src[Pos]))
			{
				++Pos;
			}
			return Src.Mid(Start, Pos - Start);
		}

		TSharedPtr<FUetkxExprNode> MakeLiteral(FRuiValue Value)
		{
			TSharedPtr<FUetkxExprNode> Node = MakeShared<FUetkxExprNode>();
			Node->Op = EUetkxExprOp::Literal;
			Node->Literal = MoveTemp(Value);
			return Node;
		}

		/** `"..."` with \" \\ \n \t escapes. Pos at the opening quote. */
		bool ReadQuoted(FString& Out)
		{
			if (Pos >= Src.Len() || Src[Pos] != '"')
			{
				return false;
			}
			++Pos;
			while (Pos < Src.Len() && Src[Pos] != '"')
			{
				TCHAR C = Src[Pos];
				if (C == '\\' && Pos + 1 < Src.Len())
				{
					++Pos;
					const TCHAR E = Src[Pos];
					C = E == 'n' ? '\n' : E == 't' ? '\t' : E == 'r' ? '\r' : E;
				}
				Out.AppendChar(C);
				++Pos;
			}
			if (Pos >= Src.Len())
			{
				return false;
			}
			++Pos; // closing quote
			return true;
		}

		TSharedPtr<FUetkxExprNode> ParseTernary()
		{
			TSharedPtr<FUetkxExprNode> Cond = ParseOr();
			if (!Cond.IsValid())
			{
				return nullptr;
			}
			if (!Accept(TEXT("?")))
			{
				return Cond;
			}
			TSharedPtr<FUetkxExprNode> Then = ParseTernary();
			if (!Then.IsValid() || !Accept(TEXT(":")))
			{
				return Fail(TEXT("malformed ternary"));
			}
			TSharedPtr<FUetkxExprNode> Else = ParseTernary();
			if (!Else.IsValid())
			{
				return nullptr;
			}
			TSharedPtr<FUetkxExprNode> Node = MakeShared<FUetkxExprNode>();
			Node->Op = EUetkxExprOp::Ternary;
			Node->Args = {Cond, Then, Else};
			return Node;
		}

		TSharedPtr<FUetkxExprNode> ParseBinaryChain(TSharedPtr<FUetkxExprNode> (FParser::*Next)(),
													std::initializer_list<const TCHAR*> Ops)
		{
			TSharedPtr<FUetkxExprNode> Left = (this->*Next)();
			while (Left.IsValid())
			{
				const TCHAR* Matched = nullptr;
				for (const TCHAR* Op : Ops)
				{
					if (Peek(Op))
					{
						Matched = Op;
						break;
					}
				}
				if (!Matched)
				{
					break;
				}
				Accept(Matched);
				TSharedPtr<FUetkxExprNode> Right = (this->*Next)();
				if (!Right.IsValid())
				{
					return nullptr;
				}
				TSharedPtr<FUetkxExprNode> Node = MakeShared<FUetkxExprNode>();
				Node->Op = EUetkxExprOp::Binary;
				Node->Name = Matched;
				Node->Args = {Left, Right};
				Left = Node;
			}
			return Left;
		}

		TSharedPtr<FUetkxExprNode> ParseOr() { return ParseBinaryChain(&FParser::ParseAnd, {TEXT("||")}); }
		TSharedPtr<FUetkxExprNode> ParseAnd() { return ParseBinaryChain(&FParser::ParseEquality, {TEXT("&&")}); }
		TSharedPtr<FUetkxExprNode> ParseEquality()
		{
			return ParseBinaryChain(&FParser::ParseRelational, {TEXT("=="), TEXT("!=")});
		}
		TSharedPtr<FUetkxExprNode> ParseRelational()
		{
			// order matters: <= before < (longest match)
			return ParseBinaryChain(&FParser::ParseAdditive, {TEXT("<="), TEXT(">="), TEXT("<"), TEXT(">")});
		}
		TSharedPtr<FUetkxExprNode> ParseAdditive()
		{
			return ParseBinaryChain(&FParser::ParseMultiplicative, {TEXT("+"), TEXT("-")});
		}
		TSharedPtr<FUetkxExprNode> ParseMultiplicative()
		{
			return ParseBinaryChain(&FParser::ParseUnary, {TEXT("*"), TEXT("/"), TEXT("%")});
		}

		TSharedPtr<FUetkxExprNode> ParseUnary()
		{
			SkipWs();
			if (Accept(TEXT("!")))
			{
				TSharedPtr<FUetkxExprNode> Inner = ParseUnary();
				if (!Inner.IsValid())
				{
					return nullptr;
				}
				TSharedPtr<FUetkxExprNode> Node = MakeShared<FUetkxExprNode>();
				Node->Op = EUetkxExprOp::Unary;
				Node->Name = TEXT("!");
				Node->Args = {Inner};
				return Node;
			}
			if (Peek(TEXT("-")))
			{
				// unary minus only when NOT part of a number literal (ParsePrimary reads those)
				const int32 Save = Pos;
				Accept(TEXT("-"));
				SkipWs();
				if (Pos < Src.Len() && !FChar::IsDigit(Src[Pos]))
				{
					TSharedPtr<FUetkxExprNode> Inner = ParseUnary();
					if (!Inner.IsValid())
					{
						return nullptr;
					}
					TSharedPtr<FUetkxExprNode> Node = MakeShared<FUetkxExprNode>();
					Node->Op = EUetkxExprOp::Unary;
					Node->Name = TEXT("-");
					Node->Args = {Inner};
					return Node;
				}
				Pos = Save; // negative literal — let ParsePrimary read it
			}
			return ParsePostfix();
		}

		TSharedPtr<FUetkxExprNode> ParsePostfix()
		{
			TSharedPtr<FUetkxExprNode> Node = ParsePrimary();
			while (Node.IsValid())
			{
				SkipWs();
				if (Accept(TEXT(".")))
				{
					const FString Field = ReadIdent();
					if (Field.IsEmpty())
					{
						return Fail(TEXT("expected member name after `.`"));
					}
					SkipWs();
					if (Accept(TEXT("(")))
					{
						// method sugar: receiver becomes arg 0 of ".Field"
						TSharedPtr<FUetkxExprNode> Call = MakeShared<FUetkxExprNode>();
						Call->Op = EUetkxExprOp::Call;
						Call->Name = TEXT(".") + Field;
						Call->Args.Add(Node);
						if (!ParseArgs(Call->Args))
						{
							return nullptr;
						}
						Node = Call;
						continue;
					}
					TSharedPtr<FUetkxExprNode> Member = MakeShared<FUetkxExprNode>();
					Member->Op = EUetkxExprOp::Member;
					Member->Name = Field;
					Member->Args = {Node};
					Node = Member;
					continue;
				}
				if (Peek(TEXT("[")))
				{
					return Fail(TEXT("indexing is outside the interp subset (no array values)"));
				}
				if (Peek(TEXT("->")))
				{
					return Fail(TEXT("pointer access is outside the interp subset"));
				}
				break;
			}
			return Node;
		}

		bool ParseArgs(TArray<TSharedPtr<FUetkxExprNode>>& OutArgs)
		{
			SkipWs();
			if (Accept(TEXT(")")))
			{
				return true;
			}
			while (true)
			{
				TSharedPtr<FUetkxExprNode> Arg = ParseTernary();
				if (!Arg.IsValid())
				{
					return false;
				}
				OutArgs.Add(Arg);
				if (Accept(TEXT(")")))
				{
					return true;
				}
				if (!Accept(TEXT(",")))
				{
					Fail(TEXT("expected `,` or `)` in call args"));
					return false;
				}
			}
		}

		TSharedPtr<FUetkxExprNode> ParsePrimary()
		{
			SkipWs();
			if (Pos >= Src.Len())
			{
				return Fail(TEXT("unexpected end of expression"));
			}
			const TCHAR C = Src[Pos];
			if (C == '(')
			{
				++Pos;
				TSharedPtr<FUetkxExprNode> Inner = ParseTernary();
				if (!Inner.IsValid() || !Accept(TEXT(")")))
				{
					return Fail(TEXT("unclosed `(`"));
				}
				return Inner;
			}
			if (C == '"')
			{
				FString Value;
				if (!ReadQuoted(Value))
				{
					return Fail(TEXT("unterminated string"));
				}
				return MakeLiteral(FRuiValue(Value));
			}
			if (FChar::IsDigit(C) || (C == '-' && Pos + 1 < Src.Len() && FChar::IsDigit(Src[Pos + 1])))
			{
				const int32 Start = Pos;
				if (C == '-')
				{
					++Pos;
				}
				bool bFloat = false;
				while (Pos < Src.Len() && (FChar::IsDigit(Src[Pos]) || Src[Pos] == '.'))
				{
					bFloat |= Src[Pos] == '.';
					++Pos;
				}
				FString Number = Src.Mid(Start, Pos - Start);
				if (Pos < Src.Len() && (Src[Pos] == 'f' || Src[Pos] == 'F'))
				{
					bFloat = true;
					++Pos;
				}
				if (bFloat)
				{
					return MakeLiteral(FRuiValue(FCString::Atod(*Number)));
				}
				return MakeLiteral(FRuiValue(static_cast<int64>(FCString::Atoi64(*Number))));
			}
			if (!IsIdentStart(C))
			{
				return Fail(FString::Printf(TEXT("unsupported token `%c`"), C));
			}
			FString Ident = ReadIdent();
			// `::`-qualified name chains fold into the call name (FText::FromString)
			while (Peek(TEXT("::")))
			{
				Accept(TEXT("::"));
				const FString Next = ReadIdent();
				if (Next.IsEmpty())
				{
					return Fail(TEXT("expected name after `::`"));
				}
				Ident += TEXT("::") + Next;
			}
			if (Ident == TEXT("true"))
			{
				return MakeLiteral(FRuiValue(true));
			}
			if (Ident == TEXT("false"))
			{
				return MakeLiteral(FRuiValue(false));
			}
			SkipWs();
			if (Accept(TEXT("(")))
			{
				if (Ident == TEXT("TEXT"))
				{
					// TEXT("...") is a literal, not a call
					SkipWs();
					FString Value;
					if (!ReadQuoted(Value) || !Accept(TEXT(")")))
					{
						return Fail(TEXT("malformed TEXT literal"));
					}
					return MakeLiteral(FRuiValue(Value));
				}
				TSharedPtr<FUetkxExprNode> Call = MakeShared<FUetkxExprNode>();
				Call->Op = EUetkxExprOp::Call;
				Call->Name = Ident;
				if (!ParseArgs(Call->Args))
				{
					return nullptr;
				}
				if (!FVmRegistry::Get().Fns.Contains(FName(*Ident)))
				{
					return Fail(FString::Printf(TEXT("`%s` is not in the interp whitelist"), *Ident));
				}
				return Call;
			}
			if (Ident.Contains(TEXT("::")))
			{
				return Fail(FString::Printf(TEXT("qualified name `%s` used as a value"), *Ident));
			}
			TSharedPtr<FUetkxExprNode> Node = MakeShared<FUetkxExprNode>();
			Node->Op = EUetkxExprOp::Ident;
			Node->Name = Ident;
			return Node;
		}
	};

	double AsNumber(const FRuiValue& Value, bool& bOk)
	{
		bOk = true;
		switch (Value.Kind)
		{
		case FRuiValue::EKind::Int:
			return static_cast<double>(Value.IntValue);
		case FRuiValue::EKind::Float:
			return Value.FloatValue;
		case FRuiValue::EKind::Bool:
			return Value.BoolValue ? 1.0 : 0.0;
		default:
			bOk = false;
			return 0.0;
		}
	}

	bool BothInts(const FRuiValue& A, const FRuiValue& B)
	{
		return A.Kind == FRuiValue::EKind::Int && B.Kind == FRuiValue::EKind::Int;
	}

	void AddError(TArray<FString>* OutErrors, const FString& Message)
	{
		if (OutErrors)
		{
			OutErrors->Add(Message);
		}
	}

	/** The %d/%i/%s/%f (+ %.Nf, %%) subset of Printf over FRuiValue args. */
	FRuiValue PrintfSubset(TArrayView<const FRuiValue> Args)
	{
		if (Args.Num() == 0 || Args[0].Kind != FRuiValue::EKind::String)
		{
			return FRuiValue();
		}
		const FString& Pattern = Args[0].StringValue;
		FString Out;
		int32 ArgIndex = 1;
		for (int32 i = 0; i < Pattern.Len(); ++i)
		{
			if (Pattern[i] != '%')
			{
				Out.AppendChar(Pattern[i]);
				continue;
			}
			if (i + 1 < Pattern.Len() && Pattern[i + 1] == '%')
			{
				Out.AppendChar('%');
				++i;
				continue;
			}
			// %[.N]spec
			int32 Precision = -1;
			int32 j = i + 1;
			if (j < Pattern.Len() && Pattern[j] == '.')
			{
				++j;
				int32 P = 0;
				while (j < Pattern.Len() && FChar::IsDigit(Pattern[j]))
				{
					P = P * 10 + (Pattern[j] - '0');
					++j;
				}
				Precision = P;
			}
			const TCHAR Spec = j < Pattern.Len() ? Pattern[j] : '\0';
			const FRuiValue Arg = ArgIndex < Args.Num() ? Args[ArgIndex++] : FRuiValue();
			bool bNum = false;
			switch (Spec)
			{
			case 'd':
			case 'i':
				Out += FString::Printf(TEXT("%lld"), static_cast<int64>(AsNumber(Arg, bNum)));
				break;
			case 'f':
				Out += FString::Printf(TEXT("%.*f"), Precision >= 0 ? Precision : 6, AsNumber(Arg, bNum));
				break;
			case 's':
				Out += FUetkxExprVm::ToDisplayString(Arg);
				break;
			default:
				return FRuiValue(); // unsupported spec — total failure, caller records it
			}
			i = j;
		}
		return FRuiValue(Out);
	}
} // namespace

TSharedPtr<FUetkxExprNode> FUetkxExprVm::Parse(const FString& Expr, FString* OutWhy)
{
	RegisterBuiltins();
	FParser Parser(Expr, OutWhy);
	return Parser.Run();
}

bool FUetkxExprVm::Truthy(const FRuiValue& Value)
{
	switch (Value.Kind)
	{
	case FRuiValue::EKind::Bool:
		return Value.BoolValue;
	case FRuiValue::EKind::Int:
		return Value.IntValue != 0;
	case FRuiValue::EKind::Float:
		return Value.FloatValue != 0.0;
	case FRuiValue::EKind::String:
		return !Value.StringValue.IsEmpty();
	case FRuiValue::EKind::Text:
		return !Value.TextValue.IsEmpty();
	case FRuiValue::EKind::Name:
		return !Value.NameValue.IsNone();
	default:
		return false;
	}
}

FString FUetkxExprVm::ToDisplayString(const FRuiValue& Value)
{
	switch (Value.Kind)
	{
	case FRuiValue::EKind::Bool:
		return Value.BoolValue ? TEXT("true") : TEXT("false");
	case FRuiValue::EKind::Int:
		return FString::Printf(TEXT("%lld"), Value.IntValue);
	case FRuiValue::EKind::Float:
		return FString::SanitizeFloat(Value.FloatValue);
	case FRuiValue::EKind::String:
		return Value.StringValue;
	case FRuiValue::EKind::Name:
		return Value.NameValue.ToString();
	case FRuiValue::EKind::Text:
		return Value.TextValue.ToString();
	default:
		return FString();
	}
}

FRuiValue FUetkxExprVm::Eval(const FUetkxExprNode& Node, const FUetkxExprEnv& Env, TArray<FString>* OutErrors)
{
	switch (Node.Op)
	{
	case EUetkxExprOp::Literal:
		return Node.Literal;
	case EUetkxExprOp::Ident:
	{
		FRuiValue Out;
		if (!Env.Resolve(Node.Name, Out))
		{
			AddError(OutErrors, FString::Printf(TEXT("`%s` is not in scope"), *Node.Name));
			return FRuiValue();
		}
		return Out;
	}
	case EUetkxExprOp::Member:
	{
		const FRuiValue Receiver = Eval(*Node.Args[0], Env, OutErrors);
		if (Receiver.Kind == FRuiValue::EKind::Vector2)
		{
			if (Node.Name == TEXT("X"))
			{
				return FRuiValue(Receiver.Vector2Value.X);
			}
			if (Node.Name == TEXT("Y"))
			{
				return FRuiValue(Receiver.Vector2Value.Y);
			}
		}
		if (Receiver.Kind == FRuiValue::EKind::Color)
		{
			if (Node.Name == TEXT("R"))
			{
				return FRuiValue(Receiver.ColorValue.R);
			}
			if (Node.Name == TEXT("G"))
			{
				return FRuiValue(Receiver.ColorValue.G);
			}
			if (Node.Name == TEXT("B"))
			{
				return FRuiValue(Receiver.ColorValue.B);
			}
			if (Node.Name == TEXT("A"))
			{
				return FRuiValue(Receiver.ColorValue.A);
			}
		}
		AddError(OutErrors, FString::Printf(TEXT("no member `%s` on this value"), *Node.Name));
		return FRuiValue();
	}
	case EUetkxExprOp::Call:
	{
		TArray<FRuiValue> Args;
		Args.Reserve(Node.Args.Num());
		for (const TSharedPtr<FUetkxExprNode>& Arg : Node.Args)
		{
			Args.Add(Eval(*Arg, Env, OutErrors));
		}
		TFunction<FRuiValue(TArrayView<const FRuiValue>)> Fn;
		{
			FVmRegistry& Reg = FVmRegistry::Get();
			FScopeLock Guard(&Reg.Lock);
			if (const auto* Found = Reg.Fns.Find(FName(*Node.Name)))
			{
				Fn = *Found;
			}
		}
		if (!Fn)
		{
			AddError(OutErrors, FString::Printf(TEXT("`%s` is not whitelisted"), *Node.Name));
			return FRuiValue();
		}
		return Fn(Args);
	}
	case EUetkxExprOp::Unary:
	{
		const FRuiValue Inner = Eval(*Node.Args[0], Env, OutErrors);
		if (Node.Name == TEXT("!"))
		{
			return FRuiValue(!Truthy(Inner));
		}
		bool bOk = false;
		const double Num = AsNumber(Inner, bOk);
		if (!bOk)
		{
			AddError(OutErrors, TEXT("unary `-` on a non-number"));
			return FRuiValue();
		}
		return Inner.Kind == FRuiValue::EKind::Int ? FRuiValue(static_cast<int64>(-Num)) : FRuiValue(-Num);
	}
	case EUetkxExprOp::Binary:
	{
		const FString& Op = Node.Name;
		// short-circuit forms evaluate lazily
		if (Op == TEXT("&&"))
		{
			return FRuiValue(Truthy(Eval(*Node.Args[0], Env, OutErrors)) &&
							 Truthy(Eval(*Node.Args[1], Env, OutErrors)));
		}
		if (Op == TEXT("||"))
		{
			if (Truthy(Eval(*Node.Args[0], Env, OutErrors)))
			{
				return FRuiValue(true);
			}
			return FRuiValue(Truthy(Eval(*Node.Args[1], Env, OutErrors)));
		}
		const FRuiValue A = Eval(*Node.Args[0], Env, OutErrors);
		const FRuiValue B = Eval(*Node.Args[1], Env, OutErrors);
		if (Op == TEXT("=="))
		{
			return FRuiValue(A == B);
		}
		if (Op == TEXT("!="))
		{
			return FRuiValue(!(A == B));
		}
		if (Op == TEXT("+") && (A.Kind == FRuiValue::EKind::String || B.Kind == FRuiValue::EKind::String))
		{
			return FRuiValue(ToDisplayString(A) + ToDisplayString(B));
		}
		bool bOkA = false, bOkB = false;
		const double NumA = AsNumber(A, bOkA);
		const double NumB = AsNumber(B, bOkB);
		if (!bOkA || !bOkB)
		{
			AddError(OutErrors, FString::Printf(TEXT("operator `%s` on non-numbers"), *Op));
			return FRuiValue();
		}
		if (Op == TEXT("<"))
		{
			return FRuiValue(NumA < NumB);
		}
		if (Op == TEXT("<="))
		{
			return FRuiValue(NumA <= NumB);
		}
		if (Op == TEXT(">"))
		{
			return FRuiValue(NumA > NumB);
		}
		if (Op == TEXT(">="))
		{
			return FRuiValue(NumA >= NumB);
		}
		const bool bIntMath = BothInts(A, B);
		if (Op == TEXT("+"))
		{
			return bIntMath ? FRuiValue(A.IntValue + B.IntValue) : FRuiValue(NumA + NumB);
		}
		if (Op == TEXT("-"))
		{
			return bIntMath ? FRuiValue(A.IntValue - B.IntValue) : FRuiValue(NumA - NumB);
		}
		if (Op == TEXT("*"))
		{
			return bIntMath ? FRuiValue(A.IntValue * B.IntValue) : FRuiValue(NumA * NumB);
		}
		if (Op == TEXT("/"))
		{
			if (NumB == 0.0)
			{
				AddError(OutErrors, TEXT("division by zero"));
				return FRuiValue();
			}
			return bIntMath ? FRuiValue(A.IntValue / B.IntValue) : FRuiValue(NumA / NumB);
		}
		if (Op == TEXT("%"))
		{
			if (!bIntMath || B.IntValue == 0)
			{
				AddError(OutErrors, TEXT("`%` needs non-zero integers"));
				return FRuiValue();
			}
			return FRuiValue(A.IntValue % B.IntValue);
		}
		AddError(OutErrors, FString::Printf(TEXT("operator `%s` unsupported"), *Op));
		return FRuiValue();
	}
	case EUetkxExprOp::Ternary:
		return Truthy(Eval(*Node.Args[0], Env, OutErrors)) ? Eval(*Node.Args[1], Env, OutErrors)
														   : Eval(*Node.Args[2], Env, OutErrors);
	}
	return FRuiValue();
}

void FUetkxExprVm::RegisterFn(FName Qualified, TFunction<FRuiValue(TArrayView<const FRuiValue>)> Fn)
{
	FVmRegistry& Reg = FVmRegistry::Get();
	FScopeLock Guard(&Reg.Lock);
	Reg.Fns.Add(Qualified, MoveTemp(Fn));
}

bool FUetkxExprVm::HasFn(FName Qualified)
{
	FVmRegistry& Reg = FVmRegistry::Get();
	FScopeLock Guard(&Reg.Lock);
	return Reg.Fns.Contains(Qualified);
}

void FUetkxExprVm::RegisterBuiltins()
{
	static bool bOnce = false;
	if (bOnce)
	{
		return;
	}
	bOnce = true;

	auto Num = [](const FRuiValue& Value)
	{
		bool bOk = false;
		return AsNumber(Value, bOk);
	};

	RegisterFn(TEXT("FText::FromString"), [](TArrayView<const FRuiValue> Args)
			   { return FRuiValue(FText::FromString(Args.Num() ? ToDisplayString(Args[0]) : FString())); });
	RegisterFn(TEXT("FText::AsNumber"),
			   [Num](TArrayView<const FRuiValue> Args)
			   {
				   if (!Args.Num())
				   {
					   return FRuiValue();
				   }
				   if (Args[0].Kind == FRuiValue::EKind::Int)
				   {
					   return FRuiValue(FText::AsNumber(Args[0].IntValue));
				   }
				   return FRuiValue(FText::AsNumber(Num(Args[0])));
			   });
	RegisterFn(TEXT("FString::Printf"), [](TArrayView<const FRuiValue> Args) { return PrintfSubset(Args); });
	RegisterFn(TEXT("FString"), [](TArrayView<const FRuiValue> Args)
			   { return FRuiValue(Args.Num() ? ToDisplayString(Args[0]) : FString()); });
	RegisterFn(TEXT("FName"), [](TArrayView<const FRuiValue> Args)
			   { return FRuiValue(FName(*(Args.Num() ? ToDisplayString(Args[0]) : FString()))); });
	RegisterFn(
		TEXT("FVector2D"), [Num](TArrayView<const FRuiValue> Args)
		{ return FRuiValue(FVector2D(Args.Num() > 0 ? Num(Args[0]) : 0.0, Args.Num() > 1 ? Num(Args[1]) : 0.0)); });
	RegisterFn(TEXT("FLinearColor"),
			   [Num](TArrayView<const FRuiValue> Args)
			   {
				   return FRuiValue(
					   FLinearColor(Args.Num() > 0 ? Num(Args[0]) : 0.0, Args.Num() > 1 ? Num(Args[1]) : 0.0,
									Args.Num() > 2 ? Num(Args[2]) : 0.0, Args.Num() > 3 ? Num(Args[3]) : 1.0));
			   });
	RegisterFn(TEXT("FMath::Min"), [Num](TArrayView<const FRuiValue> Args)
			   { return Args.Num() < 2 ? FRuiValue() : FRuiValue(FMath::Min(Num(Args[0]), Num(Args[1]))); });
	RegisterFn(TEXT("FMath::Max"), [Num](TArrayView<const FRuiValue> Args)
			   { return Args.Num() < 2 ? FRuiValue() : FRuiValue(FMath::Max(Num(Args[0]), Num(Args[1]))); });
	RegisterFn(
		TEXT("FMath::Clamp"), [Num](TArrayView<const FRuiValue> Args)
		{ return Args.Num() < 3 ? FRuiValue() : FRuiValue(FMath::Clamp(Num(Args[0]), Num(Args[1]), Num(Args[2]))); });
	RegisterFn(TEXT("FMath::Abs"), [Num](TArrayView<const FRuiValue> Args)
			   { return Args.Num() < 1 ? FRuiValue() : FRuiValue(FMath::Abs(Num(Args[0]))); });
	RegisterFn(TEXT("LexToString"), [](TArrayView<const FRuiValue> Args)
			   { return FRuiValue(Args.Num() ? ToDisplayString(Args[0]) : FString()); });
	// method sugar
	RegisterFn(TEXT(".ToString"), [](TArrayView<const FRuiValue> Args)
			   { return FRuiValue(Args.Num() ? ToDisplayString(Args[0]) : FString()); });
	RegisterFn(TEXT(".IsEmpty"), [](TArrayView<const FRuiValue> Args)
			   { return FRuiValue(Args.Num() ? ToDisplayString(Args[0]).IsEmpty() : true); });
}

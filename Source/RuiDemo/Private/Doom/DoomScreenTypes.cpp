// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "Doom/DoomScreenTypes.h"

#include "Engine/Texture2D.h"

namespace RuiDoom
{
	const TSharedRef<FSlateBrush>& FDoomBrushPool::Solid()
	{
		static const TSharedRef<FSlateBrush> Brush = []()
		{
			TSharedRef<FSlateBrush> B = MakeShared<FSlateBrush>();
			B->DrawAs = ESlateBrushDrawType::Image;
			B->TintColor = FLinearColor::White; // per-quad color rides the WIDGET tint
			B->SetResourceObject(nullptr);		// no texture = solid fill
			return B;
		}();
		return Brush;
	}

	TSharedRef<FSlateBrush> FDoomBrushPool::GetTextured(FName Key, UTexture2D* Texture, const FBox2f& UVRegion)
	{
		FPair& Pair = Pool.FindOrAdd(Key);
		if (!Pair.A.IsValid())
		{
			Pair.A = MakeShared<FSlateBrush>();
			Pair.B = MakeShared<FSlateBrush>();
			for (const TSharedPtr<FSlateBrush>& B : {Pair.A, Pair.B})
			{
				B->DrawAs = ESlateBrushDrawType::Image;
				B->Tiling = ESlateBrushTileType::NoTile;
				B->TintColor = FLinearColor::White;
			}
		}
		Pair.bUseB = !Pair.bUseB; // flip identity every acquire — pointer-diff always re-applies
		const TSharedPtr<FSlateBrush>& Brush = Pair.bUseB ? Pair.B : Pair.A;
		Brush->SetResourceObject(Texture);
		if (Texture != nullptr)
		{
			Brush->ImageSize = FVector2D(Texture->GetSizeX(), Texture->GetSizeY());
		}
		if (UVRegion.bIsValid)
		{
			Brush->SetUVRegion(UVRegion);
		}
		else
		{
			Brush->SetUVRegion(FBox2f(FVector2f(0.0f, 0.0f), FVector2f(1.0f, 1.0f)));
		}
		return Brush.ToSharedRef();
	}

	void FDoomBrushPool::Reset()
	{
		Pool.Empty();
	}
} // namespace RuiDoom

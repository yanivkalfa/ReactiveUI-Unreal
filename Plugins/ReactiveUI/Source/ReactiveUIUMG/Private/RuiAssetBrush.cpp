// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "RuiAssetBrush.h"

#include "UObject/GCObject.h"

namespace
{
	/** Process-wide GC root: references the resource object of every LIVE asset brush. Brushes
	 *  are tracked weakly, so when the props that owned a brush release it the entry is compacted
	 *  and its asset is free to collect again — no leak, no dangling paint. */
	class FRuiAssetBrushRoot : public FGCObject
	{
	public:
		void Track(const TSharedRef<FSlateBrush>& Brush) { Brushes.Add(Brush); }

		int32 NumLive()
		{
			Compact();
			return Brushes.Num();
		}

		virtual void AddReferencedObjects(FReferenceCollector& Collector) override
		{
			for (int32 i = Brushes.Num() - 1; i >= 0; --i)
			{
				TSharedPtr<FSlateBrush> Brush = Brushes[i].Pin();
				if (!Brush.IsValid())
				{
					Brushes.RemoveAtSwap(i);
					continue;
				}
				// TObjectPtr, not UObject*: the raw-pointer AddReferencedObject overload is
				// deprecated in UE 5.7 and unsafe under incremental GC. If the collector nulls
				// the reference (object being destroyed), write that back into the brush.
				if (TObjectPtr<UObject> Resource = Brush->GetResourceObject())
				{
					Collector.AddReferencedObject(Resource);
					if (Resource != Brush->GetResourceObject())
					{
						Brush->SetResourceObject(Resource);
					}
				}
			}
		}

		virtual FString GetReferencerName() const override { return TEXT("RUI::Umg::AssetBrushRoot"); }

	private:
		void Compact()
		{
			Brushes.RemoveAll([](const TWeakPtr<FSlateBrush>& B) { return !B.IsValid(); });
		}

		TArray<TWeakPtr<FSlateBrush>> Brushes;
	};

	FRuiAssetBrushRoot& BrushRoot()
	{
		static FRuiAssetBrushRoot Root;
		return Root;
	}
} // namespace

TSharedPtr<FSlateBrush> RUI::Umg::MakeAssetBrush(UObject* ResourceObject, FVector2D ImageSize, FLinearColor Tint,
												 ESlateBrushDrawType::Type DrawAs)
{
	TSharedRef<FSlateBrush> Brush = MakeShared<FSlateBrush>();
	Brush->SetResourceObject(ResourceObject);
	Brush->ImageSize = ImageSize;
	Brush->TintColor = FSlateColor(Tint);
	Brush->DrawAs = DrawAs;
	BrushRoot().Track(Brush);
	return Brush;
}

int32 RUI::Umg::NumTrackedAssetBrushes()
{
	return BrushRoot().NumLive();
}

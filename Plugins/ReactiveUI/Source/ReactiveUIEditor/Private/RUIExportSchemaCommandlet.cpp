// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "RUIExportSchemaCommandlet.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UetkxCodegen.h"

DEFINE_LOG_CATEGORY_STATIC(LogRUIExportSchema, Log, All);

int32 URUIExportSchemaCommandlet::Main(const FString& Params)
{
	const FString Path = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("ReactiveUI"), TEXT("schema.json"));
	if (!FFileHelper::SaveStringToFile(FUetkxCodegen::ExportSchemaJson(), *Path,
									   FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogRUIExportSchema, Error, TEXT("could not write %s"), *Path);
		return 1;
	}
	UE_LOG(LogRUIExportSchema, Display, TEXT("schema exported: %s"), *Path);
	return 0;
}

// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "UetkxConfig.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

FUetkxConfig FUetkxConfig::Load(const FString& Dir)
{
	FUetkxConfig Config;
	FString D = Dir;
	for (int32 Depth = 0; Depth < 32 && !D.IsEmpty(); ++Depth)
	{
		const FString ConfigPath = D / TEXT("uetkx.config.json");
		FString Json;
		if (FFileHelper::LoadFileToString(Json, *ConfigPath))
		{
			Config.ConfigDir = D;
			TSharedPtr<FJsonObject> Root;
			if (FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) && Root.IsValid())
			{
				double Num = 0.0;
				FString Str;
				bool bFlag = false;
				if (Root->TryGetNumberField(TEXT("printWidth"), Num))
				{
					Config.PrintWidth = static_cast<int32>(Num);
				}
				if (Root->TryGetStringField(TEXT("indentStyle"), Str))
				{
					Config.IndentStyle = Str;
				}
				if (Root->TryGetNumberField(TEXT("indentSize"), Num))
				{
					Config.IndentSize = static_cast<int32>(Num);
				}
				if (Root->TryGetBoolField(TEXT("singleAttributePerLine"), bFlag))
				{
					Config.bSingleAttributePerLine = bFlag;
				}
				if (Root->TryGetBoolField(TEXT("insertSpaceBeforeSelfClose"), bFlag))
				{
					Config.bInsertSpaceBeforeSelfClose = bFlag;
				}
				// `~/` family-root anchor (A1/round-3). Empty string is treated as absent (falls
				// back to the module root) so an accidental `"root": ""` never anchors at ConfigDir.
				FString RootStr;
				if (Root->TryGetStringField(TEXT("root"), RootStr) && !RootStr.TrimStartAndEnd().IsEmpty())
				{
					Config.bHasRoot = true;
					Config.RootRelative = RootStr.TrimStartAndEnd();
				}
			}
			return Config; // nearest config wins, malformed = defaults (never half-applied)
		}
		const FString Parent = FPaths::GetPath(D);
		if (Parent == D)
		{
			break;
		}
		D = Parent;
	}
	return Config;
}

FString FUetkxConfig::ModuleRootFor(const FString& Path)
{
	// Byte-identical to the aggregator's grouping walk (UetkxDriver::BuildAggregators) so the two
	// never disagree on which module a file belongs to. Returns the `Dir` as produced by GetPath
	// (already `/`-normalized by UE); comparison sites normalize as needed.
	FString Dir = FPaths::GetPath(Path);
	while (!Dir.IsEmpty())
	{
		TArray<FString> BuildFiles;
		IFileManager::Get().FindFiles(BuildFiles, *(Dir / TEXT("*.Build.cs")), true, false);
		if (!BuildFiles.IsEmpty())
		{
			return Dir;
		}
		const FString Parent = FPaths::GetPath(Dir);
		if (Parent == Dir)
		{
			break;
		}
		Dir = Parent;
	}
	return FString();
}

FString FUetkxConfig::RootAnchorFor(const FString& UetkxAbsPath)
{
	const FString FileDir = FPaths::GetPath(UetkxAbsPath);
	const FUetkxConfig Config = Load(FileDir);
	if (Config.bHasRoot)
	{
		FString Anchor = FPaths::Combine(Config.ConfigDir, Config.RootRelative);
		FPaths::CollapseRelativeDirectories(Anchor);
		FPaths::NormalizeDirectoryName(Anchor);
		return Anchor;
	}
	return ModuleRootFor(UetkxAbsPath);
}

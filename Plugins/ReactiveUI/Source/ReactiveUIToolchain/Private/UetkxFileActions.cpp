// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.

#include "UetkxFileActions.h"

#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/FileHelper.h"

FString FUetkxFileActions::CreateComponentFile(const FString& Dir, const FString& Name)
{
	if (Name.IsEmpty() || !(Name[0] >= 'A' && Name[0] <= 'Z'))
	{
		return FString(); // the component grammar requires UpperCamel (UETKX2100)
	}
	for (const TCHAR C : Name)
	{
		if (!FChar::IsAlnum(C) && C != '_')
		{
			return FString();
		}
	}
	const FString Path = Dir / (Name + TEXT(".uetkx"));
	if (IFileManager::Get().FileExists(*Path))
	{
		return FString();
	}
	const FString Template = FString::Printf(TEXT("component %s {\n"
												  "\treturn (\n"
												  "\t\t<VerticalBox>\n"
												  "\t\t\t<TextBlock Text=\"%s\" />\n"
												  "\t\t</VerticalBox>\n"
												  "\t);\n"
												  "}\n"),
											 *Name, *Name);
	return FFileHelper::SaveStringToFile(Template, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)
			   ? Path
			   : FString();
}

void FUetkxFileActions::OpenExternal(const FString& Path)
{
	FPlatformProcess::LaunchFileInDefaultExternalApplication(*Path);
}

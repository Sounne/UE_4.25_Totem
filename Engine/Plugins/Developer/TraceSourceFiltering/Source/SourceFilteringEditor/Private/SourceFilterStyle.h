// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateTypes.h"

class FSourceFilterStyle
{
public:
	static void Initialize();
	static void Shutdown();

	static const ISlateStyle& Get();
	static const FName& GetStyleSetName();
	static const FSlateBrush* GetBrush(FName PropertyName, const ANSICHAR* Specifier = NULL)
	{
		return StyleSet->GetBrush(PropertyName, Specifier);
	}

private:
	static FSlateBrush* FilterBrush;
	static FSlateBrush* FilterSetOperationBrush;
	static FSlateBrush* FilterSetBrush;
	/** Singleton instances of this style. */
	static TSharedPtr< class FSlateStyleSet > StyleSet;	
	static FTextBlockStyle NormalText;
};

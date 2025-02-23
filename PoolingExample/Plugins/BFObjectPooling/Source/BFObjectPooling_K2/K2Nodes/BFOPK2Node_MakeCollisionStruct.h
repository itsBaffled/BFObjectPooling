// Copyright (c) 2024 Jack Holland 
// Licensed under the MIT License. See LICENSE.md file in repo root for full license information.

#pragma once

#include "CoreMinimal.h"
#include "K2Node.h"
#include "BFOPK2Node_MakeCollisionStruct.generated.h"



UCLASS()
class BFOBJECTPOOLING_K2_API UBFOPK2Node_MakeCollisionStruct : public UK2Node
{
	GENERATED_BODY()
public:
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
	virtual void AllocateDefaultPins() override;
	virtual void ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual bool IsNodePure() const override { return true; }
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual void PinDefaultValueChanged(UEdGraphPin* Pin) override;
	virtual void PostReconstructNode() override;

private:
	void UpdateOutputPinType();
};

// Copyright (c) 2024 Jack Holland 
// Licensed under the MIT License. See LICENSE.md file in repo root for full license information.

#include "BFOPK2Node_MakeCollisionStruct.h"
#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintNodeSpawner.h"
#include "K2Node_CallFunction.h"
#include "KismetCompiler.h"
#include "BFObjectPooling/PoolBP/BFObjectPoolingBlueprintFunctionLibrary.h"


namespace BF::OP
{
	static const FString ZeroStringValue = "0";
	static const FString DefaultCollisionProfile = "WorldDynamic";
	static const FString CollisionShapeToolTip = "The type of collision shape to use for a scene component that we sweep. Entirely valid to have no collision shape at all.";

	
	static const FText FunctionTitle = NSLOCTEXT("BFOP", "BFOPK2_MakeCollShape_Title", "Make FBFCollisionShapeDescription");
	static const FText FunctionCategory = NSLOCTEXT("BFOP", "BFOPK2_MakeCollShape_Category", "BFOPPool");
	static const FText FunctionTooltip = NSLOCTEXT("BFOP", "BFOPK2_MakeCollShape_ToolTip", "Makes a FBFCollisionShapeDescription struct, shape params are dynamically based on your input type.");

	static const FName FuncName = "MakeCollisionStruct";
	static const FName FuncParam1Name = "CollisionShape";
	static const FName FuncParam2Name = "CollisionProfile";
	static const FName FuncParam3Name = "XParam";
	static const FName FuncParam4Name = "YParam";
	static const FName FuncParam5Name = "ZParam";

	
	static const FName InputPinParam1Name = "Collision Shape";
	static const FName InputPinParam2Name = "Collision Profile";
	static const FName InputPinParam3Name = "X Extent"; 
	static const FName InputPinParam4Name = "Y Extent";
	static const FName InputPinParam5Name = "Z Extent";
	static const FName OutputPinParamName = "Collision Struct";

	
	static const FText InputPinParam3FriendlyName_Radius = NSLOCTEXT("BFOP","BFOPK2_MakeCollShape_Radius", "Radius"); 
	static const FText InputPinParam4FriendlyName_Height = NSLOCTEXT("BFOP","BFOPK2_MakeCollShape_Height", "Half Height");

	static const FText InputPinParam3FriendlyName_XExtent = NSLOCTEXT("BFOP","BFOPK2_MakeCollShape_ExtentX", "X Extent");
	static const FText InputPinParam4FriendlyName_YExtent = NSLOCTEXT("BFOP","BFOPK2_MakeCollShape_ExtentY", "Y Extent");
	static const FText InputPinParam5FriendlyName_ZExtent = NSLOCTEXT("BFOP","BFOPK2_MakeCollShape_ExtentZ", "Z Extent");
}

void UBFOPK2Node_MakeCollisionStruct::AllocateDefaultPins()
{
	UEdGraphPin* CollisionShapePin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Byte, BF::OP::InputPinParam1Name);
	UEnum* CollisionShapeEnum = StaticEnum<EBFCollisionShapeType>();
	CollisionShapePin->DefaultValue = CollisionShapeEnum->GetNameStringByIndex(0);
	CollisionShapePin->PinType.PinSubCategoryObject = CollisionShapeEnum;
	CollisionShapePin->PinToolTip = BF::OP::CollisionShapeToolTip;

	UEdGraphPin* CollisionProfilePin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Struct, BF::OP::InputPinParam2Name);
	CollisionProfilePin->PinType.PinSubCategoryObject = FCollisionProfileName::StaticStruct();
	CollisionProfilePin->DefaultValue = BF::OP::DefaultCollisionProfile;


	// Radius, Radius and Half Height or XYZ extents, depends on input type which ones we show and hide.
	UEdGraphPin* FloatPin1 = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Real, BF::OP::InputPinParam3Name);
	FloatPin1->PinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
	FloatPin1->DefaultValue = FString::FromInt(0);

	UEdGraphPin* FloatPin2 = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Real, BF::OP::InputPinParam4Name);
	 FloatPin2->PinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
	 FloatPin2->DefaultValue = FString::FromInt(0);

	UEdGraphPin* FloatPin3 = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Real, BF::OP::InputPinParam5Name);
	FloatPin3->PinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
	FloatPin3->DefaultValue = FString::FromInt(0);

	UEdGraphPin* OutputCollisionStructPin = CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Struct, BF::OP::OutputPinParamName);
	OutputCollisionStructPin->PinType.PinSubCategoryObject = FBFCollisionShapeDescription::StaticStruct();

	Super::AllocateDefaultPins();
}

FText UBFOPK2Node_MakeCollisionStruct::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return BF::OP::FunctionTitle;
}

void UBFOPK2Node_MakeCollisionStruct::PinDefaultValueChanged(UEdGraphPin* Pin)
{
	Super::PinDefaultValueChanged(Pin);
	UpdateOutputPinType();

}

FText UBFOPK2Node_MakeCollisionStruct::GetTooltipText() const
{
	return BF::OP::FunctionTooltip;
}

void UBFOPK2Node_MakeCollisionStruct::ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);
	
	UK2Node_CallFunction* CallFunction = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);

	CallFunction->SetFromFunction(UBFObjectPoolingBlueprintFunctionLibrary::StaticClass()->FindFunctionByName(BF::OP::FuncName));
	CallFunction->AllocateDefaultPins();

	UEdGraphPin* ParamCollisionType = FindPinChecked(BF::OP::InputPinParam1Name);
	UEdGraphPin* ParamCollisionProfile = FindPinChecked(BF::OP::InputPinParam2Name);
	UEdGraphPin* ParamX = FindPinChecked(BF::OP::InputPinParam3Name);
	UEdGraphPin* ParamY = FindPinChecked(BF::OP::InputPinParam4Name);
	UEdGraphPin* ParamZ = FindPinChecked(BF::OP::InputPinParam5Name);
	UEdGraphPin* OutputPin = FindPinChecked(BF::OP::OutputPinParamName);

	UEdGraphPin* CallFunctionParam1 = CallFunction->FindPinChecked(BF::OP::FuncParam1Name); // Shape type
	UEdGraphPin* CallFunctionParam2 = CallFunction->FindPinChecked(BF::OP::FuncParam2Name); // Profile
	UEdGraphPin* CallFunctionParam3 = CallFunction->FindPinChecked(BF::OP::FuncParam3Name); // X
	UEdGraphPin* CallFunctionParam4 = CallFunction->FindPinChecked(BF::OP::FuncParam4Name); // Y
	UEdGraphPin* CallFunctionParam5 = CallFunction->FindPinChecked(BF::OP::FuncParam5Name); // Z
	UEdGraphPin* CallFunctionReturnType = CallFunction->GetReturnValuePin(); // Return value

	CompilerContext.MovePinLinksToIntermediate(*ParamCollisionType, *CallFunctionParam1);
	CompilerContext.MovePinLinksToIntermediate(*ParamCollisionProfile, *CallFunctionParam2);
	CompilerContext.MovePinLinksToIntermediate(*ParamX, *CallFunctionParam3);
	CompilerContext.MovePinLinksToIntermediate(*ParamY, *CallFunctionParam4);
	CompilerContext.MovePinLinksToIntermediate(*ParamZ, *CallFunctionParam5);
	CompilerContext.MovePinLinksToIntermediate(*OutputPin, *CallFunctionReturnType);
	BreakAllNodeLinks();
}

FSlateIcon UBFOPK2Node_MakeCollisionStruct::GetIconAndTint(FLinearColor& OutColor) const
{
	return FSlateIcon(FAppStyle::GetAppStyleSetName(), "GraphEditor.MakeStruct_16x");
}

void UBFOPK2Node_MakeCollisionStruct::PostReconstructNode()
{
	Super::PostReconstructNode();
	UpdateOutputPinType();
}

void UBFOPK2Node_MakeCollisionStruct::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	Super::GetMenuActions(ActionRegistrar);
	UClass* Action = GetClass();
	if (ActionRegistrar.IsOpenForRegistration(Action))
	{
		UBlueprintNodeSpawner* Spawner = UBlueprintNodeSpawner::Create(GetClass());
		ActionRegistrar.AddBlueprintAction(Action, Spawner);
	}
}

void UBFOPK2Node_MakeCollisionStruct::UpdateOutputPinType()
{
	UEdGraphPin* ParamCollisionType = FindPinChecked(BF::OP::InputPinParam1Name);
	UEdGraphPin* ParamCollisionProfile = FindPinChecked(BF::OP::InputPinParam2Name);
	UEdGraphPin* ParamX = FindPinChecked(BF::OP::InputPinParam3Name);
	UEdGraphPin* ParamY = FindPinChecked(BF::OP::InputPinParam4Name);
	UEdGraphPin* ParamZ = FindPinChecked(BF::OP::InputPinParam5Name);
	
	
	if(ParamCollisionType->PinType.PinSubCategoryObject == nullptr)
	{
		ParamCollisionType->PinType.PinCategory = UEdGraphSchema_K2::PC_Byte;
		ParamCollisionType->PinType.PinSubCategoryObject = StaticEnum<EBFCollisionShapeType>();
		ParamCollisionType->DefaultValue = StaticEnum<EBFCollisionShapeType>()->GetNameStringByIndex(0);
		
		ParamCollisionProfile->PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		ParamCollisionProfile->PinType.PinSubCategoryObject = FCollisionProfileName::StaticStruct();
		ParamCollisionProfile->DefaultValue = BF::OP::DefaultCollisionProfile;
		
		ParamX->PinType.PinCategory = UEdGraphSchema_K2::PC_Real;
		ParamX->DefaultValue = BF::OP::ZeroStringValue;
		ParamY->PinType.PinCategory = UEdGraphSchema_K2::PC_Real;
		ParamY->DefaultValue = BF::OP::ZeroStringValue;
		ParamZ->PinType.PinCategory = UEdGraphSchema_K2::PC_Real;
		ParamZ->DefaultValue = BF::OP::ZeroStringValue;
	}
	else
	{
		FString SelectedValue = ParamCollisionType->GetDefaultAsString();
		EBFCollisionShapeType SelectedType = (EBFCollisionShapeType)StaticEnum<EBFCollisionShapeType>()->GetIndexByNameString(SelectedValue);
		switch (SelectedType)
		{
			case EBFCollisionShapeType::NoCollisionShape:
			{
				ParamX->bHidden = true;
				ParamX->DefaultValue = BF::OP::ZeroStringValue;
				ParamY->bHidden = true;
				ParamY->DefaultValue = BF::OP::ZeroStringValue;
				ParamZ->bHidden = true;
				ParamZ->DefaultValue = BF::OP::ZeroStringValue;
				break;
			}
			case EBFCollisionShapeType::Sphere:
			{
				ParamX->bHidden = false;
				ParamX->PinFriendlyName = BF::OP::InputPinParam3FriendlyName_Radius;

				ParamY->bHidden = true;
				ParamY->DefaultValue = BF::OP::ZeroStringValue;
				ParamZ->bHidden = true;
				ParamZ->DefaultValue = BF::OP::ZeroStringValue;
				break;
			}
			case EBFCollisionShapeType::Capsule:
			{
				ParamX->bHidden = false;
				ParamX->PinFriendlyName = BF::OP::InputPinParam3FriendlyName_Radius;

				ParamY->bHidden = false;
				ParamY->PinFriendlyName = BF::OP::InputPinParam4FriendlyName_Height;

				ParamZ->bHidden = true;
				ParamZ->DefaultValue = BF::OP::ZeroStringValue;
				break;
			}
			case EBFCollisionShapeType::Box:
			{
				ParamX->bHidden = false;
				ParamX->PinFriendlyName = BF::OP::InputPinParam3FriendlyName_XExtent;
			
				ParamY->bHidden = false;
				ParamY->PinFriendlyName = BF::OP::InputPinParam4FriendlyName_YExtent;
			
				ParamZ->bHidden = false;
				ParamZ->PinFriendlyName = BF::OP::InputPinParam5FriendlyName_ZExtent;
				break;
			}
		}
	}
	// Update the node so we reflect the new hidden state of the pins.
	GetGraph()->NotifyGraphChanged();
}


FLinearColor UBFOPK2Node_MakeCollisionStruct::GetNodeTitleColor() const
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	FEdGraphPinType PinType;
	PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
	PinType.PinSubCategoryObject = FBFCollisionShapeDescription::StaticStruct();
	return K2Schema->GetPinTypeColor(PinType);
}



















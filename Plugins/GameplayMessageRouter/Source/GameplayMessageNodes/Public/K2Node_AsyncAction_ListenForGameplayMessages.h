// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "K2Node_AsyncAction.h"

#include "K2Node_AsyncAction_ListenForGameplayMessages.generated.h"

class FBlueprintActionDatabaseRegistrar;
class FKismetCompilerContext;
class FMulticastDelegateProperty;
class FString;
class UEdGraph;
class UEdGraphPin;
class UObject;

/** 为 Gameplay Message 异步监听代理生成并编译专用流程的蓝图节点。 */
/**
 * Blueprint node which is spawned to handle the async logic for UAsyncAction_RegisterGameplayMessageReceiver
 */

UCLASS()
class UK2Node_AsyncAction_ListenForGameplayMessages : public UK2Node_AsyncAction
{
	GENERATED_BODY()

	//~UEdGraphNode 接口
	//~UEdGraphNode interface
	virtual void PostReconstructNode() override;
	virtual void PinDefaultValueChanged(UEdGraphPin* ChangedPin) override;
	virtual void GetPinHoverText(const UEdGraphPin& Pin, FString& HoverTextOut) const override;
	//~UEdGraphNode 接口结束
	//~End of UEdGraphNode interface
	
	//~UK2Node 接口
	//~UK2Node interface
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual void AllocateDefaultPins() override;
	//~UK2Node 接口结束
	//~End of UK2Node interface

protected:
	virtual bool HandleDelegates(
		const TArray<FBaseAsyncTaskHelper::FOutputPinAndLocalVariable>& VariableOutputs, UEdGraphPin* ProxyObjectPin,
		UEdGraphPin*& InOutLastThenPin, UEdGraph* SourceGraph, FKismetCompilerContext& CompilerContext) override;

private:
	
	// 在委托处理流程末尾插入 GetPayload 调用与载荷赋值节点。
	// Add the GetPayload flow to the end of the delegate handler's logic chain
	bool HandlePayloadImplementation(
		FMulticastDelegateProperty* CurrentProperty,
		const FBaseAsyncTaskHelper::FOutputPinAndLocalVariable& ProxyObjectVar,
		const FBaseAsyncTaskHelper::FOutputPinAndLocalVariable& PayloadVar,
		const FBaseAsyncTaskHelper::FOutputPinAndLocalVariable& ActualChannelVar,
		UEdGraphPin*& InOutLastActivatedThenPin, UEdGraph* SourceGraph, FKismetCompilerContext& CompilerContext);

	// 使输出 Payload 通配符类型与输入 PayloadType 保持一致。
	// Make sure the output Payload wildcard matches the input PayloadType 
	void RefreshOutputPayloadType();

	UEdGraphPin* GetPayloadPin() const;
	UEdGraphPin* GetPayloadTypePin() const;
	UEdGraphPin* GetOutputChannelPin() const;
};

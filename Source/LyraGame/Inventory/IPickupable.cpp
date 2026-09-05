// Copyright Epic Games, Inc. All Rights Reserved.

#include "IPickupable.h"

#include "GameFramework/Actor.h"
#include "LyraInventoryManagerComponent.h"
#include "UObject/ScriptInterface.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(IPickupable)

class UActorComponent;

// 构造可拾取对象蓝图函数库，函数库本身不保存运行时状态。
UPickupableStatics::UPickupableStatics()
	: Super(FObjectInitializer::Get())
{
}

// 优先返回 Actor 自身的 IPickupable；否则返回其组件中找到的第一个可拾取接口。
TScriptInterface<IPickupable> UPickupableStatics::GetFirstPickupableFromActor(AActor* Actor)
{
	// Actor 自身实现 IPickupable 时直接返回。
	// If the actor is directly pickupable, return that.
	TScriptInterface<IPickupable> PickupableActor(Actor);
	if (PickupableActor)
	{
		return PickupableActor;
	}

	// Actor 自身不可拾取时，继续查找实现接口的 ActorComponent。
	// If the actor isn't pickupable, it might have a component that has a pickupable interface.
	TArray<UActorComponent*> PickupableComponents = Actor ? Actor->GetComponentsByInterface(UPickupable::StaticClass()) : TArray<UActorComponent*>();
	if (PickupableComponents.Num() > 0)
	{
		// 当前直接取第一个可拾取组件；更复杂的多组件选择策略需由上层扩展。
		// Get first pickupable, if the user needs more sophisticated pickup distinction, will need to be solved elsewhere.
		return TScriptInterface<IPickupable>(PickupableComponents[0]);
	}

	return TScriptInterface<IPickupable>();
}

// 将拾取物声明的定义模板和现成实例依次交给库存组件添加，不在此执行容量或所有权校验。
void UPickupableStatics::AddPickupToInventory(ULyraInventoryManagerComponent* InventoryComponent, TScriptInterface<IPickupable> Pickup)
{
	if (InventoryComponent && Pickup)
	{
		const FInventoryPickup& PickupInventory = Pickup->GetPickupInventory();

		for (const FPickupTemplate& Template : PickupInventory.Templates)
		{
			InventoryComponent->AddItemDefinition(Template.ItemDef, Template.StackCount);
		}

		for (const FPickupInstance& Instance : PickupInventory.Instances)
		{
			InventoryComponent->AddItemInstance(Instance.Item);
		}
	}
}

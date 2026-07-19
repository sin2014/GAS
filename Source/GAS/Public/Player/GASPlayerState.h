// ZYZ

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/PlayerState.h"
#include "GASPlayerState.generated.h"

class UAbilitySystemComponent;
class UAttributeSet;
class ULevelUpInfo;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnPlayerStateChanged, int32 /*StateValue*/)

/**
 * 
 */
UCLASS()
class GAS_API AGASPlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()
	
public:
	AGASPlayerState();
	
	// (Replication)复制变量需要重写GetLifetimeReplicatedProps
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	UAttributeSet* GetAttributeSet() const;
	
	UPROPERTY(EditDefaultsOnly)
	TObjectPtr<ULevelUpInfo> LevelUpInfo;
	
	// 玩家等级和XP变化委托
	FOnPlayerStateChanged OnLevelChangedDelegate;
	FOnPlayerStateChanged OnXPChangedDelegate;
	
	// 玩家属性点数和技能点数变化委托
	FOnPlayerStateChanged OnAttributePointsChangedDelegate;
	FOnPlayerStateChanged OnSpellPointsChangedDelegate;

	FORCEINLINE int32 GetPlayerLevel() const { return Level; }
	FORCEINLINE int32 GetPlayerXP() const { return XP; }
	FORCEINLINE int32 GetAttributePoints() const { return AttributePoints; }
	FORCEINLINE int32 GetSpellPoints() const { return SpellPoints; }
	
	void AddLevel(int32 InLevel);
	void SetLevel(int32 InLevel);
	
	void AddXP(int32 InXP);
	void SetXP(int32 InXP);
	
	void AddAttributePoints(int32 InAttributePoints);
	void SetAttributePoints(int32 InAttributePoints);
	
	void AddSpellPoints(int32 InSpellPoints);
	void SetSpellPoints(int32 InSpellPoints);
	
protected:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;
	
	UPROPERTY()
	TObjectPtr<UAttributeSet> AttributeSet;
	
private:
	UPROPERTY(VisibleAnywhere, ReplicatedUsing = OnRep_Level)
	int32 Level = 1;
	
	UPROPERTY(VisibleAnywhere, ReplicatedUsing = OnRep_XP)
	int32 XP = 1;
	
	UPROPERTY(VisibleAnywhere, ReplicatedUsing = OnRep_AttributePoints)
	int32 AttributePoints = 0;
	
	UPROPERTY(VisibleAnywhere, ReplicatedUsing = OnRep_SpellPoints)
	int32 SpellPoints = 1;
	
	UFUNCTION()
	void OnRep_Level(int32 OldLevel);
	
	UFUNCTION()
	void OnRep_XP(int32 OldXP);
	
	UFUNCTION()
	void OnRep_AttributePoints(int32 OldAttributePoints);
	
	UFUNCTION()
	void OnRep_SpellPoints(int32 OldSpellPoints);
};

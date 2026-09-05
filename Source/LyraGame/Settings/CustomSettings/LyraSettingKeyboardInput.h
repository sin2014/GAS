// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EnhancedActionKeyMapping.h"
#include "GameSettingValue.h"
#include "UserSettings/EnhancedInputUserSettings.h"

#include "LyraSettingKeyboardInput.generated.h"

class UObject;

//--------------------------------------
// ULyraSettingKeyboardInput
//--------------------------------------

UCLASS()
class ULyraSettingKeyboardInput : public UGameSettingValue
{
	GENERATED_BODY()

public:
	ULyraSettingKeyboardInput();

	void InitializeInputData(const UEnhancedPlayerMappableKeyProfile* KeyProfile, const FKeyMappingRow& MappingData, const FPlayerMappableKeyQueryOptions& QueryOptions);

	FText GetKeyTextFromSlot(const EPlayerMappableKeySlot InSlot) const;
	
	virtual void StoreInitial() override;
	virtual void ResetToDefault() override;
	virtual void RestoreToInitial() override;

	bool ChangeBinding(int32 InKeyBindSlot, FKey NewKey);
	void GetAllMappedActionsFromKey(int32 InKeyBindSlot, FKey Key, TArray<FName>& OutActionNames) const;

	/** 此设置项中的任一按键映射已偏离默认值时返回 true。 */
	/** Returns true if mappings on this setting have been customized */
	bool IsMappingCustomized() const;
	
	FText GetSettingDisplayName() const;
	FText GetSettingDisplayCategory() const;

	const FKeyMappingRow* FindKeyMappingRow() const;
	UEnhancedPlayerMappableKeyProfile* FindMappableKeyProfile() const;
	UEnhancedInputUserSettings* GetUserSettings() const;
	
protected:
	/** ULyraSetting */
	virtual void OnInitialized() override;

protected:

	/** 此设置项所对应的玩家可映射操作名称。 */
	/** The name of this action's mappings */
	FName ActionMappingName;

	/** 用于筛选此设置项应包含哪些按键映射的查询条件。 */
	/** The query options to filter down keys on this setting for */
	FPlayerMappableKeyQueryOptions QueryOptions;

	/** 此按键设置所属的玩家按键配置标识。 */
	/** The profile identifier that this key setting is from */
	FString ProfileIdentifier;

	/** 保存各映射槽位进入设置界面时的初始按键，用于撤销未应用的修改。 */
	/** Store the initial key mappings that are set on this for each slot */
	TMap<EPlayerMappableKeySlot, FKey> InitialKeyMappings;
};

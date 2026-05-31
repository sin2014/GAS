# MMM 人物属性定义 v2

## 目标

本文档只整理人物属性的命名、来源和实现边界，不新增战斗、装备、技能、存档或 UI 系统。

当前规则基准：

- 逆向证据以 `MM逻辑逆向/docs/02_FIELD_DICTIONARY.md` 的 `PcRuntime` 和 `MM逻辑逆向/docs/modules/pc_growth.md` 为主。
- 游戏显示术语以用户确认的截图和菜单内容为准。
- MMM 可以在原作基础上增加项目自定义字段，但必须标明“MMM 自定义扩展”，不能伪装成原作逆向字段。

## 已确认修正

### 人物装备槽

原作 MM4 当前逆向确认的人物装备槽是 `PcRuntime+0x36..+0x44`，结构为 `u16[8]`。

MMM 项目人物装备槽采用 9 个：

| 槽位 | 中文名 | 来源 |
|---|---|---|
| Weapon1 | 武器1 | 原作装备槽 |
| Weapon2 | 武器2 | 原作装备槽 |
| Weapon3 | 武器3 | 原作装备槽 |
| Head | 头部 | 原作装备槽 |
| Body | 身体 | 原作装备槽 |
| Hands | 手部 | 原作装备槽 |
| Legs | 腿部 | MMM 自定义扩展 |
| Feet | 脚 | 原作装备槽 |
| Accessory | 饰品 | 原作装备槽 |

代码注释要求：

```cpp
// 腿部装备槽是 MMM 项目新增槽位，原作 MM4 PcRuntime 当前逆向未确认独立腿部槽。
```

## 升级直接变化属性

用户确认升级时游戏里提升或变化的参数如下：

| 属性名建议 | 中文显示名 | 是否升级直接变化 | 逆向依据 | 备注 |
|---|---|---:|---|---|
| Level | 等级 | 是 | `PcRuntime+0x18` | 当前等级。 |
| MaxHP | 最大 HP | 是 | `PcRuntime+0x134` | 派生最大 HP，入战时写入 BattleActor 最大值。 |
| Strength | 腕力 | 是 | `PcRuntime+0x26..+0x30` 六项基础属性之一 | 具体 lane 顺序仍需和菜单显示/成长函数校准。 |
| Vitality | 体力 | 是 | `PcRuntime+0x26..+0x30` 六项基础属性之一 | 具体 lane 顺序仍需校准。 |
| Agility | 敏捷度 | 是 | `PcRuntime+0x26..+0x30` 六项基础属性之一 | 不再使用 `Speed` 作为正式属性名。 |
| BattleLevel | 战斗等级 | 是 | `PcRuntime+0x26..+0x30` 六项基础属性之一 | 具体 lane 顺序仍需校准。 |
| DrivingLevel | 驾驶等级 | 是 | `PcRuntime+0x26..+0x30` 六项基础属性之一 | 具体 lane 顺序仍需校准。 |
| Manliness | 男子气概 | 是 | `PcRuntime+0x26..+0x30` 六项基础属性之一 | 女性角色升级时可能下降。 |

注意：

- `PcRuntime+0x26..+0x30` 已确认是升级直接增长的 6 个基础属性。
- 这 6 个属性在项目中按显示术语命名为：腕力、体力、敏捷度、战斗等级、驾驶等级、男子气概。
- 具体数组 lane 顺序仍需要用菜单显示顺序、成长函数输入和截图进一步校准后再写死。

## 生命属性

| 属性名建议 | 中文显示名 | 是否进 GAS | 是否升级直接变化 | 来源 | 备注 |
|---|---|---:|---:|---|---|
| CurrentHP | 当前 HP | 是 | 否 | `PcRuntime+0x24` | 战斗中会变化，战斗结束或保存点回写。 |
| MaxHP | 最大 HP | 是 | 是 | `PcRuntime+0x134` | 由成长/派生公式刷新。 |

## 成长属性

| 属性名建议 | 中文显示名 | 是否进 GAS | 来源 | 备注 |
|---|---|---:|---|---|
| Level | 等级 | 是 | `PcRuntime+0x18` | 用于经验阈值、成长和技能学习。 |
| LevelCap | 等级上限 | 是 | `PcRuntime+0x1A` | 用于限制最高等级。 |
| Experience | 经验值 | 是 | `PcRuntime+0x1C` | 累计经验。 |
| ExperienceToNextLevel | 距离下一级经验 | 可选 | 派生显示值 | 不是原作字段本体，可以作为 UI 派生值。 |
| SkillUpgradePoints | 技能强化点数 | 是 | `PcRuntime+0x20` | 技能强化点数池。 |
| SkillUpgradeProgress | 技能强化进度 | 是 | `PcRuntime+0x22` | 累计获得下一点技能强化点数的进度。 |

## 核心显示属性

| 属性名建议 | 中文显示名 | 是否进 GAS | 是否升级直接变化 | 备注 |
|---|---|---:|---:|---|
| Strength | 腕力 | 是 | 是 | 角色力量相关能力。 |
| Vitality | 体力 | 是 | 是 | 角色生命力和耐久相关能力。 |
| Agility | 敏捷度 | 是 | 是 | 影响行动顺序、回避相关计算和部分战斗判定。 |
| BattleLevel | 战斗等级 | 是 | 是 | 人身战斗熟练度。 |
| DrivingLevel | 驾驶等级 | 是 | 是 | 驾驶战车和载具战斗的熟练度。 |
| Manliness | 男子气概 | 是 | 是 | 原作特殊属性；女性角色升级时可能下降。 |
| Scars | 伤痕 | 是 | 否 | `PcRuntime+0x32`，死亡/战斗退出同步链已确认。 |

## 战斗派生属性

这些属性可以进入 GAS，但不应视为升级直接增长字段。它们后续应由装备、技能、状态、派生公式或战斗初始化刷新。

| 属性名建议 | 中文显示名 | 是否进 GAS | 是否升级直接变化 | 备注 |
|---|---|---:|---:|---|
| AttackPower | 攻击力 | 是 | 否 | 综合攻击能力，后续需要由人物基础值、武器和状态派生。 |
| DefensePower | 防御力 | 是 | 否 | 综合防御能力，后续需要由装备和状态派生。 |
| HitRate | 命中率 | 是 | 否 | 100.0 表示 100%。 |
| EvasionRate | 回避率 | 是 | 否 | 100.0 表示 100%。 |
| CriticalRate | 会心率 | 是 | 否 | 100.0 表示 100%。 |

## 属性防御

元素防御固定为用户确认的六类：

| 属性名建议 | 中文显示名 | 是否进 GAS | 是否升级直接变化 | 备注 |
|---|---|---:|---:|---|
| FireDefense | 火焰防御 | 是 | 否 | 抵抗火焰属性攻击。 |
| ColdDefense | 冷气防御 | 是 | 否 | 抵抗冷气属性攻击。 |
| ElectricDefense | 电气防御 | 是 | 否 | 抵抗电气属性攻击。 |
| SonicDefense | 音波防御 | 是 | 否 | 抵抗音波属性攻击。 |
| GasDefense | 瓦斯防御 | 是 | 否 | 抵抗瓦斯属性攻击。 |
| BeamDefense | 光束防御 | 是 | 否 | 抵抗光束属性攻击。 |

## 暂不进入 AttributeSet 的字段

这些字段属于人物长期结构或系统数据，但不适合放进 `UMMCharacterAttributeSet`。

| 字段 | 来源 | 原因 |
|---|---|---|
| CharacterType | `PcRuntime+0x10` | 枚举/身份数据，不是 GAS 数值属性。 |
| FormOrAvatar | `PcRuntime+0x11` | 外观或形态数据，后续应由角色数据或状态组件维护。 |
| Flags | `PcRuntime+0x14` | 位标志，不适合直接作为普通数值属性。 |
| EquipmentSlots | `PcRuntime+0x36..+0x44` + MMM 腿部扩展 | 装备槽是物品引用，不是 GAS 属性。 |
| SkillSlots | `PcRuntime+0x46` 等 | 技能槽包含 action、次数、variant，不是单一数值属性。 |
| PcInternalField34 | `PcRuntime+0x34` | 用途高置信但正式业务名未定，暂不暴露。 |
| PcDamageSourceStat14C | `PcRuntime+0x14C` | 战斗读取已确认，但正式业务名未定，暂不作为玩家显示属性。 |

## 当前代码调整记录

当前 `UMMCharacterAttributeSet` 已经定义了大部分第一版属性，按本文件需要持续校准。

已完成：

1. `Speed` 已改为 `Agility`，中文注释已改为“敏捷度”。

后续注意：

1. 注释里不要把命中率、回避率、会心率写成升级直接变化属性。
2. 装备槽不要放进 `AttributeSet`。
3. 技能槽不要放进 `AttributeSet`。
4. `Legs` 装备槽后续实现时必须标注为 MMM 自定义扩展。
5. `PcRuntime+0x26..+0x30` 的六项 lane 顺序未完全校准前，不要写死原作数组下标到业务名的映射。

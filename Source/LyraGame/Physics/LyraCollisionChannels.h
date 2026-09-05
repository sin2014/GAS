// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


// 碰撞通道编号可能被实例序列化保存；修改本列表时必须同步 DefaultEngine.ini 的 [/Script/Engine.CollisionProfile] 配置。
/**
 * when you modify this, please note that this information can be saved with instances
 * also DefaultEngine.ini [/Script/Engine.CollisionProfile] should match with this list
 **/

// 用于查询可交互 Actor 或组件的射线通道。
// Trace against Actors/Components which provide interactions.
#define Lyra_TraceChannel_Interaction					ECC_GameTraceChannel1

// 武器精确命中通道：命中 Pawn 的 Physics Asset，而不是角色胶囊体。
// Trace used by weapons, will hit physics assets instead of capsules
#define Lyra_TraceChannel_Weapon						ECC_GameTraceChannel2

// 武器粗略命中通道：命中 Pawn 胶囊体，而不是 Physics Asset。
// Trace used by by weapons, will hit pawn capsules instead of physics assets
#define Lyra_TraceChannel_Weapon_Capsule				ECC_GameTraceChannel3

// 武器多目标通道：允许查询穿过多个 Pawn，不在首次命中时停止。
// Trace used by by weapons, will trace through multiple pawns rather than stopping on the first hit
#define Lyra_TraceChannel_Weapon_Multi					ECC_GameTraceChannel4

// ECC_GameTraceChannel5 预留给 ShooterCore Game Feature 的辅助瞄准查询。
// Allocated to aim assist by the ShooterCore game feature
// ECC_GameTraceChannel5

// Copyright Epic Games, Inc. All Rights Reserved.

#include "TimerNodeHelper.h"
#include "EditorStyleSet.h"

#define LOCTEXT_NAMESPACE "TimerNode"

////////////////////////////////////////////////////////////////////////////////////////////////////
// TimerNode Type Helper
////////////////////////////////////////////////////////////////////////////////////////////////////

FText TimerNodeTypeHelper::ToText(const ETimerNodeType NodeType)
{
	static_assert(static_cast<int>(ETimerNodeType::InvalidOrMax) == 4, "Not all cases are handled in switch below!?");
	switch (NodeType)
	{
		case ETimerNodeType::GpuScope:		return LOCTEXT("Timer_Name_Gpu", "GPU");
		case ETimerNodeType::ComputeScope:	return LOCTEXT("Timer_Name_Compute", "Compute");
		case ETimerNodeType::CpuScope:		return LOCTEXT("Timer_Name_Cpu", "CPU");
		case ETimerNodeType::Group:			return LOCTEXT("Timer_Name_Group", "Group");
		default:							return LOCTEXT("InvalidOrMax", "InvalidOrMax");
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FText TimerNodeTypeHelper::ToDescription(const ETimerNodeType NodeType)
{
	static_assert(static_cast<int>(ETimerNodeType::InvalidOrMax) == 4, "Not all cases are handled in switch below!?");
	switch (NodeType)
	{
		case ETimerNodeType::GpuScope:		return LOCTEXT("Timer_Desc_Gpu", "GPU scope timer");
		case ETimerNodeType::ComputeScope:	return LOCTEXT("Timer_Desc_Compute", "Compute scope timer");
		case ETimerNodeType::CpuScope:		return LOCTEXT("Timer_Desc_Cpu", "CPU scope timer");
		case ETimerNodeType::Group:			return LOCTEXT("Timer_Desc_Group", "Group timer node");
		default:							return LOCTEXT("InvalidOrMax", "InvalidOrMax");
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FName TimerNodeTypeHelper::ToBrushName(const ETimerNodeType NodeType)
{
	static_assert(static_cast<int>(ETimerNodeType::InvalidOrMax) == 4, "Not all cases are handled in switch below!?");
	switch (NodeType)
	{
		case ETimerNodeType::GpuScope:		return TEXT("Profiler.FiltersAndPresets.StatTypeIcon"); //TODO: "Icons.GpuTimer"
		case ETimerNodeType::ComputeScope:	return TEXT("Profiler.FiltersAndPresets.StatTypeIcon"); //TODO: "Icons.ComputeTimer"
		case ETimerNodeType::CpuScope:		return TEXT("Profiler.FiltersAndPresets.StatTypeIcon"); //TODO: "Icons.CpuTimer"
		case ETimerNodeType::Group:			return TEXT("Profiler.Misc.GenericGroup"); //TODO: "Icons.GenericGroup"
		default:							return NAME_None;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

const FSlateBrush* TimerNodeTypeHelper::GetIconForGroup()
{
	return FEditorStyle::GetBrush(TEXT("Profiler.Misc.GenericGroup")); //TODO: FInsightsStyle::GetBrush(TEXT("Icons.GenericGroup"));
}

////////////////////////////////////////////////////////////////////////////////////////////////////

const FSlateBrush* TimerNodeTypeHelper::GetIconForTimerNodeType(const ETimerNodeType NodeType)
{
	static_assert(static_cast<int>(ETimerNodeType::InvalidOrMax) == 4, "Not all cases are handled in switch below!?");
	switch (NodeType)
	{
		case ETimerNodeType::GpuScope:		return FEditorStyle::GetBrush(TEXT("Profiler.Type.NumberFloat")); //TODO: FInsightsStyle::GetBrush(TEXT("Icons.GpuTimer"));
		case ETimerNodeType::ComputeScope:	return FEditorStyle::GetBrush(TEXT("Profiler.Type.NumberFloat")); //TODO: FInsightsStyle::GetBrush(TEXT("Icons.ComputeTimer"));
		case ETimerNodeType::CpuScope:		return FEditorStyle::GetBrush(TEXT("Profiler.Type.NumberFloat")); //TODO: FInsightsStyle::GetBrush(TEXT("Icons.CpuTimer"));
		case ETimerNodeType::Group:			return FEditorStyle::GetBrush(TEXT("Profiler.Misc.GenericGroup")); //TODO: FInsightsStyle::GetBrush(TEXT("Icons.GenericGroup"));
		default:							return nullptr;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// TimerNode Grouping Helper
////////////////////////////////////////////////////////////////////////////////////////////////////

FText TimerNodeGroupingHelper::ToText(const ETimerGroupingMode GroupingMode)
{
	static_assert(static_cast<int>(ETimerGroupingMode::InvalidOrMax) == 7, "Not all cases are handled in switch below!?");
	switch (GroupingMode)
	{
		case ETimerGroupingMode::Flat:					return LOCTEXT("Grouping_Name_Flat",				"Flat");
		case ETimerGroupingMode::ByName:				return LOCTEXT("Grouping_Name_ByName",				"Timer Name");
		case ETimerGroupingMode::ByMetaGroupName:		return LOCTEXT("Grouping_Name_MetaGroupName",		"Meta Group Name");
		case ETimerGroupingMode::ByType:				return LOCTEXT("Grouping_Name_Type",				"Timer Type");
		case ETimerGroupingMode::ByTotalInclusiveTime:	return LOCTEXT("Grouping_Name_TotalInclusiveTime",	"Total Inclusive Time");
		case ETimerGroupingMode::ByTotalExclusiveTime:	return LOCTEXT("Grouping_Name_TotalExclusiveTime",	"Total Exclusive Time");
		case ETimerGroupingMode::ByInstanceCount:		return LOCTEXT("Grouping_Name_InstanceCount",		"Instance Count");
		default:										return LOCTEXT("InvalidOrMax", "InvalidOrMax");
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FText TimerNodeGroupingHelper::ToDescription(const ETimerGroupingMode GroupingMode)
{
	static_assert(static_cast<int>(ETimerGroupingMode::InvalidOrMax) == 7, "Not all cases are handled in switch below!?");
	switch (GroupingMode)
	{
		case ETimerGroupingMode::Flat:					return LOCTEXT("Grouping_Desc_Flat",				"Creates a single group. Includes all timers.");
		case ETimerGroupingMode::ByName:				return LOCTEXT("Grouping_Desc_ByName",				"Creates one group for one letter.");
		case ETimerGroupingMode::ByMetaGroupName:		return LOCTEXT("Grouping_Desc_MetaGroupName",		"Creates groups based on metadata group names of timers.");
		case ETimerGroupingMode::ByType:				return LOCTEXT("Grouping_Desc_Type",				"Creates one group for each timer type.");
		case ETimerGroupingMode::ByTotalInclusiveTime:	return LOCTEXT("Grouping_Desc_TotalInclusiveTime",	"Creates one group for each logarithmic range ie. 0.001 - 0.01, 0.01 - 0.1, 0.1 - 1.0, 1.0 - 10.0 etc");
		case ETimerGroupingMode::ByTotalExclusiveTime:	return LOCTEXT("Grouping_Desc_TotalExclusiveTime",	"Creates one group for each logarithmic range ie. 0.001 - 0.01, 0.01 - 0.1, 0.1 - 1.0, 1.0 - 10.0 etc");
		case ETimerGroupingMode::ByInstanceCount:		return LOCTEXT("Grouping_Desc_InstanceCount",		"Creates one group for each logarithmic range ie. 0, 1 - 10, 10 - 100, 100 - 1000, etc");
		default:										return LOCTEXT("InvalidOrMax", "InvalidOrMax");
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FName TimerNodeGroupingHelper::ToBrushName(const ETimerGroupingMode GroupingMode)
{
	static_assert(static_cast<int>(ETimerGroupingMode::InvalidOrMax) == 7, "Not all cases are handled in switch below!?");
	switch (GroupingMode)
	{
		case ETimerGroupingMode::Flat:					return TEXT("Profiler.FiltersAndPresets.GroupNameIcon"); //TODO: "Icons.Grouping.Flat"
		case ETimerGroupingMode::ByName:				return TEXT("Profiler.FiltersAndPresets.GroupNameIcon"); //TODO: "Icons.Grouping.ByName"
		case ETimerGroupingMode::ByMetaGroupName	:	return TEXT("Profiler.FiltersAndPresets.StatNameIcon"); //TODO
		case ETimerGroupingMode::ByType:				return TEXT("Profiler.FiltersAndPresets.StatTypeIcon"); //TODO
		case ETimerGroupingMode::ByTotalInclusiveTime:	return TEXT("Profiler.FiltersAndPresets.StatValueIcon"); //TODO
		case ETimerGroupingMode::ByTotalExclusiveTime:	return TEXT("Profiler.FiltersAndPresets.StatValueIcon"); //TODO
		case ETimerGroupingMode::ByInstanceCount:		return TEXT("Profiler.FiltersAndPresets.StatValueIcon"); //TODO
		default:										return NAME_None;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE

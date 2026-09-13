#pragma once

namespace rage
{
	class netSyncTree;
}

namespace YimMenu::Hooks::Protections
{
	// Cheap pre-validation for sync nodes whose serialized counts are later used
	// as array indices by the legacy protection visitor.
	bool HasUnsafeSyncBounds(rage::netSyncTree* tree);
}

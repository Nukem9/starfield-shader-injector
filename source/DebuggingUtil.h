#pragma once

struct ID3D12Object;

namespace DebuggingUtil
{
	void SetObjectDebugName(ID3D12Object *Object, const char *Name);
}

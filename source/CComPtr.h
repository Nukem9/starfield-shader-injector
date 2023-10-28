#pragma once

#ifndef __WRL_CLASSIC_COM_STRICT__
#define __WRL_CLASSIC_COM_STRICT__
#endif // __WRL_CLASSIC_COM_STRICT__

#ifndef __WRL_NO_DEFAULT_LIB__
#define __WRL_NO_DEFAULT_LIB__
#endif // __WRL_NO_DEFAULT_LIB__

#include <wrl/client.h>

template<typename T>
using CComPtr = Microsoft::WRL::ComPtr<T>;

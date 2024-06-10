#pragma once

// 改变进程特权, isOpen 为 true则打开该特权，false则关闭
bool OpProcessPrivilege(const wchar_t* privName, bool isOpen);

// 是否开启 SeEnableDebug权限
bool SeEnableDebugPrivilege(bool isOpen = true);


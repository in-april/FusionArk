#pragma once

// �ı������Ȩ, isOpen Ϊ true��򿪸���Ȩ��false��ر�
bool OpProcessPrivilege(const wchar_t* privName, bool isOpen);

// �Ƿ��� SeEnableDebugȨ��
bool SeEnableDebugPrivilege(bool isOpen = true);


#pragma once

// ��ȡָ����ַ��ָ��ƫ�ƴ�������
template <typename T>
T getData(const void* address, int offset = 0)
{
	return *reinterpret_cast<const T*>(reinterpret_cast<const uint8_t*>(address) + offset);
}



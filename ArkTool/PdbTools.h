#pragma once
#include <map>
#include <string>

// ���ݽṹ�����ƻ�ȡ�ṹ���ڳ�Ա��ƫ��
std::map<std::wstring, int> GetStruct(std::wstring name);

bool InitOffsetAll(Offset* offset);
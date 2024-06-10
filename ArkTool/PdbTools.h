#pragma once
#include <map>
#include <string>

// 根据结构体名称获取结构体内成员的偏移
std::map<std::wstring, int> GetStruct(std::wstring name);

bool InitOffsetAll(Offset* offset);
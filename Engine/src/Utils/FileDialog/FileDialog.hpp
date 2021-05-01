#pragma once

class FileDialog
{
public:
	static std::string OpenFile(const char* filter);
	static std::string SaveFile(const char* filter);
};

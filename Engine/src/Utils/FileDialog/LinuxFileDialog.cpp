#ifdef __linux__
#include "Utils/FileDialog/FileDialog.hpp"
#include <filesystem>

std::string FileDialog::OpenFile(const char* filter)
{

	char filename[1024];
	std::string cmd = "zenity --file-selection --filename=\"" + std::filesystem::current_path().generic_string() + "/\"";
	FILE* f = popen(cmd.c_str(), "r");
	fgets(filename, 1024, f);

	int result = pclose(f);
	if(result < 0)
		LOG_ERROR("Error in FileDialog::OpenFile");

	std::string path(filename);
	path.pop_back(); // remove the \n character at the end
	return path;
}
std::string FileDialog::SaveFile(const char* filter)
{
	char filename[1024];
	FILE* f = popen("zenity --file-selection  --save" , "r");
	fgets(filename, 1024, f);

	int result = pclose(f);
	if(result < 0)
		LOG_ERROR("Error in FileDialog::SaveFile");

	std::string path(filename);
	path.pop_back(); // remove the \n character at the end
	return path;
}

#endif

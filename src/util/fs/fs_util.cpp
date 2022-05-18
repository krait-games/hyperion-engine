#include "fs_util.h"
#include <util/defines.h>

#include <filesystem>
#include <fstream>

#include <sys/stat.h>

#if HYP_WINDOWS
    #include <direct.h>
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN 1
    #endif
    #include <windows.h>
    #undef WIN32_LEAN_AND_MEAN
#endif

namespace hyperion::v2 {

bool FileSystem::DirExists(const std::string &path)
{
    struct stat st;
    
    if (stat(path.c_str(), &st) == 0) {
        return st.st_mode & S_IFDIR;
    }

    return false;
}

int FileSystem::Mkdir(const std::string &path)
{
#if HYP_WINDOWS
    return ::_mkdir(path.c_str());
#else
    return ::mkdir(path.c_str(), 0755);
#endif
}

std::string FileSystem::CurrentPath()
{
    std::string dir;
    char *buffer;
#ifdef WIN32
    buffer = _getcwd(nullptr, 0);
#else
    buffer = getcwd(nullptr, MAXPATHLEN);
#endif

	if (buffer != nullptr) {
        dir = buffer;

        std::free(buffer);
	}

	return dir;
}

std::string FileSystem::RelativePath(const std::string &path, const std::string &base)
{
    return std::filesystem::proximate(path, base).string();
}

} // namespace hyperion::v2
#ifndef BLITZKRIEG_GAME_MAIN_H
#define BLITZKRIEG_GAME_MAIN_H

#include <string>
#include <vector>

namespace NPlatform
{
struct Arguments
{
	int argc;
	const char *const *argv;
};
}

namespace NGame
{
enum class EFullscreenMode
{
	windowed,
	fullscreen,
};

struct CommandLineOptions
{
	int screenWidth = 1024;
	int screenHeight = 768;
	int screenBpp = 16;
	int stencilBpp = 0;
	int frequency = 0;
	EFullscreenMode fullscreenMode = EFullscreenMode::windowed;
	// Which display a fullscreen window lands on: either an index (0 = primary,
	// matching GFX.Monitor.Index) or part of the display's name, because the
	// index order changes as monitors are plugged in.
	std::string monitor;
	bool useDxt = false;
	bool multiplayer = false;
	bool cycledLaunch = false;
	int guaranteeFps = -1;
	int autoSavePeriod = 0;
	std::string movieDirectory;
	std::string connectAddress;
	int hostPort = 0;
	bool passwordRequired = false;
	bool startupSmoke = false;
	bool referenceScene = false;
	std::string referenceScenePath;
	int referenceWidth = 0;
	int referenceHeight = 0;
	std::string password;
	std::string mapName;
	std::string bindName = "bind.cfg";
	std::string saveFile;
	std::string modName;
	std::string dataDirectory;
	std::string saveHistoryFile;
	std::string loadHistoryFile;
	std::string playerName;
	std::string roomName;
	bool showScriptErrors = false;
	bool cheats = false;
	bool oneSave = false;
	bool showHelp = false;
	bool parseError = false;
	std::string renderer = "sdl_gpu";
	std::vector<std::string> unknownArguments;
};

CommandLineOptions ParseCommandLine(const NPlatform::Arguments &arguments);
int CommandLineExitCode(const CommandLineOptions &options);
}

struct BkGameLaunchInfo
{
	NPlatform::Arguments arguments{};
	NGame::CommandLineOptions options{};
};

int RunGame(const BkGameLaunchInfo &launch);
int GameMain(const NPlatform::Arguments &arguments);

#endif

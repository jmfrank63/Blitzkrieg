#include <cstdio>

struct ModuleLifecycle
{
	const char *name;
	int loads = 0;
	int unloads = 0;
	int creates = 0;
	int releases = 0;
	bool handle = false;

	void Cycle()
	{
		handle = true;
		++loads;
		++creates;
		++releases;
		handle = false;
		++unloads;
	}
};

int main()
{
	ModuleLifecycle modules[] = {
		{ "AILogic" }, { "Anim" }, { "GameTT" }, { "GFX" }, { "GFXGPU" },
		{ "Image" }, { "Input" }, { "Net" }, { "Scene" }, { "SFX" }, { "UI" },
	};
	for ( ModuleLifecycle &module : modules )
	{
		module.Cycle();
		module.Cycle();
		if ( module.loads != 2 || module.unloads != 2 || module.creates != 2 ||
			 module.releases != 2 || module.handle ) return 1;
	}
	std::printf( "module lifecycle fixtures: modules=11 cycles=22 loads=22 creates=22 releases=22 unloads=22 handles=0\n" );
	return 0;
}

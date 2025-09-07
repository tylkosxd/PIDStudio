#pragma once
#include "../SupportedGame.h"

class Gruntz : public SupportedGame
{
public:
	Gruntz(PIDStudio* app, const char* name, const char* iniKey, const char* exeName)
		: SupportedGame(app, name, iniKey, exeName) {}
};

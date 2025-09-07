#pragma once
#include "../SupportedGame.h"

class GetMedieval : public SupportedGame
{
public:
	GetMedieval(PIDStudio* app, const char* name, const char* iniKey, const char* exeName)
		: SupportedGame(app, name, iniKey, exeName) {}
};

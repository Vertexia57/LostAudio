#pragma once

#include <chrono>
#include <string>

namespace lost
{
	// Recalculates the delta time, using the previous time this function was ran as the "OldMillis"
	void _recalcDeltaTime();

	// Gets the time it took for the LAST frame to finish processing
	double _getDeltaTime();
}
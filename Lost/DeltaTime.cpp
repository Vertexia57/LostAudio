#include "DeltaTime.h"
#include <map>
#include "Log.h"

namespace lost
{
	std::chrono::milliseconds currentMillis = static_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch().count());
	std::chrono::milliseconds oldMillis = static_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch().count());
	double deltaTime = 0.0;

	int frames = 0;

	int fps = 0;
	int oldfps = 0;

	int averagefps = 0;

	double timeSinceLastFPSCount = 0.0;
	const float timeToUpdateFPS = 0.1f;

	void _recalcDeltaTime()
	{
		currentMillis = static_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch().count());
		deltaTime = (currentMillis - oldMillis).count() / 10000.0f;
		oldMillis = currentMillis;

		frames++;
		timeSinceLastFPSCount += deltaTime / 1000.0f;
		if (timeSinceLastFPSCount >= timeToUpdateFPS)
		{
			oldfps = fps;
			fps = round((float)frames / timeSinceLastFPSCount);
			averagefps = round((float)(fps + oldfps) / 2.0f);

			timeSinceLastFPSCount = 0.0;
			frames = 0;
		}
	}

	double _getDeltaTime()
	{
		return deltaTime;
	}
}
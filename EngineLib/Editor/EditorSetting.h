#pragma once

#include "Core/Core.h"
#include <fstream>

struct FEditorSetting
{
	float CameraSensitivity = 0.1f;
	float GridSpacing = 1.0f;

	void Load(const std::string& FilePath = "Config/editor.ini")
	{
		std::ifstream File(FilePath);

		if (!File.is_open())
		{
			throw std::runtime_error("Load failed");
		}

		std::string Key;

		while (File >> Key)
		{
			if (Key == "CameraSensitivity")
			{
				File >> CameraSensitivity;
			}
			if (Key == "GridSpacing")
			{
				File >> GridSpacing;
			}
		}
		
	}

	void Save(const std::string& FilePath = "Config/editor.ini") const
	{
		std::ofstream File(FilePath);

		if (!File.is_open())
		{
			throw std::runtime_error("Save failed");
		}

		File << "CameraSensitivity " << CameraSensitivity << '\n';
		File << "GridSpacing " << GridSpacing << '\n';
	}
};

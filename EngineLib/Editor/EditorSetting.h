#pragma once

#include "Core/Core.h"
#include <fstream>

struct FEditorSetting
{
	float CameraSensitivity = 0.1f;
	float GridSpacing = 1.0f;
	FVector3 CameraLocation = FVector3(0, 0, 0);
	FRotator CameraRotation = FRotator(0,0,0);
	float CameraFOV = 15.0f;
	float CameraNearClip = 1.0f;
	float CameraFarClip = 1000.0f;
	float RatioV = 0.5f;
	float RatioH = 0.5f;

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

			if (Key == "Location")
			{
				File >> CameraLocation.x;
				File >> CameraLocation.y;
				File >> CameraLocation.z;
			}

			if (Key == "Rotation")
			{
				File >> CameraRotation.Roll;
				File >> CameraRotation.Pitch;
				File >> CameraRotation.Yaw;
			}

			if (Key == "FOV")
			{
				File >> CameraFOV;
			}

			if (Key == "NearClip")
			{
				File >> CameraNearClip;
			}

			if (Key == "FarClip")
			{
				File >> CameraFarClip;
			}

			if (Key == "SplitterRatioV") {
				File >> RatioV;
			}


			if (Key == "SplitterRatioH") {
				File >> RatioH;
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
		File << "Location " << CameraLocation.x << " " << CameraLocation.y << " " << CameraLocation.z << '\n';
		File << "Rotation " << CameraRotation.Roll << " " << CameraRotation.Pitch << " " << CameraRotation.Yaw << '\n';
		File << "FOV " << CameraFOV << '\n';
		File << "NearClip " << CameraNearClip << '\n';
		File << "FarClip " << CameraFarClip << '\n';
		File << "SplitterRatioV " << RatioV << '\n';
		File << "SplitterRatioH " << RatioH << '\n';

	}
};

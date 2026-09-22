#pragma once

#include <vector>
#include <format>
#include <mutex>

#include "Core/Core.h"
#include "Core/Container/TArray.h"

enum class ELogLevel { Log, Warning, Error, Fatal };
enum class ELogCategory { Core, Editor, Render, Physics, Etc };

struct ImFont;

struct FConsoleMessage
{
	FString Time;
	ELogLevel Level = ELogLevel::Log;
	ELogCategory Category = ELogCategory::Etc;
	FString Text;
};

#define UE_LOG(Level, Category, fmt, ...)                                   \
ConsoleWindow::Get().AddLogPrintf(                                  \
	ELogLevel::Level,                                                       \
	ELogCategory::Category,                                                 \
	fmt, ##__VA_ARGS__)


#define UE_LOG_F(Level, Category, fmt, ...)                                 \
ConsoleWindow::Get().AddLogFormat(                                  \
	ELogLevel::Level,                                                       \
	ELogCategory::Category,                                                 \
	fmt, ##__VA_ARGS__)


class ConsoleWindow
{
public:
	ConsoleWindow();

	// Singleton pattern
	ConsoleWindow(const ConsoleWindow&) = delete;
	ConsoleWindow& operator=(const ConsoleWindow&) = delete;
	ConsoleWindow(ConsoleWindow&&) = delete;
	ConsoleWindow& operator=(ConsoleWindow&&) = delete;

	static ConsoleWindow& Get();

	void Init(std::string_view title, int maxLines);

	template<typename... Args>
	void AddLogFormat(ELogLevel Level, ELogCategory Category, std::string_view fmt, Args&&... args)
	{
		AddLog(Level, Category, std::vformat(fmt, std::make_format_args(args...)));
	}

	template<typename... Args>
	void AddLogPrintf(ELogLevel Level, ELogCategory Category, const char* fmt, Args&&... args)
	{
		char buffer[512];
		snprintf(buffer, sizeof(buffer), fmt, std::forward<Args>(args)...);
		AddLog(Level, Category, buffer);
	}

	void Clear();
	void Draw();


	bool bMemoryStat = false;
	bool bFpsStat = false;
private:
	FString mTitle;

	bool mbAutoScroll = true;

	void AddLog(ELogLevel Level, ELogCategory Category, std::string_view Text);

	void PushHistory(FConsoleMessage Message);
	void FlushPending();

	const FConsoleMessage& GetMessage(size_t Index) const;
	void ExecuteCommand(const char* Input);

	size_t mCapacity = 1000;
	size_t mFront = 0;
	size_t mCount = 0;

	TArray<FConsoleMessage> mPendingBuffers[2];
	TArray<FConsoleMessage> mMessages;  // 최종 로그
	char mInputBuffer[256] = {};

	size_t mWriteBufferIndex = 0;
	size_t mReadBufferIndex = 1;

	std::mutex mPendingMutex;

	ImFont* mFont = nullptr;
};

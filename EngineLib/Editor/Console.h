#pragma once

#include <vector>
#include <format>
#include <mutex>
#include <string>
#include "ThirdParty/ImGui/imgui.h"

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
	FString SourceFile;
	int SourceLine = 0;
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


// WEEK3 convenience macros, with an explicit WEEK4 category. DEBUG records source location.
#define UE_LOG_WARN(Category, ...) \
    ConsoleWindow::Get().AddLogPrintf(ELogLevel::Warning, ELogCategory::Category, __VA_ARGS__)
#define UE_LOG_WARN_F(Category, ...) \
    ConsoleWindow::Get().AddLogFormat(ELogLevel::Warning, ELogCategory::Category, __VA_ARGS__)
#define UE_LOG_ERROR(Category, ...) \
    ConsoleWindow::Get().AddLogPrintf(ELogLevel::Error, ELogCategory::Category, __VA_ARGS__)
#define UE_LOG_ERROR_F(Category, ...) \
    ConsoleWindow::Get().AddLogFormat(ELogLevel::Error, ELogCategory::Category, __VA_ARGS__)
#define UE_DEBUG_LOG(Category, ...) \
    ConsoleWindow::Get().AddLogPrintfAt(ELogLevel::Log, ELogCategory::Category, __FILE__, __LINE__, __VA_ARGS__)
#define UE_DEBUG_LOG_F(Category, ...) \
    ConsoleWindow::Get().AddLogFormatAt(ELogLevel::Log, ELogCategory::Category, __FILE__, __LINE__, __VA_ARGS__)
#define UE_DEBUG_LOG_WARN(Category, ...) \
    ConsoleWindow::Get().AddLogPrintfAt(ELogLevel::Warning, ELogCategory::Category, __FILE__, __LINE__, __VA_ARGS__)
#define UE_DEBUG_LOG_WARN_F(Category, ...) \
    ConsoleWindow::Get().AddLogFormatAt(ELogLevel::Warning, ELogCategory::Category, __FILE__, __LINE__, __VA_ARGS__)
#define UE_DEBUG_LOG_ERROR(Category, ...) \
    ConsoleWindow::Get().AddLogPrintfAt(ELogLevel::Error, ELogCategory::Category, __FILE__, __LINE__, __VA_ARGS__)
#define UE_DEBUG_LOG_ERROR_F(Category, ...) \
    ConsoleWindow::Get().AddLogFormatAt(ELogLevel::Error, ELogCategory::Category, __FILE__, __LINE__, __VA_ARGS__)

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
		AddLogFormatAt(Level, Category, nullptr, 0, fmt, std::forward<Args>(args)...);
	}

	template<typename... Args>
	void AddLogFormatAt(ELogLevel Level, ELogCategory Category, const char* File, int Line,
		std::string_view fmt, Args&&... args)
	{
		AddLog(Level, Category, std::vformat(fmt, std::make_format_args(args...)), File, Line);
	}

	template<typename... Args>
	void AddLogPrintf(ELogLevel Level, ELogCategory Category, const char* fmt, Args&&... args)
	{
		AddLogPrintfAt(Level, Category, nullptr, 0, fmt, std::forward<Args>(args)...);
	}

	template<typename... Args>
	void AddLogPrintfAt(ELogLevel Level, ELogCategory Category, const char* File, int Line,
		const char* fmt, Args&&... args)
	{
		char buffer[512]{};
		snprintf(buffer, sizeof(buffer), fmt, std::forward<Args>(args)...);
		AddLog(Level, Category, buffer, File, Line);
	}

	void Clear();
	void Draw();


private:
	FString mTitle;

	bool mbAutoScroll = true;
	bool mbScrollToBottom = false;
	bool mbShowLog = true;
	bool mbShowWarning = true;
	bool mbShowError = true;
	bool mbShowFatal = true;
	ImGuiTextFilter mFilter;
	std::vector<std::string> mCommandHistory;
	int mHistoryPosition = -1;
	std::string mHistoryDraft;
	static constexpr size_t MaxCommandHistory = 256;

	void AddLog(ELogLevel Level, ELogCategory Category, std::string_view Text,
		const char* File = nullptr, int Line = 0);

	void PushMessage(FConsoleMessage Message);
	void FlushPending();

	const FConsoleMessage& GetMessage(size_t Index) const;
	void ExecuteCommand(const char* Input);
	static int TextEditCallbackStub(ImGuiInputTextCallbackData* Data);
	int TextEditCallback(ImGuiInputTextCallbackData* Data);

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
